// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "master_impl.h"

#include <cmath>
#include <vector>
#include <queue>
#include <boost/lexical_cast.hpp>
#include "proto/agent.pb.h"
#include "rpc/rpc_client.h"
#include "common/timer.h"
#include <gflags/gflags.h>

DECLARE_int32(task_deploy_timeout);
DECLARE_int32(agent_keepalive_timeout);
DECLARE_string(master_checkpoint_path);
DECLARE_int32(master_max_len_sched_task_list);
DECLARE_int32(master_safe_mode_last);

namespace galaxy {
//agent load id index
typedef boost::multi_index::nth_index<AgentLoadIndex,0>::type agent_id_index;
//agent load cpu left index
typedef boost::multi_index::nth_index<AgentLoadIndex,1>::type cpu_left_index;


MasterImpl::MasterImpl()
    : next_agent_id_(1),
      next_job_id_(1),
      next_task_id_(1),
      rpc_client_(NULL),
      is_safe_mode_(false),
      start_time_(0),
      persistence_handler_(NULL) {
    rpc_client_ = new RpcClient();
    thread_pool_.AddTask(boost::bind(&MasterImpl::Schedule, this));
    thread_pool_.AddTask(boost::bind(&MasterImpl::DeadCheck, this));
}

bool MasterImpl::Recover() {
    std::string checkpoint_path = FLAGS_master_checkpoint_path;
    common::MutexLock lock(&agent_lock_);
    // clear jobs 
    jobs_.clear();

    if (persistence_handler_ == NULL) {
        // open leveldb  TODO do some config
        leveldb::Options options;
        options.create_if_missing = true;
        leveldb::Status status = 
            leveldb::DB::Open(options, 
                    checkpoint_path, &persistence_handler_);
        if (!status.ok()) {
            LOG(FATAL, "open checkpoint %s failed res[%s]",
                    checkpoint_path.c_str(), status.ToString().c_str()); 
            return false;
        }
    }

    // scan JobCheckPointCell 
    // TODO JobInfo is equal to JobCheckpointCell, use pb later

    // TODO do some config options
    leveldb::Iterator*  it = 
        persistence_handler_->NewIterator(leveldb::ReadOptions());
    it->SeekToFirst();
    int64_t max_job_id = 0;
    while (it->Valid()) {
        std::string job_key = it->key().ToString();
        std::string job_cell = it->value().ToString();
        int64_t job_id = atol(job_key.c_str());
        if (job_id == 0) {
            LOG(WARNING, "job id invalid %s", job_key.c_str()); 
            return false;
        }
        JobCheckPointCell cell;
        if (!cell.ParseFromString(job_cell)) {
            LOG(WARNING, "job cell invalid %s", job_key.c_str());
            return false;
        }
        if (max_job_id < job_id) {
            max_job_id = job_id; 
        }
        JobInfo& job_info = jobs_[job_id];
        job_info.id = cell.job_id();
        job_info.replica_num = cell.replica_num();
        job_info.job_name = cell.job_name();
        job_info.job_raw = cell.job_raw();
        job_info.cmd_line = cell.cmd_line();
        job_info.cpu_share = cell.cpu_share();
        job_info.mem_share = cell.mem_share();
        job_info.deploy_step_size = cell.deploy_step_size();
        job_info.cpu_limit = cell.cpu_share();
        if (cell.has_cpu_limit() 
                && cell.cpu_limit() > job_info.cpu_share) {
            job_info.cpu_limit = cell.cpu_limit();
        }
        if (job_info.deploy_step_size == 0) {
            job_info.deploy_step_size = job_info.replica_num;
        }
        job_info.killed = cell.killed();
        
        job_info.running_num = 0;
        job_info.scale_down_time = 0;

        LOG(INFO, "recover job info %s cpu_share: %lf cpu_limit: %lf mem_share: %ld deploy_step_size: %d", 
                job_info.job_name.c_str(),
                job_info.cpu_share,
                job_info.cpu_limit,
                job_info.mem_share,
                job_info.deploy_step_size);
        // only safe_mode when recovered, 
        is_safe_mode_ = true;
        it->Next();
    }
    delete it;
    
    next_job_id_ = max_job_id + 1;
    start_time_ = common::timer::now_time();
    LOG(INFO, "master recover success");
    return true;
}

MasterImpl::~MasterImpl() {
    delete rpc_client_;
}

void MasterImpl::TerminateTask(::google::protobuf::RpcController* /*controller*/,
                 const ::galaxy::TerminateTaskRequest* request,
                 ::galaxy::TerminateTaskResponse* response,
                 ::google::protobuf::Closure* done) {
    if (SafeModeCheck()) {
        response->set_status(kMasterResponseErrorSafeMode);
        LOG(WARNING, "can't terminate task in safe mode");
        done->Run(); 
        return;
    }
    if (!request->has_task_id()) {
        response->set_status(kMasterResponseErrorInput);
        done->Run();
        return;
    }
    int64_t task_id = request->task_id();

    common::MutexLock lock(&agent_lock_);
    std::map<int64_t, TaskInstance>::iterator it;
    it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        response->set_status(kMasterResponseErrorInternal);
        done->Run();
        return;
    }

    std::string agent_addr = it->second.agent_addr();
    AgentInfo& agent = agents_[agent_addr];
    CancelTaskOnAgent(&agent, task_id);
    done->Run();
}

void MasterImpl::ListNode(::google::protobuf::RpcController* /*controller*/,
                          const ::galaxy::ListNodeRequest* /*request*/,
                          ::galaxy::ListNodeResponse* response,
                          ::google::protobuf::Closure* done) {
    MutexLock lock(&agent_lock_);
    std::map<std::string, AgentInfo>::iterator it = agents_.begin();
    for (; it != agents_.end(); ++it) {
        AgentInfo& agent = it->second;
        NodeInstance* node = response->add_nodes();
        node->set_node_id(agent.id);
        node->set_addr(agent.addr);
        node->set_task_num(agent.running_tasks.size());
        node->set_cpu_share(agent.cpu_share);
        node->set_mem_share(agent.mem_share);
        node->set_cpu_allocated(agent.cpu_allocated);
        node->set_mem_allocated(agent.mem_allocated);
        node->set_cpu_used(agent.cpu_used);
        node->set_mem_used(agent.mem_used);
    }
    done->Run();
}

void MasterImpl::ListTaskForAgent(const std::string& agent_addr,
            ::google::protobuf::RepeatedPtrField<TaskInstance >* tasks){
    common::MutexLock lock(&agent_lock_);
    std::map<std::string, AgentInfo>::iterator agent_it = agents_.find(agent_addr);
    if(agent_it != agents_.end()){
        AgentInfo& agent = agent_it->second;
        std::set<int64_t>::iterator task_set_it = agent.running_tasks.begin();
        for(;task_set_it != agent.running_tasks.end(); ++task_set_it){
                std::map<int64_t, TaskInstance>::iterator task_it
                    = tasks_.find(*task_set_it);
                if (task_it != tasks_.end()) {
                    TaskInstance* task = tasks->Add();
                    task->CopyFrom(task_it->second);
                }

        }
    }

}
void MasterImpl::ListTaskForJob(int64_t job_id,
        ::google::protobuf::RepeatedPtrField<TaskInstance >* tasks,
        ::google::protobuf::RepeatedPtrField<TaskInstance >* sched_tasks) {
    common::MutexLock lock(&agent_lock_);

    std::map<int64_t, JobInfo>::iterator job_it = jobs_.find(job_id);
    if (job_it != jobs_.end()) {
        JobInfo& job = job_it->second;
        std::map<std::string, std::set<int64_t> >::iterator agent_it
            = job.agent_tasks.begin();
        for (; agent_it != job.agent_tasks.end(); ++agent_it) {
            std::string agent_addr = agent_it->first;
            std::set<int64_t>& task_set = agent_it->second;
            std::set<int64_t>::iterator id_it = task_set.begin();
            for (; id_it != task_set.end(); ++id_it) {
                int64_t task_id = *id_it;
                std::map<int64_t, TaskInstance>::iterator task_it
                    = tasks_.find(task_id);
                if (task_it != tasks_.end()) {
                    TaskInstance* task = tasks->Add();
                    task->CopyFrom(task_it->second);
                }
            }
            LOG(DEBUG, "list tasks %u for job %ld", task_set.size(), job_id);
        }

        if (sched_tasks == NULL) {
            return; 
        }

        std::deque<TaskInstance>::iterator sched_it = job.scheduled_tasks.begin();
        LOG(DEBUG, "list schedule tasks %u for job %ld", job.scheduled_tasks.size(), job_id);
        for (; sched_it != job.scheduled_tasks.end(); ++sched_it) {
            TaskInstance* task = sched_tasks->Add(); 
            task->CopyFrom(*sched_it);
        }
    
    }
}

void MasterImpl::ListTask(::google::protobuf::RpcController* /*controller*/,
                 const ::galaxy::ListTaskRequest* request,
                 ::galaxy::ListTaskResponse* response,
                 ::google::protobuf::Closure* done) {
    std::map<int64_t, TaskInstance> tmp_task_pair;
    std::map<int64_t, TaskInstance>::iterator it;
    {
        // @TODO memory copy
        common::MutexLock lock(&agent_lock_);
        tmp_task_pair = tasks_;
    }
    if (request->has_task_id()) {
        // just list one
        it = tmp_task_pair.find(request->task_id());
        if (it != tmp_task_pair.end()) {
            response->add_tasks()->CopyFrom(it->second);
        }
    } else if (request->has_job_id()) {
        ListTaskForJob(request->job_id(), response->mutable_tasks(), response->mutable_scheduled_tasks());
    } else if(request->has_agent_addr()){
        ListTaskForAgent(request->agent_addr(),response->mutable_tasks());
    }else {
        it = tmp_task_pair.begin();
        for (; it != tmp_task_pair.end(); ++it) {
            response->add_tasks()->CopyFrom(it->second);
        }
    }
    done->Run();
}


void MasterImpl::ListJob(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::ListJobRequest* /*request*/,
                         ::galaxy::ListJobResponse* response,
                         ::google::protobuf::Closure* done) {
    MutexLock lock(&agent_lock_);

    std::map<int64_t, JobInfo>::iterator it = jobs_.begin();
    for (; it !=jobs_.end(); ++it) {
        JobInfo& job = it->second;
        JobInstance* job_inst = response->add_jobs();
        job_inst->set_job_id(job.id);
        job_inst->set_job_name(job.job_name);
        job_inst->set_running_task_num(job.running_num);
        job_inst->set_replica_num(job.replica_num);
    }
    done->Run();
}

void MasterImpl::DeadCheck() {
    int32_t now_time = common::timer::now_time();

    MutexLock lock(&agent_lock_);
    std::map<int32_t, std::set<std::string> >::iterator it = alives_.begin();

    int idle_time = 5;

    while (it != alives_.end()
           && it->first + FLAGS_agent_keepalive_timeout <= now_time) {
        std::set<std::string>::iterator node = it->second.begin();
        while (node != it->second.end()) {
            AgentInfo& agent = agents_[*node];
            LOG(INFO, "[DeadCheck] Agent %s dead, %lu task fail",
                agent.addr.c_str(), agent.running_tasks.size());
            std::set<int64_t> running_tasks;
            UpdateJobsOnAgent(&agent, running_tasks, true);
            RemoveIndex(agent.id);
            agents_.erase(*node);
            it->second.erase(node);
            node = it->second.begin();
        }
        assert(it->second.empty());
        alives_.erase(it);
        it = alives_.begin();
    }
    if (it != alives_.end()) {
        idle_time = it->first + FLAGS_agent_keepalive_timeout - now_time;
        // LOG(INFO, "it->first= %d, now_time= %d\n", it->first, now_time);
        if (idle_time > 5) {
            idle_time = 5;
        }
    }
    thread_pool_.DelayTask(idle_time * 1000,
                           boost::bind(&MasterImpl::DeadCheck, this));
}

void MasterImpl::UpdateJobsOnAgent(AgentInfo* agent,
                                   const std::set<int64_t>& running_tasks,
                                   bool clear_all) {
    std::set<int64_t> internal_running_tasks = running_tasks;
    agent_lock_.AssertHeld();
    const std::string& agent_addr = agent->addr;
    assert(!agent_addr.empty());
    LOG(INFO,"update jobs on agent %s",agent_addr.c_str());
    int32_t now_time = common::timer::now_time();
    std::set<int64_t>::iterator it = agent->running_tasks.begin();
    std::vector<int64_t> del_tasks;
    // TODO agent->running_tasks and internal_running_tasks all sorted, 
    for (; it != agent->running_tasks.end(); ++it) {
        int64_t task_id = *it;
        if (internal_running_tasks.find(task_id) == internal_running_tasks.end()) { 
            TaskInstance& instance = tasks_[task_id];
            int64_t job_id = instance.job_id();
            if (instance.status() == DEPLOYING
                    && instance.start_time() + FLAGS_task_deploy_timeout > now_time
                        && !clear_all) {
                LOG(INFO, "Wait for deploy timeout %ld", task_id);
                continue;
            }
            del_tasks.push_back(task_id);
            assert(jobs_.find(job_id) != jobs_.end());
            JobInfo& job = jobs_[job_id]; 
            job.agent_tasks[agent_addr].erase(task_id);
            if (job.agent_tasks[agent_addr].empty()) {
                job.agent_tasks.erase(agent_addr);
            }
            job.running_num --;
            if (instance.status() == COMPLETE) { 
                job.complete_tasks[agent_addr].insert(task_id);
                job.replica_num --;
            }
            tasks_[task_id].set_end_time(common::timer::now_time());
            if ((int)job.scheduled_tasks.size() >= FLAGS_master_max_len_sched_task_list) {
                job.scheduled_tasks.pop_front(); 
            }
            std::set<int64_t>::iterator deploying_tasks_it = job.deploying_tasks.find(task_id);
            if (deploying_tasks_it != job.deploying_tasks.end()) {
                job.deploying_tasks.erase(deploying_tasks_it);
            }

            job.scheduled_tasks.push_back(tasks_[task_id]);  
            LOG(DEBUG, "job %ld has schedule tasks %u : id %ld state %d ", 
                    job_id,
                    job.scheduled_tasks.size(),
                    task_id,
                    instance.status());
            tasks_.erase(task_id);
            LOG(INFO, "Job[%s] task %ld disappear from %s",
                job.job_name.c_str(), task_id, agent_addr.c_str());
        } else {
            TaskInstance& instance = tasks_[task_id];
            internal_running_tasks.erase(task_id);

            int64_t job_id = instance.job_id();
            std::map<int64_t,JobInfo>::iterator job_it = jobs_.find(job_id);
            assert(job_it != jobs_.end());
            JobInfo& job = job_it->second;
            if (instance.status() != DEPLOYING) {
                std::set<int64_t>::iterator deploying_tasks_it = job.deploying_tasks.find(task_id);
                if(deploying_tasks_it != job.deploying_tasks.end()){
                    job.deploying_tasks.erase(deploying_tasks_it);
                }
            }
            if (instance.status() != ERROR
               && instance.status() != COMPLETE) {
                continue;
            }
            //释放资源
            LOG(INFO,"delay cancel task %d on agent %s",task_id,agent_addr.c_str());
            thread_pool_.DelayTask(100, boost::bind(&MasterImpl::DelayRemoveZombieTaskOnAgent,this, agent, task_id));
        }
    }
    for (uint64_t i = 0UL; i < del_tasks.size(); ++i) {
        agent->running_tasks.erase(del_tasks[i]);
    }

    std::set<int64_t>::iterator rt_it = internal_running_tasks.begin();
    for (; rt_it != internal_running_tasks.end(); ++rt_it) {
        int64_t rt_task_id = *rt_it;
        LOG(WARNING, "task %ld not in master add killed", *rt_it);
        TaskInstance& rt_instance = tasks_[rt_task_id];
        // rebuild agent, task relationship
        agent->running_tasks.insert(rt_task_id);

        // rebuild job, task relationship
        if (jobs_.find(rt_instance.job_id()) == jobs_.end()) {
            JobInfo dirty_job_info;
            dirty_job_info.id = rt_instance.job_id();
            dirty_job_info.replica_num = 0;
            dirty_job_info.killed = true;
            dirty_job_info.job_name = "Out-of-date";
            dirty_job_info.running_num = 0;
            dirty_job_info.deploy_step_size = 1; 
            jobs_[rt_instance.job_id()] = dirty_job_info;
        }

        JobInfo& job_info = jobs_[rt_instance.job_id()];
        if (job_info.agent_tasks[agent_addr].find(rt_task_id) 
                == job_info.agent_tasks[agent_addr].end()) {
            job_info.running_num ++;
        }
        rt_instance.mutable_info()->set_task_name(job_info.job_name);
        rt_instance.mutable_info()->set_task_id(rt_task_id);
        rt_instance.mutable_info()->set_required_cpu(job_info.cpu_share);
        rt_instance.mutable_info()->set_required_mem(job_info.mem_share);
        // TODO rt_instance.mutable_info->set_offset()
        //      rt_instance.mutable_info->set_start_time()
        job_info.agent_tasks[agent_addr].insert(rt_task_id);
        //thread_pool_.DelayTask(100, boost::bind(&MasterImpl::DelayRemoveZombieTaskOnAgent,this, agent, *rt_it));
    }
}

void MasterImpl::HeartBeat(::google::protobuf::RpcController* /*controller*/,
                           const ::galaxy::HeartBeatRequest* request,
                           ::galaxy::HeartBeatResponse* response,
                           ::google::protobuf::Closure* done) {
    const std::string& agent_addr = request->agent_addr();
    LOG(DEBUG, "HeartBeat from %s task_status_size  %d ", agent_addr.c_str(),request->task_status_size());
    int now_time = common::timer::now_time();
    MutexLock lock(&agent_lock_);
    std::map<std::string, AgentInfo>::iterator it;
    it = agents_.find(agent_addr);
    AgentInfo* agent = NULL;
    if (it == agents_.end()) {
        LOG(INFO, "New Agent register %s", agent_addr.c_str());
        alives_[now_time].insert(agent_addr);
        agent = &agents_[agent_addr];
        agent->addr = agent_addr;
        agent->id = next_agent_id_ ++;
        agent->cpu_share = request->cpu_share();
        agent->mem_share = request->mem_share();
        agent->cpu_allocated = request->used_cpu_share();
        agent->mem_allocated = request->used_mem_share();
        agent->stub = NULL;
        agent->version = 1;
        agent->alive_timestamp = now_time;
    } else {
        agent = &(it->second);
        int32_t es = alives_[agent->alive_timestamp].erase(agent_addr);
        if (alives_[agent->alive_timestamp].empty()) {
            alives_.erase(agent->alive_timestamp);
        }
        assert(es);
        alives_[now_time].insert(agent_addr);
        agent->alive_timestamp = now_time;
        if(request->version() < agent->version){
            LOG(WARNING,"mismatch agent version expect %d but %d ,heart beat message is discard", agent->version, request->version());
            response->set_agent_id(agent->id);
            response->set_version(agent->version);
            done->Run();
            return;
        }
        agent->cpu_allocated = request->used_cpu_share();
        agent->mem_allocated = request->used_mem_share();
        LOG(DEBUG, "cpu_allocated:%lf, mem_allocated:%ld", 
                   agent->cpu_allocated, 
                   agent->mem_allocated);
    }
    response->set_agent_id(agent->id);
    response->set_version(agent->version);
    //@TODO maybe copy out of lock
    int task_num = request->task_status_size();
    std::set<int64_t> running_tasks;
    for (int i = 0; i < task_num; i++) {
        assert(request->task_status(i).has_task_id());
        int64_t task_id = request->task_status(i).task_id();
        TaskInstance& instance = tasks_[task_id];
        running_tasks.insert(task_id);
        int task_status = request->task_status(i).status();
        instance.set_status(task_status);
        instance.set_job_id(request->task_status(i).job_id());
        LOG(DEBUG, "Task %d status: %s",
            task_id, TaskState_Name((TaskState)task_status).c_str());
        instance.set_agent_addr(agent_addr);
        LOG(DEBUG, "%s run task %d %d", agent_addr.c_str(),
            task_id, request->task_status(i).status());
        if (request->task_status(i).has_cpu_usage()) {
            instance.set_cpu_usage(
                    request->task_status(i).cpu_usage());
            LOG(DEBUG, "%d use cpu %f", task_id, instance.cpu_usage());
        }
        if (request->task_status(i).has_memory_usage()) {
            instance.set_memory_usage(
                    request->task_status(i).memory_usage());
            LOG(DEBUG, "%d use memory %ld", task_id, instance.memory_usage());
        }
    }
    UpdateJobsOnAgent(agent, running_tasks);
    std::set<int64_t>::iterator rt_it = agent->running_tasks.begin();
    agent->mem_used = 0 ;
    agent->cpu_used = 0.0 ;
    for (;rt_it != agent->running_tasks.end();++rt_it) {
        std::map<int64_t, TaskInstance>::iterator inner_it =  
            tasks_.find(*rt_it);
        if(inner_it == tasks_.end()){
            continue;
        }
        agent->mem_used += inner_it->second.memory_usage();
        //
        agent->cpu_used += inner_it->second.cpu_usage() * inner_it->second.info().required_cpu();
    }
    SaveIndex(*agent);
    done->Run();
}

void MasterImpl::DelayRemoveZombieTaskOnAgent(AgentInfo * agent,int64_t task_id){
    MutexLock lock(&agent_lock_);

    CancelTaskOnAgent(agent,task_id);
}


void MasterImpl::KillJob(::google::protobuf::RpcController* /*controller*/,
                   const ::galaxy::KillJobRequest* request,
                   ::galaxy::KillJobResponse* /*response*/,
                   ::google::protobuf::Closure* done) {
    if (SafeModeCheck()) { 
        LOG(WARNING, "can't kill job in safe mode"); 
        done->Run();
        return;
    }
    MutexLock lock(&agent_lock_);
    int64_t job_id = request->job_id();
    std::map<int64_t, JobInfo>::iterator it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        done->Run();
        return;
    }

    JobInfo& job = it->second;
    int64_t old_replica_num = job.replica_num;
    job.replica_num = 0;
    job.killed = true;
    // Not Delete here, delete by recover
    if (!PersistenceJobInfo(job)) {
        LOG(WARNING, "kill job failed for persistence"); 
        job.replica_num = old_replica_num;
        job.killed = false;
        done->Run();
        return;
    }
    
    done->Run();
}

void MasterImpl::UpdateJob(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::UpdateJobRequest* request,
                         ::galaxy::UpdateJobResponse* response,
                         ::google::protobuf::Closure* done) {
    if (SafeModeCheck()) {
        LOG(WARNING, "can't update job in safe mode"); 
        response->set_status(kMasterResponseErrorSafeMode);
        done->Run();
        return;
    }
    MutexLock lock(&agent_lock_);
    int64_t job_id = request->job_id();
    std::map<int64_t, JobInfo>::iterator it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        response->set_status(kMasterResponseErrorInternal);
        done->Run();
        return;
    }

    JobInfo& job = it->second;
    int64_t old_replica_num = job.replica_num;
    job.replica_num = request->replica_num();

    if (!PersistenceJobInfo(job)) {
        // roll back 
        job.replica_num = old_replica_num;
        response->set_status(kMasterResponseErrorInternal);
    } else {
        response->set_status(kMasterResponseOK); 
    }
    done->Run();
}

void MasterImpl::NewJob(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::NewJobRequest* request,
                         ::galaxy::NewJobResponse* response,
                         ::google::protobuf::Closure* done) {

    MutexLock lock(&agent_lock_);
    int64_t job_id = next_job_id_++;
    while (jobs_.find(job_id) != jobs_.end()) {
        job_id = next_job_id_ ++; 
    }
    JobInfo job;
    job.id = job_id;
    job.job_name = request->job_name();
    job.job_raw = request->job_raw();
    job.cmd_line = request->cmd_line();
    job.replica_num = request->replica_num();

    job.running_num = 0;
    job.scale_down_time = 0;
    job.killed = false;
    job.cpu_share = request->cpu_share();
    job.mem_share = request->mem_share();
    job.cpu_limit = job.cpu_share;
    if (request->has_cpu_limit() 
            && request->cpu_limit() > job.cpu_share) {
        job.cpu_limit = request->cpu_limit();
    }  

    if (request->deploy_step_size() > 0) {
        job.deploy_step_size = request->deploy_step_size();
    } else {
        job.deploy_step_size = job.replica_num;
    }
    LOG(DEBUG, "new job %s replica_num: %d cmd_line: %s cpu_share: %lf cpu_limit: %lf mem_share: %ld deloy_step_size: %d",
            job.job_name.c_str(),
            job.replica_num,
            job.cmd_line.c_str(),
            job.cpu_share,
            job.cpu_limit,
            job.mem_share,
            job.deploy_step_size);

    if (!PersistenceJobInfo(job)) {
        response->set_status(kMasterResponseErrorInternal); 
        done->Run();
        return;
    }

    jobs_[job_id] = job;
    response->set_status(kMasterResponseOK);
    response->set_job_id(job_id);
    done->Run();
}

bool MasterImpl::ScheduleTask(JobInfo* job, const std::string& agent_addr) {
    agent_lock_.AssertHeld();
    AgentInfo& agent = agents_[agent_addr];

    if (agent.stub == NULL) {
        bool ret = rpc_client_->GetStub(agent_addr, &agent.stub);
        assert(ret);
    }

    int64_t task_id = next_task_id_++;
    while(tasks_.find(task_id) != tasks_.end()){
        task_id = next_task_id_++;
    }
    RunTaskRequest rt_request;
    rt_request.set_task_id(task_id);
    rt_request.set_task_name(job->job_name);
    rt_request.set_task_raw(job->job_raw);
    rt_request.set_cmd_line(job->cmd_line);
    rt_request.set_cpu_share(job->cpu_share);
    rt_request.set_mem_share(job->mem_share);
    rt_request.set_task_offset(job->running_num);
    rt_request.set_job_replicate_num(job->replica_num);
    rt_request.set_job_id(job->id);
    rt_request.set_cpu_limit(job->cpu_limit);
    RunTaskResponse rt_response;
    LOG(INFO, "ScheduleTask on %s", agent_addr.c_str());
    bool ret = rpc_client_->SendRequest(agent.stub, &Agent_Stub::RunTask,
                                        &rt_request, &rt_response, 5, 1);
    if (!ret || (rt_response.has_status() 
              && rt_response.status() != 0)) {
        LOG(WARNING, "RunTask faild agent= %s", agent_addr.c_str());
        return false;
    } else {
        agent.running_tasks.insert(task_id);
        TaskInstance& instance = tasks_[task_id];
        instance.mutable_info()->set_task_name(job->job_name);
        instance.mutable_info()->set_task_id(task_id);
        instance.mutable_info()->set_required_cpu(job->cpu_share);
        instance.mutable_info()->set_required_mem(job->mem_share);
        instance.mutable_info()->set_limited_cpu(job->cpu_limit);
        instance.set_agent_addr(agent_addr);
        instance.set_job_id(job->id);
        instance.set_start_time(common::timer::now_time());
        instance.set_status(DEPLOYING);
        instance.set_offset(job->running_num);
        job->agent_tasks[agent_addr].insert(task_id);
        job->running_num ++;
        job->deploying_tasks.insert(task_id);
        return true;
    }
}

bool MasterImpl::CancelTaskOnAgent(AgentInfo* agent, int64_t task_id) {
    agent_lock_.AssertHeld();
    LOG(INFO,"cancel task %ld on agent %s",task_id,agent->addr.c_str());
    if (agent->stub == NULL) {
    bool ret = rpc_client_->GetStub(agent->addr, &agent->stub);
        assert(ret);
    }
    KillTaskRequest kill_request;
    kill_request.set_task_id(task_id);
    KillTaskResponse kill_response;
    bool ret = rpc_client_->SendRequest(agent->stub, &Agent_Stub::KillTask,
                                        &kill_request, &kill_response, 5, 1);
    if (!ret || (kill_response.has_status() 
             && kill_response.status() != 0)) {
        LOG(WARNING, "Kill task %ld agent= %s",
            task_id, agent->addr.c_str());
        return false;
    } else {
        std::map<int64_t, TaskInstance>::iterator it;         
        it = tasks_.find(task_id);
        if (it != tasks_.end()) {
            it->second.set_agent_addr(agent->addr); 
            if (kill_response.has_gc_path()) {
                it->second.set_root_path(kill_response.gc_path());
            }
        }
    }
    return true;
}

void MasterImpl::ScaleDown(JobInfo* job) {
    agent_lock_.AssertHeld();
    std::string agent_addr;
    int64_t task_id = -1;
    std::set<int64_t>::size_type high_load = 0;
    std::map<std::string, std::set<int64_t> >::iterator it = job->agent_tasks.begin();
    if (job->running_num <= 0) {
        LOG(INFO, "[ScaleDown] %s[%d/%d] no need scale down",
                job->job_name.c_str(),
                job->running_num,
                job->replica_num);
        return;
    }
    for (; it != job->agent_tasks.end(); ++it) {
        assert(agents_.find(it->first) != agents_.end());
        assert(!it->second.empty());
        // 只考虑了agent的负载，没有考虑job在agent上分布的多少，需要一个更复杂的算法么?
        AgentInfo& ai = agents_[it->first];
        LOG(DEBUG, "[ScaleDown] %s[%s: %d] high_load %d",
                job->job_name.c_str(),
                it->first.c_str(),
                ai.running_tasks.size(),
                high_load);
        if (ai.running_tasks.size() > high_load) {
            high_load = ai.running_tasks.size();
            agent_addr = ai.addr;
            task_id = *(it->second.begin());
        }
    }
    assert(!agent_addr.empty() && task_id != -1);
    LOG(INFO, "[ScaleDown] %s[%d/%d] on %s will be canceled",
        job->job_name.c_str(), job->running_num, job->replica_num, agent_addr.c_str());
    AgentInfo& agent = agents_[agent_addr];
    bool success = CancelTaskOnAgent(&agent, task_id);
    if(success){
        LOG(INFO,"kill task %ld successfully",task_id);
        std::map<int64_t,TaskInstance>::iterator intance_it = tasks_.find(task_id);
        if(intance_it != tasks_.end()){
            intance_it->second.set_status(KILLED);
            agent.version += 1;
        }
    }else{
        LOG(INFO,"fail to kill task %ld",task_id);
    }
}

void MasterImpl::Schedule() {
    MutexLock lock(&agent_lock_);
    if (SafeModeCheck()) {
        LOG(WARNING, "no schedule in safe mode");
        thread_pool_.DelayTask(1000, boost::bind(&MasterImpl::Schedule, this));
        return;
    }
    int32_t now_time = common::timer::now_time();
    std::map<int64_t, JobInfo>::iterator job_it = jobs_.begin();
    std::vector<int64_t> should_rm_job;
    for (; job_it != jobs_.end(); ++job_it) {
        JobInfo& job = job_it->second;
        LOG(INFO,"job %ld ,running_num %ld",job.id,job.running_num);
        if (job.running_num == 0 && job.killed) {
            should_rm_job.push_back(job_it->first);
            continue;
        }
        if (job.running_num > job.replica_num && job.scale_down_time + 10 < now_time) {
            ScaleDown(&job);
            // 避免瞬间缩成0了
            job.scale_down_time = now_time;
        }
        int count_for_log = 0;
        //fix warning
        uint32_t deploy_step_size = job.deploy_step_size;
        //just for log
        uint32_t old_deploying_tasks_size = job.deploying_tasks.size();
        int max_deploying_try_times = deploy_step_size - old_deploying_tasks_size;
        for (int deploying_times = 0;
                job.deploying_tasks.size() < deploy_step_size
                    && job.running_num < job.replica_num 
                    && deploying_times < max_deploying_try_times;
                        ++deploying_times) {
            LOG(INFO, "[Schedule] Job[%s] running %d tasks, replica_num %d",
                job.job_name.c_str(), job.running_num, job.replica_num);
            std::string agent_addr = AllocResource(job);
            if (agent_addr.empty()) {
                LOG(WARNING, "Allocate resource fail, delay schedule job %s",job.job_name.c_str());
                break;
            }
            bool ret = ScheduleTask(&job, agent_addr);
            if (ret) {
                //update index
                std::map<std::string,AgentInfo>::iterator agent_it = agents_.find(agent_addr);
                if (agent_it != agents_.end()) {
                    agent_it->second.mem_allocated += job.mem_share;
                    agent_it->second.cpu_allocated += job.cpu_share;
                    agent_it->second.version += 1;
                    SaveIndex(agent_it->second);
                }   
            }   
            count_for_log++;

        }
        LOG(INFO,"schedule job %ld ,the deploying size is %d,deployed count %d", job.id,
                 old_deploying_tasks_size, count_for_log);
    }
    for (uint32_t i = 0;i < should_rm_job.size();i++) {
        LOG(INFO,"remove job %ld",should_rm_job[i]);

        if (!DeletePersistenceJobInfo(jobs_[should_rm_job[i]])) {
            LOG(FATAL, "remove job %ld persistence failed", 
                    should_rm_job[i]);
        }
        jobs_.erase(should_rm_job[i]);
    }
    thread_pool_.DelayTask(1000, boost::bind(&MasterImpl::Schedule, this));
}

//负载计算
//目前使用3个因数
//1、当前机器mem使用量，负载与内存使用量成正比，与内存总量成反比
//2、当前机器的cpu使用量，负载与cpu使用量成正比，与cpu总量成反比
//3、当前机器上的任务数，负载与任务数成正比
// 由于各个维度的度量单位不同，所以通过资源的“占用比”来衡量；
// 各维度资源的综合评估通过指数相加的方式，这种方式比直接求和或求乘积更加平滑，
// 且能避免选出各维度资源消耗不平衡的机器。
// 参考资料：http://www.columbia.edu/~cs2035/courses/ieor4405.S13/datacenter_scheduling.ppt
double MasterImpl::CalcLoad(const AgentInfo& agent){
    if(agent.mem_share <= 0 || agent.cpu_share <= 0.0 ){
        LOG(FATAL,"invalid agent input ,mem_share %ld,cpu_share %f",agent.mem_share,agent.cpu_share);
        return 0.0;
    }
    const double tasks_count_base_line = 32.0;
    double mem_factor = agent.mem_allocated / static_cast<double>(agent.mem_share);
    double cpu_factor = agent.cpu_allocated / agent.cpu_share;
    double task_count_factor = agent.running_tasks.size() / tasks_count_base_line;
    return exp(cpu_factor) + exp(mem_factor) + exp(task_count_factor);
}

std::string MasterImpl::AllocResource(const JobInfo& job){
    LOG(INFO,"alloc resource for job %ld,mem_require %ld, cpu_require %f",
        job.id,job.mem_share,job.cpu_share);
    agent_lock_.AssertHeld();
    std::string addr;
    cpu_left_index& cidx = boost::multi_index::get<1>(index_);
    // NOTO first iterator value not satisfy requirement
    cpu_left_index::iterator it = cidx.lower_bound(job.cpu_share);
    cpu_left_index::iterator it_start = cidx.begin();
    cpu_left_index::iterator cur_agent;
    bool last_found = false;
    double current_min_load = 0;
    if(it != cidx.end() 
            && it->mem_left >= job.mem_share
            && it->cpu_left >= job.cpu_share){
        LOG(DEBUG, "alloc resource for job %ld list agent %s cpu left %lf mem left %ld",
                job.id,
                it->agent_addr.c_str(),
                it->cpu_left,
                it->mem_left);
        last_found = true;
        current_min_load = it->load;
        addr = it->agent_addr;
        cur_agent = it;
    }
    for(;it_start != it;++it_start){
        LOG(DEBUG, "alloc resource for job %ld list agent %s cpu left %lf mem left %ld",
                job.id,
                it_start->agent_addr.c_str(),
                it_start->cpu_left,
                it_start->mem_left);
        //判断内存是否满足需求
        if(it_start->mem_left < job.mem_share){
            continue;
        }
        //第一次赋值current_min_load;
        if(!last_found){
            current_min_load = it_start->load;
            addr = it_start->agent_addr;
            last_found = true;
            cur_agent = it;
            continue;
        }
        //找到负载更小的节点
        if(current_min_load > it_start->load){
            addr = it_start->agent_addr;
            current_min_load = it_start->load;
            cur_agent = it;
        }
    }
    if(last_found){
        LOG(INFO,"alloc resource for job %ld on host %s with load %f cpu left %f mem left %ld",
                job.id,addr.c_str(),
                current_min_load,
                cur_agent->cpu_left,
                cur_agent->mem_left);
    }else{
        LOG(WARNING,"no enough  resource to alloc for job %ld",job.id);
    }
    return addr;
}


void MasterImpl::SaveIndex(const AgentInfo& agent){
    agent_lock_.AssertHeld();
    double load_factor = CalcLoad(agent);
    AgentLoad load(agent,load_factor);
    LOG(INFO,"save agent[%s] %ld load index load %f,mem_left %ld,cpu_left %f ",
              load.agent_addr.c_str(),
              load.agent_id,
              load_factor,
              load.mem_left,
              load.cpu_left);
    agent_id_index& aidx = boost::multi_index::get<0>(index_);
    agent_id_index::iterator it = aidx.find(load.agent_id);
    if(it != aidx.end()){
        aidx.modify(it,load);
    }else{
        aidx.insert(load);
    }
}

void MasterImpl::RemoveIndex(int64_t agent_id){
    agent_lock_.AssertHeld();
    LOG(INFO,"remove agent %ld load index ",agent_id);
    agent_id_index& aidx = boost::multi_index::get<0>(index_);
    agent_id_index::iterator it = aidx.find(agent_id);
    if(it != aidx.end()){
        aidx.erase(it);
    }
}

bool MasterImpl::DeletePersistenceJobInfo(const JobInfo& job_info) {
    if (persistence_handler_ == NULL) {
        LOG(WARNING, "persistence handler not inited yet");
        return false;
    }    
    std::string key = boost::lexical_cast<std::string>(job_info.id);

    leveldb::Status status = 
        persistence_handler_->Delete(leveldb::WriteOptions(), key);
    if (!status.ok()) {
        LOG(WARNING, "delete %s failed", key.c_str()); 
        return false;
    }
    return true;
}

bool MasterImpl::PersistenceJobInfo(const JobInfo& job_info) {
    // setup persistence cell TODO should be only use one struct
    JobCheckPointCell cell;
    cell.set_job_id(job_info.id);
    cell.set_replica_num(job_info.replica_num);
    cell.set_job_name(job_info.job_name);
    cell.set_job_raw(job_info.job_raw);
    cell.set_cmd_line(job_info.cmd_line);
    cell.set_cpu_share(job_info.cpu_share);
    cell.set_mem_share(job_info.mem_share);
    cell.set_deploy_step_size(job_info.deploy_step_size);
    cell.set_killed(job_info.killed);
    cell.set_cpu_limit(job_info.cpu_limit);

    LOG(DEBUG, "cell name: %s replica_num: %d cmd_line: %s cpu_share: %lf mem_share: %ld deloy_step_size: %d",
            cell.job_name().c_str(),
            cell.replica_num(),
            cell.cmd_line().c_str(),
            cell.cpu_share(),
            cell.mem_share(),
            cell.deploy_step_size());
    // check persistence_handler init
    if (persistence_handler_ == NULL) {
        LOG(WARNING, "persistence handler not inited yet");
        return false;
    }

    // contruct key
    std::string key = boost::lexical_cast<std::string>(job_info.id);
    // serialize cell 
    std::string cell_value;
    if (!cell.SerializeToString(&cell_value)) {
        LOG(WARNING, "serialize job cell %s failed",
                key.c_str()); 
        return false;
    }

    leveldb::Status write_status = 
        persistence_handler_->Put(
            leveldb::WriteOptions(), key, cell_value);
    if (!write_status.ok()) {
        LOG(WARNING, "serialize job cell %s failed to write",
                key.c_str());
        return false;
    }
    return true;
}

bool MasterImpl::SafeModeCheck() {
    if (!is_safe_mode_) {
        return false; 
    }

    int now_time = common::timer::now_time();
    if (now_time - start_time_ > FLAGS_master_safe_mode_last) {
        is_safe_mode_ = false; 
        LOG(INFO, "safe mode close auto");
        return false;
    }

    return true;
}


} // namespace galaxy

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
