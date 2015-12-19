// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scheduler/scheduler.h"

#include <math.h>
#include <algorithm>
#include <gflags/gflags.h>
#include "logging.h"
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "utils/resource_utils.h"
#include "proto/master.pb.h"

DECLARE_int32(scheduler_feasibility_factor);
DECLARE_double(scheduler_cpu_used_factor);
DECLARE_double(scheduler_mem_used_factor);
DECLARE_double(scheduler_prod_count_factor);
DECLARE_double(scheduler_cpu_overload_threashold);
DECLARE_int32(scheduler_agent_overload_turns_threashold);

namespace baidu {
namespace galaxy {

double Scheduler::CalcLoad(const AgentInfo& agent) {
    double cpu_load = agent.used().millicores() * FLAGS_scheduler_cpu_used_factor /
            agent.total().millicores();
    double mem_load = agent.used().memory() * FLAGS_scheduler_mem_used_factor /
            agent.total().memory();
    double prod_load = agent.pods_size() / FLAGS_scheduler_prod_count_factor;
    return exp(cpu_load) + exp(mem_load) + exp(prod_load);
}

// @brief 按照priority降序排序
static bool JobCompare(const JobInfo& left, const JobInfo& right) {
    return left.desc().priority() > right.desc().priority();
}


static bool ScoreCompare(const std::pair<double, std::string>& left,
                         const std::pair<double, std::string>& right) {
    return left.first > right.first;
}

static bool AgentCompare(const std::pair<double, std::string>& left,
                         const std::pair<double, std::string>& right) {
    return left.first < right.first;
}

PodScaleCell::PodScaleCell(){}
PodScaleCell::~PodScaleCell(){}

void Scheduler::Propose(boost::shared_ptr<PodScaleCell> cell,
                        const std::string& master_addr) {
    cell->Score();
    std::vector<ScheduleInfo> propose_pods;
    cell->Propose(&propose_pods);
    LOG(INFO, "propose sched info count %d", propose_pods.size());
    Master_Stub* master_stub;
    bool ok = rpc_client_.GetStub(master_addr, &master_stub);
    if (!ok) {
        LOG(WARNING, "fail to prepare master stub");
        return;
    }
    ProposeRequest* request = new ProposeRequest();
    ProposeResponse* response = new ProposeResponse();
    for (std::vector<ScheduleInfo>::iterator it = propose_pods.begin();
         it != propose_pods.end(); ++it) {
        ScheduleInfo* sched = request->add_schedule();
        sched->CopyFrom(*it);
    }
    boost::function<void (const ProposeRequest*, ProposeResponse*, bool, int)> call_back;
    call_back = boost::bind(&Scheduler::ProposeCallBack, this, _1, _2, _3, _4);
    rpc_client_.AsyncRequest(master_stub,
                            &Master_Stub::Propose,
                            request,
                            response,
                            call_back,
                            5, 1);
    delete master_stub;
}

void Scheduler::ProposeCallBack(const ProposeRequest* request,
                                ProposeResponse* response,
                                bool failed, int) {
    std::string level = "unsuccessfully";
    if (!failed && response->status() == kOk) {
        level = "successfully";
    }
    for (int i = 0; i < request->schedule_size(); i++) {
        LOG(INFO, "propose %s pod %s of job %s on %s %s",
                ScheduleAction_Name(request->schedule(i).action()).c_str(),
                request->schedule(i).podid().c_str(),
                request->schedule(i).jobid().c_str(),
                request->schedule(i).endpoint().c_str(),
                level.c_str());
    }
    delete request;
    delete response;
}

void Scheduler::ScheduleScaleUp(const std::string& master_addr,
                                std::vector<JobInfo>& pending_jobs) {
    MutexLock lock(&sched_mutex_);
    std::vector<boost::shared_ptr<PodScaleUpCell> > pending_pods;
    // sort job by priority
    int32_t all_pod_needs = ChoosePods(pending_jobs, pending_pods);
    // shuffle agent
    boost::unordered_map<std::string, AgentInfo>::iterator it = resources_->begin();
    std::vector<std::string> keys;
    for (; it != resources_->end(); ++it) {
        keys.push_back(it->first);
    }
    Shuffle(keys);
    std::vector<std::string>::iterator key_it = keys.begin();
    int cur_pod_count = 0;
    boost::unordered_map<std::string, AgentInfo> changed_resource;
    for (; key_it != keys.end() &&
         cur_pod_count < all_pod_needs; ++key_it) {
        AgentInfo agent = resources_->find(*key_it)->second;
        bool changed = false;
        for (std::vector<boost::shared_ptr<PodScaleUpCell> >::iterator pod_it = pending_pods.begin();
                pod_it != pending_pods.end(); ++pod_it) {
            boost::shared_ptr<PodScaleUpCell> cell = *pod_it;
            LOG(INFO, "checking: %s on %s",
                    cell->job.jobid().c_str(),
                    agent.endpoint().c_str());
            if (cell->feasible.size() >= cell->feasible_limit
                && !cell->proposed) {
                LOG(INFO, "feasibility checking done for %s",
                        cell->job.jobid().c_str());
                thread_pool_.AddTask(boost::bind(&Scheduler::Propose, this, cell, master_addr));
                cell->proposed = true;
                continue;
            } else {
                bool ok =cell->AllocFrom(&agent);
                LOG(INFO, "feasibility checking %s on %s return %d",
                        cell->job.jobid().c_str(),
                        agent.endpoint().c_str(), (int)ok);
                if (ok) {
                    cell->feasible.push_back(agent);
                    cur_pod_count++;
                    changed = true;
                }
            }
        }
        if (changed) {
            changed_resource.insert(std::make_pair(*key_it, agent));
        }
    }

    std::vector<JobInfo> need_preempt_jobs;
    for (size_t i = 0; i < pending_pods.size(); i++) {
        boost::shared_ptr<PodScaleUpCell> cell = pending_pods[i];
        if (cell->proposed) {
            continue;
        }
        if (cell->feasible.size() <=0) {
            LOG(INFO, "job %s has no feasibie agent to deploy", cell->job.jobid().c_str());
            if (cell->job.desc().type() == kLongRun) {
                LOG(INFO, "move job %s to preempt queue", cell->job.jobid().c_str());
                need_preempt_jobs.push_back(cell->job);
            }
            continue;
        }
        thread_pool_.AddTask(boost::bind(&Scheduler::Propose, this, cell, master_addr));
    }
    SchedulePreempt(master_addr, 
                    changed_resource,
                    need_preempt_jobs);

}


bool PodPreemptCell::TryPreempt(AgentInfo* agent_info, boost::unordered_map<std::string, JobDescriptor>* jobs) {
    if (agent_info == NULL) {
        return false;
    }
    std::set<std::string> labels_set;
    for (int i = 0; i < agent_info->tags_size(); i++) {
        labels_set.insert(agent_info->tags(i));
    } 
    if (pod.labels_size() > 0) {
        std::string label = pod.labels(0);
        if (!label.empty() && labels_set.find(label) 
             == labels_set.end()) {
            LOG(INFO, "agent %s with version %d does not fit job %s label check %s", 
                      agent_info->endpoint().c_str(),
                      agent_info->version(),
                      job.jobid().c_str(),
                      label.c_str());
            return false;
        }
    }
    AgentInfo copied_agent = *agent_info;
    preempted_pods.clear();
    std::vector<Resource> alloc_set;
    for (int i = 0; i < agent_info->pods_size(); i++) {
        const PodStatus& ipod = agent_info->pods(i);
        boost::unordered_map<std::string, JobDescriptor>::iterator job_it = jobs->find(ipod.jobid());
        if (job_it == jobs->end() || job_it->second.type() != kBatch) {
            continue;
        }
        LOG(INFO, "release pod %s resource for pod %s", ipod.podid().c_str(), podid.c_str());
        ResourceUtils::DeallocResource(job_it->second.pod().requirement(), &copied_agent);
        PreemptEntity entity;
        entity.set_podid(ipod.podid());
        entity.set_jobid(ipod.jobid());
        preempted_pods.push_back(entity);
        bool ok = ResourceUtils::AllocResource(pod.requirement(), alloc_set, &copied_agent);
        if (ok) {
            LOG(INFO, "agent %s satisfy pod %s requirement after release source ", copied_agent.endpoint().c_str(), podid.c_str());
            agent = copied_agent.endpoint();
            return ok;
        }
    }
    return false;
}

void Scheduler::SchedulePreempt(const std::string& master_addr,
                                 const boost::unordered_map<std::string, AgentInfo>& changed_resource,
                                 std::vector<JobInfo>& jobs) {
    sched_mutex_.AssertHeld();
    LOG(INFO, "start preemption process job count %u changed agents %u", jobs.size(), changed_resource.size());
    boost::unordered_map<std::string, AgentInfo>::iterator it = resources_->begin();
    std::vector<std::string> keys;
    for (; it != resources_->end(); ++it) {
        keys.push_back(it->first);
    }
    Shuffle(keys);
    std::vector<std::string>::iterator key_it = keys.begin();
    std::vector<boost::shared_ptr<PodPreemptCell> > cells;
    for (size_t i = 0; i < jobs.size(); i++) {
        std::vector<std::string> job_pending_pods;
        uint32_t deploying_num = 0;
        for (int j = 0; j < jobs[i].pods_size(); j++) {
            if (jobs[i].pods(j).state() == kPodDeploying) {
                deploying_num ++;
                continue;
            }
            if (jobs[i].pods(j).stage() == kStagePending) {
                job_pending_pods.push_back(jobs[i].pods(j).podid());
            }
        }
        uint32_t max_deploy_pods = job_pending_pods.size();
        if (jobs[i].desc().deploy_step() > 0 && 
            (uint32_t)jobs[i].desc().deploy_step() < max_deploy_pods) { 
            max_deploy_pods = jobs[i].desc().deploy_step();
        }
        if (job_pending_pods.size() <= 0 
           || deploying_num >= max_deploy_pods) {
            continue;
        }
        boost::shared_ptr<PodPreemptCell> cell(new PodPreemptCell);
        cell->job = jobs[i];
        for (uint32_t j = 0; j < job_pending_pods.size() && deploying_num < max_deploy_pods; j++) {
            cell->podid = job_pending_pods[j];
            break;
        }
        cell->preempted = false;
        PodDescriptor pod_desc;
        bool find_desc = false;
        for (int j = 0; j < cell->job.pod_descs_size(); ++j) {
            if (cell->job.pod_descs(j).version() != cell->job.latest_version()) {
                continue;
            }
            pod_desc.CopyFrom(cell->job.pod_descs(j));
            find_desc = true;
            break;
        }
        if (find_desc) {
            cell->pod = pod_desc;
            cells.push_back(cell);
        }
    }
    uint32_t total_count = 0;
    for (; key_it != keys.end(); ++key_it) {
        if (total_count >= cells.size()) {
            break;
        }
        LOG(INFO, "try preempt on agent %s", (*key_it).c_str());
        AgentInfo agent = resources_->find(*key_it)->second;
        boost::unordered_map<std::string, AgentInfo>::const_iterator changed_it = changed_resource.find(*key_it);
        if (changed_it !=  changed_resource.end()) {
            agent = changed_it->second;
        }
        if (!CanPreempt(agent)) {
            LOG(INFO, "agent %s can not be preempted", (*key_it).c_str());
            continue;
        }
        for (size_t i = 0; i < cells.size(); i++) {
            boost::shared_ptr<PodPreemptCell> cell = cells[i];
            if (cell->preempted) {
                continue;
            }
            bool ok = cell->TryPreempt(&agent, jobs_);
            if (ok) {
                LOG(INFO ,"pod %s of job %s preempt on agent %s",
                        cell->podid.c_str(), cell->job.jobid().c_str(), cell->agent.c_str());
                Preempt(cell, master_addr);
                cell->preempted = true;
                total_count ++;
            }else {
                LOG(WARNING, "agent %s can no be preempted by pod %s", (*key_it).c_str(), cell->podid.c_str());
            }
        }
    }
}

void Scheduler::PreemptCallBack(const PreemptRequest* request,
                         PreemptResponse* response,
                         bool failed, int) {
    delete request;
    delete response;

}

void Scheduler::Preempt(boost::shared_ptr<PodPreemptCell> cell,
                        const std::string& master_addr) {
    Master_Stub* master_stub;
    bool ok = rpc_client_.GetStub(master_addr, &master_stub);
    if (!ok) {
        LOG(WARNING, "fail to prepare master stub");
        return;
    }

    PreemptRequest* request = new PreemptRequest();
    PreemptResponse* response = new PreemptResponse();
    PreemptEntity* pending_pod = request->mutable_pending_pod();
    pending_pod->set_jobid(cell->job.jobid());
    pending_pod->set_podid(cell->podid);
    for (size_t i = 0; i < cell->preempted_pods.size(); i++) {
        PreemptEntity* preempt_pod = request->add_preempted_pods();
        preempt_pod->set_jobid(cell->preempted_pods[i].jobid());
        preempt_pod->set_podid(cell->preempted_pods[i].podid());
    }
    request->set_addr(cell->agent);
    boost::function<void (const PreemptRequest*, PreemptResponse*, bool, int)> call_back;
    call_back = boost::bind(&Scheduler::PreemptCallBack, this, _1, _2, _3, _4);
    rpc_client_.AsyncRequest(master_stub,
                            &Master_Stub::Preempt,
                            request,
                            response,
                            call_back,
                            5, 1);
    delete master_stub;

}

bool Scheduler::CanPreempt(const AgentInfo& agent_info) {
    sched_mutex_.AssertHeld();
    for (int i = 0; i <agent_info.pods_size(); i++) {
        const PodStatus& pod = agent_info.pods(i);
        boost::unordered_map<std::string, JobDescriptor>::iterator job_it = jobs_->find(pod.jobid());
        if (job_it !=  jobs_->end() && job_it->second.type() == kBatch) {
            return true;
        }
    }
    return false;
}

void Scheduler::ScheduleScaleDown(const std::string& master_addr,
                                  std::vector<JobInfo>& reducing_jobs) {
    MutexLock lock(&sched_mutex_);
    std::vector<boost::shared_ptr<PodScaleDownCell> > reducing_pods;
    int32_t total_reducing_count = ChooseReducingPod(reducing_jobs,
                                                     reducing_pods);

    if (total_reducing_count < 0) {
        return;
    }

    for (std::vector<boost::shared_ptr<PodScaleDownCell> >::iterator pod_it = reducing_pods.begin();
            pod_it != reducing_pods.end(); ++pod_it) {
        boost::shared_ptr<PodScaleDownCell> cell = *pod_it;
        if (cell->scale_down_count <= 0) {
            continue;
        }
        thread_pool_.AddTask(boost::bind(&Scheduler::Propose, this, cell, master_addr));
    }

}

int32_t Scheduler::SyncResources(const GetResourceSnapshotResponse* response) {
    MutexLock lock(&sched_mutex_);
    LOG(INFO, "sync resource from master, update agent_info count %d, deleted count %d", 
             response->agents_size(),
             response->deleted_agents_size());
    for (int32_t i = 0; i < response->deleted_agents_size(); i++) {
        boost::unordered_map<std::string, AgentInfo>::iterator it =
             resources_->find(response->deleted_agents(i));
        if (it != resources_->end()) {
            resources_->erase(it->first);
        }
    }

    for (int32_t i =0 ; i < response->agents_size(); i++) {
        const AgentInfo& agent = response->agents(i);
        boost::unordered_map<std::string, AgentInfo>::iterator it = resources_->find(agent.endpoint());
        if (it == resources_->end()) {
            resources_->insert(std::make_pair(agent.endpoint(), agent));
        }else {
            it->second.CopyFrom(agent);
        }
        LOG(INFO, "sync agent %s with version %d stat:mem total %ld, cpu total %d, mem assigned %ld, cpu assinged %d",
              agent.endpoint().c_str(), agent.version(),
              agent.total().memory(), agent.total().millicores(),
              agent.assigned().memory(), agent.assigned().millicores());
    }
    return resources_->size();
}

int32_t Scheduler::ChoosePods(std::vector<JobInfo>& pending_jobs,
                              std::vector<boost::shared_ptr<PodScaleUpCell> >& pending_pods) {

    // 按照Job优先级进行排序
    std::sort(pending_jobs.begin(), pending_jobs.end(), JobCompare);
    std::vector<JobInfo>::const_iterator job_it = pending_jobs.begin();
    int32_t all_pod_needs = 0;
    for (; job_it != pending_jobs.end(); ++job_it) {
        std::vector<std::string> job_pending_pods;
        uint32_t deploying_num = 0;
        for (int i = 0; i < job_it->pods_size(); i++) {
            // use pod state 
            if (job_it->pods(i).state() == kPodDeploying) {
                deploying_num ++;
                continue;
            }
            if (job_it->pods(i).stage() == kStagePending) {
                job_pending_pods.push_back(job_it->pods(i).podid());
            }
        }
        uint32_t max_deploy_pods = job_pending_pods.size();
        LOG(INFO, "job %s deploy stat: pending_size %u, deploying_num %u, max_deploy_pods %u, step size %u",
          job_it->jobid().c_str(), job_pending_pods.size(), deploying_num, max_deploy_pods, 
          job_it->desc().deploy_step());
        if (job_pending_pods.size() <= 0 
           || deploying_num >= (uint32_t)job_it->desc().deploy_step()) {
            continue;
        }
        std::vector<std::string> need_schedule;
        for (uint32_t i = 0; 
            i < job_pending_pods.size()
            && deploying_num < (uint32_t)job_it->desc().deploy_step()
            && deploying_num < max_deploy_pods;
            i++) {
            need_schedule.push_back(job_pending_pods[i]);
            deploying_num++;
        }
        PodDescriptor pod_desc;
        bool find_desc = false;
        for (int i = 0; i < job_it->pod_descs_size(); ++i) {
            if (job_it->pod_descs(i).version() != job_it->latest_version()) {
                continue;
            }
            pod_desc.CopyFrom(job_it->pod_descs(i));
            find_desc = true;
            break;
        }
        if (!find_desc) {
            LOG(WARNING, "fail to get job %s latest version %s pod descripto",
              job_it->jobid().c_str(), job_it->latest_version().c_str());
            continue;
        }
        boost::shared_ptr<PodScaleUpCell> need_schedule_cell(new PodScaleUpCell());
        need_schedule_cell->proposed = false;
        need_schedule_cell->pod = pod_desc;
        need_schedule_cell->job.CopyFrom(*job_it);
        need_schedule_cell->pod_ids = need_schedule;
        need_schedule_cell->feasible_limit = need_schedule.size() * FLAGS_scheduler_feasibility_factor;
        all_pod_needs += need_schedule_cell->feasible_limit;
        pending_pods.push_back(need_schedule_cell); 
        LOG(INFO, "scheduler choosed %u pod to deploy for job %s" , need_schedule.size(), job_it->jobid().c_str());
    }
    LOG(INFO, "total %d pods need be scheduled", all_pod_needs);
    return all_pod_needs;
}

int32_t Scheduler::ChooseReducingPod(std::vector<JobInfo>& reducing_jobs,
                                     std::vector<boost::shared_ptr<PodScaleDownCell> >& reducing_pods) {
    sched_mutex_.AssertHeld();
    int reducing_count = 0;
    std::vector<JobInfo>::const_iterator job_it = reducing_jobs.begin();
    for (; job_it != reducing_jobs.end(); ++job_it) {
        boost::shared_ptr<PodScaleDownCell> cell(new PodScaleDownCell());
        cell->pod = job_it->desc().pod();
        cell->job = *job_it;
        for (int i = 0; i < job_it->pods_size(); ++i) {
            boost::unordered_map<std::string, AgentInfo>::iterator agt_it =
                    resources_->find(job_it->pods(i).endpoint());
            if (agt_it == resources_->end()) {
                LOG(INFO, "scale down pod %s dose not belong to agent %s",
                        job_it->pods(i).podid().c_str(),
                        job_it->pods(i).endpoint().c_str());
            }
            else {
                LOG(INFO, "scale down pod %s  agent %s",
                        job_it->pods(i).podid().c_str(),
                        job_it->pods(i).endpoint().c_str());
                cell->pod_agent_map.insert(
                        std::make_pair(job_it->pods(i).podid(), agt_it->second));
            }
        }
        int32_t scale_down_count = job_it->pods_size() - cell->job.desc().replica();
        cell->scale_down_count = scale_down_count;
        reducing_count += scale_down_count;
        LOG(INFO, "job %s scale down count %d with pod_size %d replica %d", job_it->jobid().c_str(), scale_down_count,
                job_it->pods_size(), cell->job.desc().replica());
        reducing_pods.push_back(cell);
    }
    return reducing_count;
}

void Scheduler::BuildSyncRequest(GetResourceSnapshotRequest* request) {
    MutexLock lock(&sched_mutex_);
    boost::unordered_map<std::string, AgentInfo>::iterator it =
      resources_->begin();
    for (; it != resources_->end(); ++it) {
        DiffVersion* version = request->add_versions();
        version->set_version(it->second.version());
        version->set_endpoint(it->second.endpoint());
    }
}

PodScaleUpCell::PodScaleUpCell():pod(), job(),
        schedule_count(0), feasible_limit(0) {}
PodScaleUpCell::~PodScaleUpCell(){}
bool PodScaleUpCell::AllocFrom(AgentInfo* agent_info) {

    std::set<std::string> labels_set;
    for (int i = 0; i < agent_info->tags_size(); i++) {
        labels_set.insert(agent_info->tags(i));
    }
    // TODO for simple only check first label
    if (pod.labels_size() > 0) {
        std::string label = pod.labels(0);
        if (!label.empty() && labels_set.find(label) 
             == labels_set.end()) {
            LOG(INFO, "agent %s with version %d does not fit job %s label check %s", 
                      agent_info->endpoint().c_str(),
                      agent_info->version(),
                      job.jobid().c_str(),
                      label.c_str());
            return false;
        }
    }
    std::vector<Resource> alloc_set;
    bool ok = ResourceUtils::AllocResource(pod.requirement(),
                                           alloc_set,
                                           agent_info);
    return ok; 
}

void PodScaleUpCell::Score() {
    std::vector<AgentInfo>::iterator agt_it = feasible.begin();
    for(; agt_it != feasible.end(); ++agt_it) {
        const AgentInfo& agent = *agt_it;
        double score = ScoreAgent(agent, pod);
        sorted.push_back(std::make_pair(score, agent.endpoint()));
    }
    std::sort(sorted.begin(), sorted.end(), AgentCompare);
}

void PodScaleUpCell::Propose(std::vector<ScheduleInfo>* propose) {
    std::vector<std::pair<double, std::string> >::iterator sorted_it = sorted.begin();
    for (size_t i = 0; i < pod_ids.size(); ++i) {
        if (sorted_it == sorted.end()) {
            break;
        }
        else {
            ScheduleInfo sched;
            sched.set_endpoint(sorted_it->second);
            sched.set_podid(pod_ids[i]);
            sched.set_jobid(job.jobid());
            sched.set_action(kLaunch);
            propose->push_back(sched);
            LOG(INFO, "propose %s:%s on %s",
                    sched.jobid().c_str(),
                    sched.podid().c_str(),
                    sched.endpoint().c_str());
            ++sorted_it;
        }
    }
}

double PodScaleUpCell::ScoreAgent(const AgentInfo& agent_info,
                                  const PodDescriptor& /*desc*/) {
    double score = Scheduler::CalcLoad(agent_info);
    LOG(DEBUG, "score %s %lf", agent_info.endpoint().c_str(), score);
    return score;
}

PodScaleDownCell::PodScaleDownCell() : pod(), job(), scale_down_count(0) {}
PodScaleDownCell::~PodScaleDownCell(){}
void PodScaleDownCell::Score() {
    std::map<std::string, AgentInfo>::iterator pod_agt_it = pod_agent_map.begin();
    for(; pod_agt_it != pod_agent_map.end(); ++pod_agt_it) {
        double score = ScoreAgent(pod_agt_it->second, pod);
        sorted_pods.push_back(std::make_pair(score, pod_agt_it->first));
    }
    std::sort(sorted_pods.begin(), sorted_pods.end(), ScoreCompare);
}

double PodScaleDownCell::ScoreAgent(const AgentInfo& agent_info,
                   const PodDescriptor& /*desc*/) {
    double score = Scheduler::CalcLoad(agent_info);
    LOG(DEBUG, "score %s %lf", agent_info.endpoint().c_str(), score);
    return -1 * score;
}

void PodScaleDownCell::Propose(std::vector<ScheduleInfo>* propose) {
    std::map<std::string, AgentInfo>::iterator pod_agent_it;
    std::vector<std::pair<double, std::string> >::iterator sorted_it = sorted_pods.begin();
    for (size_t i = 0; i < scale_down_count; ++i) {
        if (sorted_it == sorted_pods.end()) {
            break;
        }
        else {
            pod_agent_it = pod_agent_map.find(sorted_it->second);
            if (pod_agent_it == pod_agent_map.end()) {
                continue;
            }
            ScheduleInfo sched;
            sched.set_endpoint(pod_agent_it->second.endpoint());
            sched.set_podid(sorted_it->second);
            sched.set_jobid(job.jobid());
            sched.set_action(kTerminate);
            propose->push_back(sched);
            LOG(INFO, "scale down %s:%s on %s",
                    sched.jobid().c_str(),
                    sched.podid().c_str(),
                    sched.endpoint().c_str());
            ++sorted_it;
        }
    }
}

int32_t Scheduler::UpdateAgent(const AgentInfo* agent_info) {
    boost::unordered_map<std::string, AgentInfo>::iterator agt_it =
      resources_->find(agent_info->endpoint());
    if (agt_it != resources_->end()) {
        LOG(INFO, "update agent %s", agent_info->endpoint().c_str());
        agt_it->second.CopyFrom(*agent_info);
        return 0;
    }
    else {
        LOG(INFO, "update agent %s not exists", agent_info->endpoint().c_str());
        return 1;
    }
}


void Scheduler::ScheduleUpdate(const std::string& master_addr,
                               std::vector<JobInfo>& update_jobs) {
    std::vector<JobInfo>::const_iterator job_it = update_jobs.begin();
    for(; job_it != update_jobs.end(); ++job_it) {
        const JobInfo& job_info = *job_it;
        boost::shared_ptr<PodUpdateCell> cell(new PodUpdateCell());
        cell->job = job_info;
        thread_pool_.AddTask(boost::bind(&Scheduler::Propose, this, cell, master_addr));
    }
}

void PodUpdateCell::Score() {}
void PodUpdateCell::Propose(std::vector<ScheduleInfo>* propose) {
    std::vector<std::pair<std::string, std::string> > ineed_update_pods;
    int32_t updating_count = 0;
    for (int32_t i = 0; i < job.pods_size(); i++) {
        const PodStatus& pod = job.pods(i);
        if (pod.version() != job.latest_version()) {
            ineed_update_pods.push_back(std::make_pair(pod.endpoint(), pod.podid()));
        }
        if ( pod.stage() == kStageDeath
           || pod.state() == kPodDeploying) {
            updating_count++;
        }
    }
    LOG(INFO, "job %s update stat: updating_count %d, update_step_size %d, need_update_size %u",
        job.jobid().c_str(), updating_count, job.desc().deploy_step(), ineed_update_pods.size());

    for (size_t i = 0; i < ineed_update_pods.size() && updating_count < job.desc().deploy_step() ; i++ ) {
        updating_count++;
        ScheduleInfo sched_info;
        sched_info.set_endpoint(ineed_update_pods[i].first);
        sched_info.set_jobid(job.jobid());
        sched_info.set_podid(ineed_update_pods[i].second);
        sched_info.set_action(kTerminate);
        propose->push_back(sched_info);
        LOG(INFO, "choose agent %s update pod %s of job %s", ineed_update_pods[i].first.c_str(),
           ineed_update_pods[i].second.c_str(), job.jobid().c_str());

    }
}


void Scheduler::BuildSyncJobRequest(GetJobDescriptorRequest* request) {
    MutexLock lock(&sched_mutex_);
    boost::unordered_map<std::string, JobDescriptor>::iterator it = jobs_->begin();
    for (; it !=  jobs_->end(); ++it) {
        JobIdDiff* diff = request->mutable_jobs()->Add();
        diff->set_jobid(it->first);
        diff->set_version(it->second.pod().version());
    }
}

void Scheduler::SyncJobDescriptor(const GetJobDescriptorResponse* response) {
    MutexLock lock(&sched_mutex_);
    for (int i = 0; i < response->jobs_size(); ++i) {
        LOG(INFO, "update job %s desc type %s", response->jobs(i).jobid().c_str(), JobType_Name(response->jobs(i).desc().type()).c_str());
        boost::unordered_map<std::string, JobDescriptor>::iterator it = jobs_->find(response->jobs(i).jobid());
        if (it == jobs_->end()) {
            jobs_->insert(std::make_pair(response->jobs(i).jobid(), response->jobs(i).desc()));
        }else {
            it->second = response->jobs(i).desc();
        }
    }
    for (int i = 0; i < response->deleted_jobs_size(); ++i) {
        LOG(INFO, "delete job %s desc", response->deleted_jobs(i).c_str());
        jobs_->erase(response->deleted_jobs(i));
    }
}

}// galaxy
}// baidu
