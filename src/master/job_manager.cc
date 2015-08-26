// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "job_manager.h"

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <gflags/gflags.h>
#include "proto/agent.pb.h"
#include "proto/master.pb.h"
#include "proto/galaxy.pb.h"
#include "master_util.h"
#include "utils/resource_utils.h"
#include "timer.h"
#include <logging.h>

DECLARE_int32(master_agent_timeout);
DECLARE_int32(master_agent_rpc_timeout);
DECLARE_int32(master_query_period);

namespace baidu {
namespace galaxy {
JobManager::JobManager()
    : on_query_num_(0) {
    safe_mode_ = true;
    ScheduleNextQuery();
    state_to_stage_[kPodPending] = kStageRunning;
    state_to_stage_[kPodDeploying] = kStageRunning;
    state_to_stage_[kPodRunning] = kStageRunning;
    state_to_stage_[kPodTerminate] = kStageDeath;
    state_to_stage_[kPodError] = kStageDeath;
    BuildPodFsm();
}

JobManager::~JobManager() {
}

Status JobManager::LabelAgents(const LabelCell& label_cell) {
    AgentSet new_label_set;
    for (int i = 0; i < label_cell.agents_endpoint_size(); ++i) {
        new_label_set.insert(label_cell.agents_endpoint(i)); 
    }
    MutexLock lock(&mutex_);
    AgentSet& old_label_set = labels_[label_cell.label()];
    AgentSet to_remove;
    AgentSet to_add;
    MasterUtil::SetDiff(old_label_set, 
                        new_label_set, &to_remove, &to_add);
    LOG(INFO, "label %s old label %u, new label %u, to_remove %u, to_add %u",
            label_cell.label().c_str(), 
            old_label_set.size(), 
            new_label_set.size(), 
            to_remove.size(), 
            to_add.size()); 
    // TODO may use internal AgentInfo with set 
    for (AgentSet::iterator it = to_remove.begin(); 
                                it != to_remove.end(); ++it) {
        boost::unordered_map<AgentAddr, LabelSet>::iterator 
                lab_it = agent_labels_.find(*it);
        if (lab_it == agent_labels_.end()) {
            continue; 
        }
        LOG(DEBUG, "%s remove label %s", (*it).c_str(), label_cell.label().c_str());
        lab_it->second.erase(label_cell.label());
        agent_labels_[*it].erase(label_cell.label()); 
        // update Version
        std::map<AgentAddr, AgentInfo*>::iterator agent_it 
                                            = agents_.find(*it); 
        if (agent_it != agents_.end()) {
            AgentInfo* agent_ptr = agent_it->second;
            agent_ptr->set_version(agent_ptr->version() + 1);
            MasterUtil::ResetLabels(agent_ptr, agent_labels_[*it]);
        }
    }
    for (AgentSet::iterator it = to_add.begin();
                                it != to_add.end(); ++it) {
        agent_labels_[*it].insert(label_cell.label()); 
        // update Version
        LOG(DEBUG, "%s add label %s", (*it).c_str(), label_cell.label().c_str());
        std::map<AgentAddr, AgentInfo*>::iterator agent_it 
                                            = agents_.find(*it);
        if (agent_it != agents_.end()) {
            AgentInfo* agent_ptr = agent_it->second;
            agent_ptr->set_version(agent_ptr->version() + 1);
            MasterUtil::ResetLabels(agent_ptr, agent_labels_[*it]);
        }
    }

    labels_[label_cell.label()] = new_label_set;
    return kOk;
}

void JobManager::Add(const JobId& job_id, const JobDescriptor& job_desc) {
    Job* job = new Job();
    job->state_ = kJobNormal;
    job->desc_.CopyFrom(job_desc);
    job->id_ = job_id;
    MutexLock lock(&mutex_);
    jobs_[job_id] = job;
    FillPodsToJob(job);
    LOG(INFO, "job %s submitted by user: %s, with deploy_step %d, replica %d ",
      job_id.c_str(), 
      job_desc.user().c_str(),
      job_desc.deploy_step(),
      job_desc.replica());
}

Status JobManager::Update(const JobId& job_id, const JobDescriptor& job_desc) {
    MutexLock lock(&mutex_);
    std::map<JobId, Job*>::iterator it;
    it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        LOG(WARNING, "update job failed, job not found: %s", job_id.c_str());
        return kJobNotFound;
    }
    int old_replica = it->second->desc_.replica();
    it->second->desc_.set_replica(job_desc.replica());
    if (old_replica < job_desc.replica()) {
        FillPodsToJob(it->second);
    } else if (old_replica > job_desc.replica()) {
        scale_down_jobs_.insert(it->second->id_);
    }
    //it->second->desc_.CopyFrom(job_desc);
    LOG(INFO, "job desc updated succes: %s", job_desc.name().c_str());
    return kOk;
}

void JobManager::FillPodsToJob(Job* job) {
    mutex_.AssertHeld();
    for(int i = job->pods_.size(); i < job->desc_.replica(); i++) {
        PodId pod_id = MasterUtil::UUID();
        PodStatus* pod_status = new PodStatus();
        pod_status->set_podid(pod_id);
        pod_status->set_jobid(job->id_);
        pod_status->set_state(kPodPending);
        pod_status->set_stage(kStagePending);
        job->pods_[pod_id] = pod_status;
        pending_pods_[job->id_][pod_id] = pod_status;
        LOG(INFO, "move pod to pendings: %s", pod_id.c_str());
    }
}

void JobManager::FillAllJobs() {
    mutex_.AssertHeld();
    std::map<JobId, Job*>::iterator it;
    for (it = jobs_.begin(); it != jobs_.end(); ++it) {
        JobId job_id = it->first;
        Job* job = it->second;
        FillPodsToJob(job);
    }
}

void JobManager::ReloadJobInfo(const JobInfo& job_info) {
    Job* job = new Job();
    job->state_ = job_info.state();
    job->desc_.CopyFrom(job_info.desc());
    std::string job_id = job_info.jobid();
    job->id_ = job_id;
    MutexLock lock(&mutex_);
    jobs_[job_id] = job;
}


void JobManager::KillPodCallback(PodStatus* pod,
                                 const KillPodRequest* request, KillPodResponse* response, 
                                 bool failed, int /*error*/) {
    boost::scoped_ptr<const KillPodRequest> request_ptr(request);
    boost::scoped_ptr<KillPodResponse> response_ptr(response);
    MutexLock lock(&mutex_);
    if (pod == NULL) {
        return;
    }
    const std::string& jobid = pod->jobid();
    const std::string& podid = pod->podid();
    Status status = response->status();
    if (failed || status != kOk) {
        LOG(INFO, "Kill pod [%s %s] on [%s] fail: %d", jobid.c_str(),
            podid.c_str(), pod->endpoint().c_str(), status);
        return;
    }
    LOG(INFO, "send kill cmd for pod [%s %s] on [%s] success", jobid.c_str(),
        podid.c_str(), pod->endpoint().c_str());
}

void JobManager::GetPendingPods(JobInfoList* pending_pods,
                                int32_t max_scale_up_size,
                                JobInfoList* scale_down_pods,
                                int32_t max_scale_down_size) {
    MutexLock lock(&mutex_);
    if (safe_mode_) {
        return;
    }
    ProcessScaleDown(scale_down_pods, max_scale_down_size);
    ProcessScaleUp(pending_pods, max_scale_up_size);
}

void JobManager::ProcessScaleDown(JobInfoList* scale_down_pods,
                                  int32_t max_scale_down_size) {
    mutex_.AssertHeld();
    std::map<JobId, std::map<PodId, PodStatus*> >::iterator it;
    std::vector<JobId> should_rm;
    std::set<JobId>::iterator jobid_it = scale_down_jobs_.begin();
    int32_t job_count = 0;
    for (;jobid_it != scale_down_jobs_.end(); ++jobid_it) {
        if (job_count >= max_scale_down_size) {
            return;
        }
        std::map<JobId, Job*>::iterator job_it = jobs_.find(*jobid_it);
        if (job_it == jobs_.end()) {
            continue;
        }
        size_t replica = job_it->second->desc_.replica();
        if (replica >= job_it->second->pods_.size()) {
            should_rm.push_back(*jobid_it);
            continue;
        }
        int32_t scale_down_count = job_it->second->pods_.size() \
                                   - job_it->second->desc_.replica();
        it = pending_pods_.find(*jobid_it);
        std::vector<PodStatus*> will_rm_pods;
        if (it != pending_pods_.end()) {
            std::map<PodId, PodStatus*>::const_iterator jt = it->second.begin();
            for(; jt != it->second.end() && scale_down_count >0; 
                ++jt) { 
                --scale_down_count;
                will_rm_pods.push_back(jt->second);
            }
            for (size_t i = 0; i < will_rm_pods.size(); ++i) {
                ChangeStage(kStageRemoved, will_rm_pods[i], job_it->second);
            }
        }
        if (scale_down_count > 0) {
            job_count++;
            JobInfo* job_info = scale_down_pods->Add();
            job_info->mutable_desc()->CopyFrom(job_it->second->desc_);
            job_info->set_jobid(job_it->second->id_);
            std::map<PodId, PodStatus*>::const_iterator jt;
            for (jt = job_it->second->pods_.begin();
                 jt != job_it->second->pods_.end(); ++jt) {
                PodStatus* pod_status = jt->second;
                PodStatus* new_pod_status = job_info->add_pods();
                new_pod_status->CopyFrom(*pod_status);
            }
        }
    }
    for (size_t i = 0; i < should_rm.size(); ++i) {
        scale_down_jobs_.erase(should_rm[i]);
    }
}

void JobManager::ProcessScaleUp(JobInfoList* scale_up_pods,
                                int32_t max_scale_up_size) {
    mutex_.AssertHeld();
    int job_count = 0;
    std::map<JobId, std::map<PodId, PodStatus*> >::iterator it;
    for (it = pending_pods_.begin(); it != pending_pods_.end(); ++it) {
        if (job_count >= max_scale_up_size) {
            return;
        }
        JobInfo* job_info = scale_up_pods->Add();
        JobId job_id = it->first;
        job_info->set_jobid(job_id);
        const JobDescriptor& job_desc = jobs_[job_id]->desc_;
        job_info->mutable_desc()->CopyFrom(job_desc);
        const std::map<PodId, PodStatus*> & job_pending_pods = it->second;
        std::map<PodId, PodStatus*>::const_iterator jt;
        for (jt = job_pending_pods.begin(); jt != job_pending_pods.end(); ++jt) {
            PodStatus* pod_status = jt->second;
            PodStatus* new_pod_status = job_info->add_pods();
            new_pod_status->CopyFrom(*pod_status);
        }
        job_count++;
    }
}

Status JobManager::Propose(const ScheduleInfo& sche_info) {
    MutexLock lock(&mutex_);
    const std::string& jobid = sche_info.jobid();
    const std::string& podid = sche_info.podid();
    const std::string& endpoint = sche_info.endpoint();
    std::map<JobId, Job*>::iterator job_it = jobs_.find(jobid);
    if (job_it == jobs_.end()) { 
        LOG(INFO, "propose fail, no such job: %s", jobid.c_str());
        return kJobNotFound;
    }
    if (sche_info.action() == kTerminate) {
        std::map<PodId, PodStatus*>::iterator p_it = job_it->second->pods_.find(sche_info.podid());
        if (p_it == job_it->second->pods_.end()) {
            LOG(WARNING, "propose fail, no such pod: %s", sche_info.podid().c_str());
            return kPodNotFound;
        }
        ChangeStage(kStageRemoved, p_it->second, job_it->second);
        return kOk;
    }

    std::map<JobId, std::map<PodId, PodStatus*> >::iterator it;
    it = pending_pods_.find(jobid);
    if (it == pending_pods_.end()) {
        return kJobNotFound;
    }
    std::map<PodId, PodStatus*>& job_pending_pods = it->second;
    std::map<PodId, PodStatus*>::iterator jt = job_pending_pods.find(podid);
    if (jt == job_pending_pods.end()) {
        LOG(INFO, "propse fail, no such pod: %s", podid.c_str());
        return kPodNotFound;
    }
    std::map<AgentAddr, AgentInfo*>::iterator at = agents_.find(endpoint);
    if (at == agents_.end()) {
        LOG(INFO, "propose fail, no such agent: %s", endpoint.c_str());
        return kAgentNotFound;
    }

    
    PodStatus* pod = jt->second;
    AgentInfo* agent = at->second;
    Status feasible_status = AcquireResource(*pod, agent);
    if (feasible_status != kOk) {
        LOG(INFO, "propose fail, no resource, error code:[%d]", feasible_status);
        return feasible_status;
    }
    // update agent version
    agent->set_version(agent->version() + 1);
    pod->set_endpoint(sche_info.endpoint());
    job_pending_pods.erase(jt);
    if (job_pending_pods.size() == 0) {
        pending_pods_.erase(it);
    }
    ChangeStage(kStageRunning, pod, job_it->second);
    LOG(INFO, "propose success, %s will be run on %s",
        podid.c_str(), endpoint.c_str());
    return kOk;
}

Status JobManager::AcquireResource(const PodStatus& pod, AgentInfo* agent) {
    mutex_.AssertHeld();
    Resource pod_requirement;
    GetPodRequirement(pod, &pod_requirement);
    // calc agent unassigned resource 
    Resource unassigned;
    unassigned.CopyFrom(agent->total());
    bool ret = ResourceUtils::Alloc(agent->assigned(), unassigned);
    if (!ret) {
        return kQuota;
    }
    if (!MasterUtil::FitResource(pod_requirement, unassigned)) {
        return kQuota;
    }
    return kOk;
}


void JobManager::ReclaimResource(const PodStatus& pod, AgentInfo* agent) {
    mutex_.AssertHeld();
    Resource pod_requirement;
    GetPodRequirement(pod, &pod_requirement);
    MasterUtil::SubstractResource(pod_requirement, agent->mutable_assigned());
}

void JobManager::GetPodRequirement(const PodStatus& pod, Resource* requirement) {
    Job* job = jobs_[pod.jobid()];
    const PodDescriptor& pod_desc = job->desc_.pod();
    requirement->CopyFrom(pod_desc.requirement());
}

void JobManager::CalculatePodRequirement(const PodDescriptor& pod_desc,
                                         Resource* pod_requirement) {
    for (int32_t i = 0; i < pod_desc.tasks_size(); i++) {
        const TaskDescriptor& task_desc = pod_desc.tasks(i);
        const Resource& task_requirement = task_desc.requirement();
        pod_requirement->set_millicores(pod_requirement->millicores() + task_requirement.millicores());
        pod_requirement->set_memory(pod_requirement->memory() + task_requirement.memory());
        for (int32_t j = 0; j < task_requirement.ports_size(); j++) {
            pod_requirement->add_ports(task_requirement.ports(j));
        }
    }
}

void JobManager::KeepAlive(const std::string& agent_addr) {
    if (agent_addr == "") {
        LOG(WARNING, "ignore heartbeat with empty endpoint");
        return;
    }
    {
        MutexLock lock(&mutex_);
        if (agents_.find(agent_addr) == agents_.end()) {
            LOG(INFO, "new agent added: %s", agent_addr.c_str());
            AgentInfo* agent = new AgentInfo();
            agent->set_version(0);
            agents_[agent_addr] = agent;
            MasterUtil::ResetLabels(agent, agent_labels_[agent_addr]);
        }
        AgentInfo* agent = agents_[agent_addr];
        agent->set_state(kAlive);
        agent->set_endpoint(agent_addr);
    }

    MutexLock lock(&mutex_timer_);
    LOG(DEBUG, "receive heartbeat from %s", agent_addr.c_str());
    int64_t timer_id;
    if (agent_timer_.find(agent_addr) != agent_timer_.end()) {
        timer_id = agent_timer_[agent_addr];
        bool cancel_ok = death_checker_.CancelTask(timer_id);
        if (!cancel_ok) {
            LOG(WARNING, "agent is offline, agent: %s", agent_addr.c_str());
            {
                MutexLock g_lock(&mutex_);
                AgentInfo* agent = agents_[agent_addr];
                agent->set_state(kDead);
            }
        }
    }
    timer_id = death_checker_.DelayTask(FLAGS_master_agent_timeout,
                                        boost::bind(&JobManager::HandleAgentOffline,
                                                    this,
                                                    agent_addr));
    agent_timer_[agent_addr] = timer_id;
}

void JobManager::HandleAgentOffline(const std::string agent_addr) {
    LOG(WARNING, "agent is offline: %s", agent_addr.c_str());
    MutexLock lock(&mutex_);
    if (agents_.find(agent_addr) == agents_.end()) {
        LOG(INFO, "no such agent %s", agent_addr.c_str());
        return;
    }
    PodMap& agent_pods = pods_on_agent_[agent_addr];
    PodMap::iterator it = agent_pods.begin();
    for (; it != agent_pods.end(); ++it) {
        std::map<PodId, PodStatus*>::iterator jt = it->second.begin();
        for (; jt != it->second.end(); ++jt) {
            PodStatus* pod = jt->second; 
            std::map<JobId, Job*>::iterator job_it = jobs_.find(pod->jobid());
            if (job_it ==  jobs_.end()) {
                continue;
            }
            ChangeStage(kStagePending, pod, job_it->second);
        }
    }
    LOG(INFO, "agent is dead: %s", agent_addr.c_str());
}

void JobManager::RunPod(const PodDescriptor& desc, PodStatus* pod) {
    mutex_.AssertHeld();
    RunPodRequest* request = new RunPodRequest;
    RunPodResponse* response = new RunPodResponse;
    request->set_podid(pod->podid());
    request->mutable_pod()->CopyFrom(desc);
    request->set_jobid(pod->jobid());
    Agent_Stub* stub;
    const AgentAddr& endpoint = pod->endpoint();
    rpc_client_.GetStub(endpoint, &stub);
    boost::function<void (const RunPodRequest*, RunPodResponse*, bool, int)> run_pod_callback;
    run_pod_callback = boost::bind(&JobManager::RunPodCallback, this, pod, endpoint,
                                   _1, _2, _3, _4);
    rpc_client_.AsyncRequest(stub, &Agent_Stub::RunPod, request, response,
                             run_pod_callback, FLAGS_master_agent_rpc_timeout, 0);
    delete stub;
}


void JobManager::RunPodCallback(PodStatus* pod, AgentAddr endpoint,
                                const RunPodRequest* request,
                                RunPodResponse* response,
                                bool failed, int /*error*/) {
    boost::scoped_ptr<const RunPodRequest> request_ptr(request);
    boost::scoped_ptr<RunPodResponse> response_ptr(response);
    MutexLock lock(&mutex_);
    const std::string& jobid = pod->jobid();
    const std::string& podid = pod->podid();
    Status status = response->status();
    std::map<JobId, Job*>::iterator job_it = jobs_.find(jobid);
    if (job_it == jobs_.end()) {
        LOG(WARNING, "job %s does not exist in master", jobid.c_str());
        return;
    }
    if (failed || status != kOk) {
        LOG(WARNING, "run pod [%s %s] on [%s] fail: %d", jobid.c_str(),
            podid.c_str(), endpoint.c_str(), status);
        ChangeStage(kStageDeath, pod, job_it->second);
    } else {
        std::map<PodId, PodStatus*>::iterator pod_it = job_it->second->pods_.find(podid);
        if (pod_it == job_it->second->pods_.end()) {
            LOG(WARNING, "pod %s does not exist in master", podid.c_str());
            return;
        }
        PodStatus* pod_status = pod_it->second;
        agent_sequence_ids_[pod->endpoint()]++;
        pods_on_agent_[pod->endpoint()][jobid].insert(std::make_pair(pod_status->podid(),pod_status));
        LOG(INFO, "run pod [%s %s] on [%s] success", jobid.c_str(),
          podid.c_str(), 
          endpoint.c_str());
    }
}


void JobManager::ScheduleNextQuery() {
    thread_pool_.DelayTask(FLAGS_master_query_period, boost::bind(&JobManager::Query, this));
}

void JobManager::Query() {
    MutexLock lock(&mutex_);
    assert(on_query_num_ == 0);
    std::map<AgentAddr, AgentInfo*>::iterator it;
    for (it = agents_.begin(); it != agents_.end(); ++it) {
        AgentInfo* agent = it->second;
        if (agent->state() == kDead) {
            continue;
        }
        QueryAgent(agent);
    }
    LOG(INFO, "query %lld agents", on_query_num_);
    if (on_query_num_ == 0) {
        ScheduleNextQuery();
    }
}

void JobManager::QueryAgent(AgentInfo* agent) {
    mutex_.AssertHeld();
    const AgentAddr& endpoint = agent->endpoint();
    int64_t seq_id = agent_sequence_ids_[endpoint];
    if (agent->state() != kAlive) {
        LOG(DEBUG, "ignore dead agent [%s]", endpoint.c_str());
        return;
    }
    QueryRequest* request = new QueryRequest;
    QueryResponse* response = new QueryResponse;

    Agent_Stub* stub;
    rpc_client_.GetStub(endpoint, &stub);
    boost::function<void (const QueryRequest*, QueryResponse*, bool, int)> query_callback;
    query_callback = boost::bind(&JobManager::QueryAgentCallback, this,
                                 endpoint, _1, _2, _3, _4, seq_id);
    rpc_client_.AsyncRequest(stub, &Agent_Stub::Query, request, response,
                             query_callback, FLAGS_master_agent_rpc_timeout, 0);
    delete stub;
    on_query_num_++;
}

void JobManager::QueryAgentCallback(AgentAddr endpoint, const QueryRequest* request,
                                    QueryResponse* response, bool failed, int /*error*/,
                                    int64_t seq_id_at_query) {
    boost::scoped_ptr<const QueryRequest> request_ptr(request);
    boost::scoped_ptr<QueryResponse> response_ptr(response);
    MutexLock lock(&mutex_);
    bool first_query_on_agent = false;
    if (queried_agents_.find(endpoint) == queried_agents_.end()) {
        first_query_on_agent = true;
        LOG(INFO, "first query callback for agent: %s", endpoint.c_str());
        queried_agents_.insert(endpoint);
    }
    
    if (--on_query_num_ == 0) {
        ScheduleNextQuery();
    }
    std::map<AgentAddr, AgentInfo*>::iterator it = agents_.find(endpoint);
    if (it == agents_.end()) {
        LOG(FATAL, "query agent [%s] not exist", endpoint.c_str());
        return;
    }
    int64_t current_seq_id = agent_sequence_ids_[endpoint];
    if (seq_id_at_query < current_seq_id) {
        LOG(FATAL, "outdated query callback, just ignore it. %ld < %ld, %s", 
            seq_id_at_query, current_seq_id, endpoint.c_str());
        return;
    }
    Status status = response->status();
    if (failed || status != kOk) {
        LOG(INFO, "query agent [%s] fail: %d", endpoint.c_str(), status);
        return;
    }
    const AgentInfo& report_agent_info = response->agent();
    LOG(INFO, "agent %s stat: mem total %ld, cpu total %d, mem assigned %ld, cpu assigend %d, mem used %ld , cpu used %d, pod size %d ",
        report_agent_info.endpoint().c_str(),
        report_agent_info.total().memory(), report_agent_info.total().millicores(),
        report_agent_info.assigned().memory(), report_agent_info.assigned().millicores(),
        report_agent_info.used().memory(), report_agent_info.used().millicores(),
        report_agent_info.pods_size());
    AgentInfo* agent = it->second;
    UpdateAgentVersion(agent, report_agent_info);
    // copy only needed 
    agent->mutable_total()->CopyFrom(report_agent_info.total()); 
    agent->mutable_assigned()->CopyFrom(report_agent_info.assigned());
    agent->mutable_used()->CopyFrom(report_agent_info.used());
    agent->mutable_pods()->CopyFrom(report_agent_info.pods());
    PodMap pods_not_on_agent = pods_on_agent_[endpoint]; // this is a copy
    for (int32_t i = 0; i < report_agent_info.pods_size(); i++) {
        const PodStatus& report_pod_info = report_agent_info.pods(i);
        const JobId& jobid = report_pod_info.jobid();
        const PodId& podid = report_pod_info.podid(); 
        LOG(INFO, "the pod %s  of job %s on agent %s state %s",
                  podid.c_str(), 
                  jobid.c_str(), 
                  report_agent_info.endpoint().c_str(),
                  PodState_Name(report_pod_info.state()).c_str()); 
        // validate job 
        std::map<JobId, Job*>::iterator job_it = jobs_.find(jobid);
        if (job_it == jobs_.end()) {
            LOG(WARNING, "the job %s from agent %s does not exist in master",
               jobid.c_str(), 
               report_agent_info.endpoint().c_str());
            continue;
        }
        // for recovering
        if (first_query_on_agent 
            && jobs_[jobid]->pods_.find(podid) == jobs_[jobid]->pods_.end()) {
            PodStatus* pod = new PodStatus();
            pod->CopyFrom(report_pod_info);
            pod->set_stage(state_to_stage_[pod->state()]);
            pod->set_endpoint(report_agent_info.endpoint());
            jobs_[jobid]->pods_[podid] = pod; 
            pods_not_on_agent[jobid][podid] = pod;
            pods_on_agent_[endpoint][jobid][podid] = pod;
        }
        // validate pod
        Job* job = job_it->second;
        std::map<PodId, PodStatus*>::iterator p_it = job->pods_.find(podid);
        if (p_it == job->pods_.end()) {
            // TODO should kill this pod
            LOG(WARNING, "the pod %s from agent %s does not exist in master",
               podid.c_str(), report_agent_info.endpoint().c_str());
            continue;
        } 
        // update pod in master
        PodStatus* pod = jobs_[jobid]->pods_[podid];
        pod->mutable_status()->CopyFrom(report_pod_info.status());
        pod->mutable_resource_used()->CopyFrom(report_pod_info.resource_used());
        pod->set_state(report_pod_info.state());
        pods_not_on_agent[jobid].erase(podid);
        if (pods_not_on_agent[jobid].size() == 0) {
            pods_not_on_agent.erase(jobid);
        }
        ChangeStage(state_to_stage_[pod->state()], pod, job);
    }

    PodMap::iterator j_it = pods_not_on_agent.begin();
    for (; j_it != pods_not_on_agent.end(); ++j_it) {
        std::map<JobId, Job*>::iterator job_it = jobs_.find(j_it->first);
        if (job_it == jobs_.end()) {
            continue;
        }
        std::map<PodId, PodStatus*>::iterator p_it = j_it->second.begin();
        for (; p_it != j_it->second.end(); ++p_it) {
            LOG(INFO, "pod %s in master but not in agent, stage in master is %s ", 
              p_it->second->podid().c_str(),
              PodStage_Name(p_it->second->stage()).c_str());
            ChangeStage(kStagePending, p_it->second, job_it->second);
        }
    }

    if (queried_agents_.size() == agents_.size() && safe_mode_) {
        FillAllJobs();
        safe_mode_ = false;
        LOG(INFO, "master leave safe mode");
    }
}

void JobManager::UpdateAgentVersion(AgentInfo* old_agent_info,
                        const AgentInfo& new_agent_info) {
  
    int old_version = old_agent_info->version();
    // check assigned
    do {
        int32_t check_assigned = ResourceUtils::Compare(
                        old_agent_info->assigned(),
                        new_agent_info.assigned());
        if (check_assigned != 0) {
            old_agent_info->set_version(old_agent_info->version() + 1);
            break;
        }

        // check used
        int32_t check_used = ResourceUtils::Compare(
                        old_agent_info->used(),
                        new_agent_info.used());
        if (check_used != 0) {
            old_agent_info->set_version(old_agent_info->version() + 1);
            break;
        }

        // check total resource 
        int32_t check_total = ResourceUtils::Compare(
                        old_agent_info->total(), 
                        new_agent_info.total());
        if (check_total != 0) {
            old_agent_info->set_version(old_agent_info->version() + 1);
            break;
        } 
    } while(0);

    LOG(INFO, "agent %s change version from %d to %d", 
              old_agent_info->endpoint().c_str(), 
              old_version,
              old_agent_info->version());
    return; 
}

void JobManager::GetAgentsInfo(AgentInfoList* agents_info) {
    MutexLock lock(&mutex_);
    if (safe_mode_) {
        return;
    }
    std::map<AgentAddr, AgentInfo*>::iterator it;
    for (it = agents_.begin(); it != agents_.end(); ++it) {
        AgentInfo* agent = it->second;
        agents_info->Add()->CopyFrom(*agent);
    }
}

void JobManager::GetAliveAgentsInfo(AgentInfoList* agents_info) {
    MutexLock lock(&mutex_);
    std::map<AgentAddr, AgentInfo*>::iterator it;
    for (it = agents_.begin(); it != agents_.end(); ++it) {
        AgentInfo* agent = it->second;
        if (agent->state() != kAlive) {
            continue;
        }
        agents_info->Add()->CopyFrom(*agent);
    }
}

void JobManager::GetAliveAgentsByDiff(const DiffVersionList& versions,
                                      AgentInfoList* agents_info,
                                      StringList* deleted_agents) {
    MutexLock lock(&mutex_);
    LOG(INFO, "get alive agents by diff , diff count %u", versions.size());
    // ms
    long now_time = ::baidu::common::timer::get_micros() / 1000;
    boost::unordered_map<AgentAddr, int32_t> agents_ver_map;
    for (int i = 0; i < versions.size(); i++) {
        agents_ver_map.insert(std::make_pair(versions.Get(i).endpoint(),
                                             versions.Get(i).version()));
    }
    std::map<AgentAddr, AgentInfo*>::iterator it;
    for (it = agents_.begin(); it != agents_.end(); ++it) {
        boost::unordered_map<AgentAddr, int32_t>::iterator a_it =
                       agents_ver_map.find(it->first);
        AgentInfo* agent = it->second;
        if (agent->state() != kAlive) {
            if (a_it != agents_ver_map.end()) {
                deleted_agents->Add()->assign(it->first);
            }
            continue;
        }
        if (a_it != agents_ver_map.end() && a_it->second == it->second->version()) {
            continue;
        }
        if (a_it != agents_ver_map.end()) {
            LOG(INFO, "agent %s version has diff , the old is %d, the new is %d",
                it->second->endpoint().c_str(), a_it->second, it->second->version());
        }
        agents_info->Add()->CopyFrom(*agent);
    }
    long used_time = ::baidu::common::timer::get_micros() / 1000 - now_time;
    LOG(INFO, "process diff with time consumed %ld, agents count %d ", 
               used_time,
               agents_info->size());
}
void JobManager::GetJobsOverview(JobOverviewList* jobs_overview) {
    MutexLock lock(&mutex_);
    std::map<JobId, Job*>::iterator job_it = jobs_.begin();
    for (; job_it != jobs_.end(); ++job_it) {
        const JobId& jobid = job_it->first;
        Job* job = job_it->second;
        JobOverview* overview = jobs_overview->Add();
        overview->mutable_desc()->CopyFrom(job->desc_);
        overview->mutable_desc()->mutable_pod()->Clear();
        overview->set_jobid(jobid);
        overview->set_state(job->state_);

        uint32_t running_num = 0;
        uint32_t pending_num = 0;
        uint32_t deploying_num = 0;
        std::map<PodId, PodStatus*>& pods = job->pods_;
        std::map<PodId, PodStatus*>::iterator pod_it = pods.begin();
        for (; pod_it != pods.end(); ++pod_it) {
            // const PodId& podid = pod_it->first;
            const PodStatus* pod = pod_it->second;
            if (pod->state() == kPodRunning) {
                running_num++;
                MasterUtil::AddResource(pod->resource_used(), overview->mutable_resource_used());
            } else if(pod->state() == kPodPending) {
                pending_num++;
            } else if(pod->state() == kPodDeploying) {
                deploying_num++;
            } 
        }
        overview->set_running_num(running_num);
        overview->set_pending_num(pending_num);
        overview->set_deploying_num(deploying_num);
    }
}

Status JobManager::GetJobInfo(const JobId& jobid, JobInfo* job_info) {
    MutexLock lock(&mutex_);
    std::map<JobId, Job*>::iterator job_it = jobs_.find(jobid);
    if (job_it == jobs_.end()) {
        LOG(WARNING, "get job info failed, no such job: %s", jobid.c_str());
        return kJobNotFound;
    }

    Job* job = job_it->second;
    job_info->set_jobid(jobid);
    job_info->set_state(job->state_);
    job_info->mutable_desc()->CopyFrom(job->desc_);
    std::map<PodId, PodStatus*>::iterator pod_it = job->pods_.begin();
    for (; pod_it != job->pods_.end(); ++pod_it) {
        PodStatus* pod = pod_it->second;
        job_info->add_pods()->CopyFrom(*pod);
    }
    return kOk;
}

void JobManager::BuildPodFsm() {
    fsm_.insert(std::make_pair(BuildHandlerKey(kStagePending, kStageRemoved), 
          boost::bind(&JobManager::HandleCleanPod, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStagePending, kStageRunning), 
          boost::bind(&JobManager::HandlePendingToRunning, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStageRunning, kStageDeath), 
          boost::bind(&JobManager::HandleRunningToDeath, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStageRunning, kStageRemoved), 
          boost::bind(&JobManager::HandleRunningToRemoved, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStageRunning, kStageRunning), 
          boost::bind(&JobManager::HandleDoNothing, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStageDeath, kStageDeath), 
          boost::bind(&JobManager::HandleRunningToDeath, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStageDeath, kStagePending), 
          boost::bind(&JobManager::HandleDeathToPending, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStageRemoved, kStageDeath), 
          boost::bind(&JobManager::HandleCleanPod, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStageRemoved, kStagePending), 
          boost::bind(&JobManager::HandleCleanPod, this, _1, _2)));
    fsm_.insert(std::make_pair(BuildHandlerKey(kStageRemoved, kStageRemoved), 
          boost::bind(&JobManager::HandleDoNothing, this, _1, _2)));

}

std::string JobManager::BuildHandlerKey(const PodStage& from,
                                        const PodStage& to) {
    return PodStage_Name(from) + ":" + PodStage_Name(to);
}

void JobManager::ChangeStage(const PodStage& to,
                             PodStatus* pod,
                             Job* job) {
    mutex_.AssertHeld();
    if (safe_mode_) { 
        LOG(WARNING, "master does not support stage change in safe mode");
        return;
    }
    if (pod == NULL || job == NULL) {
        LOG(WARNING, "change pod invalidate input");
        return;
    }
    LOG(INFO, "pod %s change stage from %s to %s",
      pod->podid().c_str(),
      PodStage_Name(pod->stage()).c_str(),
      PodStage_Name(to).c_str());
    std::string handler_key = BuildHandlerKey(pod->stage(), to);
    std::map<std::string, Handle>::iterator h_it = fsm_.find(handler_key);
    if (h_it == fsm_.end()) {
        LOG(WARNING, "pod %s can not change stage from %s to %s",
        pod->podid().c_str(),
        PodStage_Name(pod->stage()).c_str(),
        PodStage_Name(to).c_str());
        return;
    }
    h_it->second(pod, job);
}

bool JobManager::HandleCleanPod(PodStatus* pod, Job* job) {
    mutex_.AssertHeld();
    if (pod == NULL || job == NULL) {
        LOG(WARNING, "fsm the input is invalidate");
        return false;
    }
    LOG(WARNING, "clean pod %s from job %s",
        pod->podid().c_str(),pod->jobid().c_str());
    PodMap::iterator job_it = pending_pods_.find(pod->jobid());
    if (job_it != pending_pods_.end()) {
        job_it->second.erase(pod->podid());
        if (job_it->second.size() <= 0) {
            pending_pods_.erase(pod->jobid());
        }
    }
    std::map<AgentAddr, PodMap>::iterator a_it = pods_on_agent_.find(pod->endpoint());
    if (a_it != pods_on_agent_.end()) {
        PodMap::iterator p_it = a_it->second.find(pod->jobid());
        if (p_it != a_it->second.end()) {
            p_it->second.erase(pod->podid());
        }
    }
    fsm_.erase(pod->podid());
    job->pods_.erase(pod->podid());
    delete pod;
    return true;
}

bool JobManager::HandlePendingToRunning(PodStatus* pod, Job* job) {
    mutex_.AssertHeld();
    if (pod == NULL || job == NULL) {
        LOG(WARNING, "fsm the input is invalidate");
        return false;
    }
    pod->set_stage(kStageRunning);
    pod->set_state(kPodDeploying);
    RunPod(job->desc_.pod(), pod);
    return true;
}

bool JobManager::HandleRunningToDeath(PodStatus* pod, Job* job) { 
    mutex_.AssertHeld();
    if (pod == NULL || job == NULL) {
        LOG(WARNING, "fsm the input is invalidate");
        return false;
    }
    pod->set_stage(kStageDeath);
    SendKillToAgent(pod);
    return true;
}

bool JobManager::HandleDeathToPending(PodStatus* pod, Job* job) {
    mutex_.AssertHeld();
    if (pod == NULL || job == NULL) {
        LOG(WARNING, "fsm the input is invalidate");
        return false;
    }
    pod->set_stage(kStagePending);
    pod->set_state(kPodPending);
    LOG(INFO, "reschedule pod %s of job %s", pod->podid().c_str(), job->id_.c_str());
    pending_pods_[job->id_][pod->podid()] = pod;
    std::map<AgentAddr, PodMap>::iterator a_it = pods_on_agent_.find(pod->endpoint());
    if (a_it != pods_on_agent_.end()) {
        PodMap::iterator p_it = a_it->second.find(pod->jobid());
        if (p_it != a_it->second.end()) {
            p_it->second.erase(pod->podid());
        }
    }
    return true;
}

bool JobManager::HandleRunningToRemoved(PodStatus* pod, Job* job) {
    mutex_.AssertHeld();
    if (pod == NULL || job == NULL) {
        LOG(WARNING, "fsm the input is invalidate");
        return false;
    }
    pod->set_stage(kStageRemoved);
    SendKillToAgent(pod);
    return true;
}


bool JobManager::HandleDoNothing(PodStatus*, Job*) {
    return true;
}

void JobManager::SendKillToAgent(PodStatus*  pod) {
    mutex_.AssertHeld();
    if (pod == NULL) {
        return;
    }
    std::map<AgentAddr, AgentInfo*>::iterator a_it =
      agents_.find(pod->endpoint());
    if (a_it == agents_.end()) {
        LOG(WARNING, "no such agent %s", pod->endpoint().c_str());
        return;
    }
    AgentInfo* agent_info = agents_[pod->endpoint()]; 
    if (agent_info->state() == kDead) {
        return;
    }
    // tell agent to clean pod
    KillPodRequest* request = new KillPodRequest;
    KillPodResponse* response = new KillPodResponse;
    request->set_podid(pod->podid());
    Agent_Stub* stub;
    rpc_client_.GetStub(pod->endpoint(), &stub);
    boost::function<void (const KillPodRequest*, KillPodResponse*, bool, int)> kill_pod_callback;
    kill_pod_callback = boost::bind(&JobManager::KillPodCallback, this, pod,
                                       _1, _2, _3, _4);
    rpc_client_.AsyncRequest(stub, &Agent_Stub::KillPod, request, response,
                                 kill_pod_callback, FLAGS_master_agent_rpc_timeout, 0);
    delete stub;
}

}
}
