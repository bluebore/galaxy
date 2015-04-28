// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "master_impl.h"

#include <vector>
#include <queue>
#include "proto/agent.pb.h"
#include "rpc/rpc_client.h"
#include "scheduler.h"

extern int FLAGS_task_deploy_timeout;
extern int FLAGS_agent_keepalive_timeout;

namespace galaxy {

MasterImpl::MasterImpl()
    : next_agent_id_(0),
      next_task_id_(0),
      next_job_id_(0),
      rpc_client_(NULL),
      scheduler_(NULL) {
    rpc_client_ = new RpcClient();
    scheduler_ =  new LoadBalanceScheduler();
    thread_pool_.AddTask(boost::bind(&MasterImpl::Schedule, this));
    thread_pool_.AddTask(boost::bind(&MasterImpl::DeadCheck, this));
}

MasterImpl::~MasterImpl() {
    delete rpc_client_;
    delete scheduler_;
}

void MasterImpl::TerminateTask(::google::protobuf::RpcController* /*controller*/,
                 const ::galaxy::TerminateTaskRequest* request,
                 ::galaxy::TerminateTaskResponse* response,
                 ::google::protobuf::Closure* done) {
    if (!request->has_task_id()) {
        response->set_status(-1);
        done->Run();
        return;
    }
    int64_t task_id = request->task_id();

    common::MutexLock lock(&agent_lock_);
    std::map<int64_t, TaskInstance>::iterator it;
    it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        response->set_status(-1);
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
        ::google::protobuf::RepeatedPtrField<TaskInstance >* tasks) {
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
        ListTaskForJob(request->job_id(), response->mutable_tasks());
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
        alives_.erase(it);
        it = alives_.begin();
    }
    int idle_time = 5;
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
    const std::string& agent_addr = agent->addr;
    assert(!agent_addr.empty());
    LOG(INFO,"update jobs on agent %s",agent_addr.c_str());
    int32_t now_time = common::timer::now_time();
    std::set<int64_t>::iterator it = agent->running_tasks.begin();
    std::vector<int64_t> del_tasks;
    for (; it != agent->running_tasks.end(); ++it) {
        int64_t task_id = *it;
        if (running_tasks.find(task_id) == running_tasks.end()) { 
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
            tasks_.erase(task_id);
            LOG(INFO, "Job[%s] task %ld disappear from %s",
                job.job_name.c_str(), task_id, agent_addr.c_str());
        }else{
            TaskInstance& instance = tasks_[task_id];
            if(instance.status() != ERROR
               && instance.status() != COMPLETE){
                continue;
            }
            int64_t job_id = instance.job_id();
            assert(jobs_.find(job_id) != jobs_.end());
            JobInfo& job = jobs_[job_id];
            job.agent_tasks[agent_addr].erase(task_id);
            if (job.agent_tasks[agent_addr].empty()) {
                job.agent_tasks.erase(agent_addr);
            }
            job.running_num --;
            del_tasks.push_back(task_id);
            tasks_.erase(task_id);
            if(instance.status() == COMPLETE){
                job.complete_tasks[agent_addr].insert(task_id);
                job.replica_num --;
            }
            //释放资源
            LOG(INFO,"delay cancel task %d on agent %s",task_id,agent_addr.c_str());
            thread_pool_.DelayTask(100, boost::bind(&MasterImpl::DelayRemoveZombieTaskOnAgent,this, agent, task_id));
        }
    }
    for (uint64_t i = 0UL; i < del_tasks.size(); ++i) {
        agent->running_tasks.erase(del_tasks[i]);
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
        agent->cpu_used = request->used_cpu_share();
        agent->mem_used = request->used_mem_share();
        agent->stub = NULL;
        agent->version = 1;
        agent->alive_timestamp = now_time;
    } else {
        agent = &(it->second);
        int32_t es = alives_[agent->alive_timestamp].erase(agent_addr);
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
        agent->cpu_used = request->used_cpu_share();
        agent->mem_used = request->used_mem_share();
        LOG(DEBUG, "cpu_use:%lf, mem_use:%ld", agent->cpu_used, agent->mem_used);
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
        LOG(INFO, "Task %d status: %s",
            task_id, TaskState_Name((TaskState)task_status).c_str());
        instance.set_agent_addr(agent_addr);
        LOG(INFO, "%s run task %d %d", agent_addr.c_str(),
            task_id, request->task_status(i).status());
        if (request->task_status(i).has_cpu_usage()) {
            instance.set_cpu_usage(
                    request->task_status(i).cpu_usage());
            LOG(INFO, "%d use cpu %f", task_id, instance.cpu_usage());
        }
        if (request->task_status(i).has_memory_usage()) {
            instance.set_memory_usage(
                    request->task_status(i).memory_usage());
            LOG(INFO, "%d use memory %ld", task_id, instance.memory_usage());
        }
    }
    UpdateJobsOnAgent(agent, running_tasks);
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
    MutexLock lock(&agent_lock_);
    int64_t job_id = request->job_id();
    std::map<int64_t, JobInfo>::iterator it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        done->Run();
        return;
    }

    JobInfo& job = it->second;
    job.replica_num = 0;
    job.killed = true;
    done->Run();
}

void MasterImpl::UpdateJob(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::UpdateJobRequest* request,
                         ::galaxy::UpdateJobResponse* response,
                         ::google::protobuf::Closure* done) {
    MutexLock lock(&agent_lock_);
    int64_t job_id = request->job_id();
    std::map<int64_t, JobInfo>::iterator it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        response->set_status(-2);
        done->Run();
        return;
    }

    JobInfo& job = it->second;
    job.replica_num = request->replica_num();
    done->Run();
}

void MasterImpl::NewJob(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::NewJobRequest* request,
                         ::galaxy::NewJobResponse* response,
                         ::google::protobuf::Closure* done) {
    MutexLock lock(&agent_lock_);
    int64_t job_id = next_job_id_++;
    JobInfo& job = jobs_[job_id];
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

    response->set_status(0);
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
    RunTaskRequest rt_request;
    rt_request.set_task_id(task_id);
    rt_request.set_task_name(job->job_name);
    rt_request.set_task_raw(job->job_raw);
    rt_request.set_cmd_line(job->cmd_line);
    rt_request.set_cpu_share(job->cpu_share);
    rt_request.set_mem_share(job->mem_share);
    rt_request.set_task_offset(job->running_num);
    rt_request.set_job_replicate_num(job->replica_num);
    RunTaskResponse rt_response;
    LOG(INFO, "ScheduleTask on %s", agent_addr.c_str());
    bool ret = rpc_client_->SendRequest(agent.stub, &Agent_Stub::RunTask,
                                        &rt_request, &rt_response, 5, 1);
    if (!ret 
            || (rt_response.has_status() 
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
        instance.set_agent_addr(agent_addr);
        instance.set_job_id(job->id);
        instance.set_start_time(common::timer::now_time());
        instance.set_status(DEPLOYING);
        instance.set_offset(job->running_num);
        job->agent_tasks[agent_addr].insert(task_id);
        job->running_num ++;
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
        for (int i = job.running_num; i < job.replica_num; i++) {
            LOG(INFO, "[Schedule] Job[%s] running %d tasks, replica_num %d",
                job.job_name.c_str(), job.running_num, job.replica_num);
            std::string agent_addr = AllocResource(job);
            if (agent_addr.empty()) {
                LOG(WARNING, "Allocate resource fail, delay schedule job %s",
                    job.job_name.c_str());
                continue;
            }
            bool ret = ScheduleTask(&job, agent_addr);
            if(ret){
               //update index
               std::map<std::string,AgentInfo>::iterator agent_it = agents_.find(agent_addr);
               if(agent_it != agents_.end()){
                   agent_it->second.mem_used += job.mem_share;
                   agent_it->second.cpu_used += job.cpu_share;
                   agent_it->second.version += 1;
                   SaveIndex(agent_it->second);
               }

            }
        }
    }
    for(uint32_t i = 0;i < should_rm_job.size();i++){
        LOG(INFO,"remove job %ld",should_rm_job[i]);
        jobs_.erase(should_rm_job[i]);
    }
    thread_pool_.DelayTask(1000, boost::bind(&MasterImpl::Schedule, this));
}

double MasterImpl::CalcLoad(const AgentInfo& agent){
    if(agent.mem_share <= 0 || agent.cpu_share <= 0.0 ){
        LOG(FATAL,"invalid agent input ,mem_share %ld,cpu_share %f",agent.mem_share,agent.cpu_share);
        return 0.0;
    }
    return scheduler_->CalcLoad(&agent);
}

std::string MasterImpl::AllocResource(const JobInfo& job){
    LOG(INFO,"alloc resource for job %ld,mem_require %ld, cpu_require %f",
        job.id,job.mem_share,job.cpu_share);
    agent_lock_.AssertHeld();
    return scheduler_->PickAgent(&index_, &job);
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

} // namespace galasy

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
