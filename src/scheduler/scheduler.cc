// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scheduler/scheduler.h"

#include <math.h>
#include <algorithm>
#include <gflags/gflags.h>
#include "logging.h"
#include "utils/resource_utils.h"

DECLARE_int32(scheduler_feasibility_factor);
DECLARE_double(scheduler_cpu_used_factor);
DECLARE_double(scheduler_mem_used_factor);
DECLARE_double(scheduler_prod_count_factor);
DECLARE_double(scheduler_cpu_overload_threashold);
DECLARE_int32(scheduler_agent_overload_turns_threashold);

namespace baidu {
namespace galaxy {

double Scheduler::CalcLoad(const AgentInfo* agent) {
    double cpu_load = agent->used().millicores() * FLAGS_scheduler_cpu_used_factor /
            agent->total().millicores();
    double mem_load = agent->used().memory() * FLAGS_scheduler_mem_used_factor /
            agent->total().memory();
    double prod_load = agent->pods_size() / FLAGS_scheduler_prod_count_factor;
    return exp(cpu_load) + exp(mem_load) + exp(prod_load);
}

// @brief 按照磁盘可用大小升序排列
static bool VolumeCompare(const Volume& volume1, const Volume& volume2) {
    return volume1.quota() < volume2.quota();
}

// @brief 按照priority降序排序
static bool JobCompare(const JobInfo* left, const JobInfo* right) {
    return left->desc().priority() > right->desc().priority();
}


static bool ScoreCompare(const std::pair<double, std::string>& left,
                         const std::pair<double, std::string>& right) {
    return left.first > right.first;
}

static bool AgentCompare(const std::pair<double, AgentInfo*>& left,
                         const std::pair<double, AgentInfo*>& right) {
    return left.first < right.first;
}

int32_t Scheduler::ScheduleScaleUp(std::vector<JobInfo*>& pending_jobs,
                                   std::vector<ScheduleInfo*>* propose) {
    LOG(INFO, "schedule scale up turns: %lld", schedule_turns_);
    ++schedule_turns_;

    int propose_count = 0;
    // 计算job优先级，及其需要调度的pod数量
    std::vector<PodScaleUpCell*> pending_pods;
    // pod 依照优先级进行排序
    int32_t all_agent_needs = ChoosePods(pending_jobs, &pending_pods);
    // shuffle resources_
    std::vector<AgentInfoExtend*> resources_to_alloc;
    ChooseResourse(&resources_to_alloc);

    // 计算feasibility
    std::vector<AgentInfoExtend*>::iterator res_it = resources_to_alloc.begin();
    int cur_feasible_count = 0;
    for (; res_it != resources_to_alloc.end() &&
            cur_feasible_count < all_agent_needs; ++res_it) {
        for (std::vector<PodScaleUpCell*>::iterator pod_it = pending_pods.begin();
                pod_it != pending_pods.end(); ++pod_it) {
            LOG(INFO, "checking: %s on %s",
                    (*pod_it)->job->jobid().c_str(),
                    (*res_it)->agent_info->endpoint().c_str());
            if ((*pod_it)->feasible.size() >= (*pod_it)->feasible_limit ) {
                LOG(INFO, "feasibility checking done for %s",
                        (*pod_it)->job->jobid().c_str());
                continue;
            } else {
                bool check = (*pod_it)->FeasibilityCheck(*res_it);
                LOG(INFO, "feasibility checking %s on %s return %d",
                        (*pod_it)->job->jobid().c_str(),
                         (*res_it)->agent_info->endpoint().c_str(), (int)check);
                if (check == true) {
                    (*pod_it)->feasible.push_back(*res_it);
                    cur_feasible_count++;
                }
                // 此处不break，说明一个Agent尽量调度多的Pod
                // TODO 此处需要再斟酌一下，对于新添入机器会出现负载较高的情况
                // 但是如果直接break的话，对于符合某个pod的所有机器对其他pod不可见的问题
                // 同样需要考虑一下
            }
        }
    }
    LOG(INFO, " PodScaleUpCell count %u", pending_pods.size());
    // 对增加实例任务进行优先级计算
    for (std::vector<PodScaleUpCell*>::iterator pod_it = pending_pods.begin();
            pod_it != pending_pods.end(); ++pod_it) {
        (*pod_it)->Score();
        uint32_t count = (*pod_it)->Propose(propose);
        propose_count += count;
        LOG(INFO, "propose jobid %s count %u", (*pod_it)->job->jobid().c_str(), count);
    }

    // 销毁PodScaleUpCell
    for (std::vector<PodScaleUpCell*>::iterator pod_it = pending_pods.begin();
            pod_it != pending_pods.end(); ++pod_it) {
        delete *pod_it;
    }
    return propose_count;
}

int32_t Scheduler::ChooseResourse(std::vector<AgentInfoExtend*>* resources_to_alloc) {
    boost::unordered_map<std::string, AgentInfoExtend*>::iterator it = resources_.begin();
    std::vector<std::string> keys;
    for (; it != resources_.end(); ++it) {
        keys.push_back(it->first);
    }
    Shuffle(keys);
    std::vector<std::string>::iterator k_it = keys.begin();
    for (; k_it != keys.end(); ++k_it) {
        AgentInfoExtend* agent = resources_[*k_it];
        resources_to_alloc->push_back(agent);
        LOG(DEBUG, "agent resource unassigned: millicores %d memory %d  #disk %u #ssd %u",
                agent->unassigned.millicores(), agent->unassigned.memory(),
                agent->unassigned.disks_size(), agent->unassigned.ssds_size());
    }
    return 0;
}

int32_t Scheduler::ScheduleScaleDown(std::vector<JobInfo*>& reducing_jobs,
                                     std::vector<ScheduleInfo*>* propose) {
    int propose_count = 0;
    // 计算job优先级，及其需要调度的pod数量
    std::vector<PodScaleDownCell*> reducing_pods;

    int32_t total_reducing_count = ChooseReducingPod(reducing_jobs, &reducing_pods);

    if (total_reducing_count < 0) {
        return 0;
    }

    // 对减少实例任务进行优先级计算
    for (std::vector<PodScaleDownCell*>::iterator pod_it = reducing_pods.begin();
            pod_it != reducing_pods.end(); ++pod_it) {
        (*pod_it)->Score();
        propose_count += (*pod_it)->Propose(propose);
    }

    // 销毁PodScaleDownCell
    for (std::vector<PodScaleDownCell*>::iterator pod_it = reducing_pods.begin();
            pod_it != reducing_pods.end(); ++pod_it) {
        delete *pod_it;
    }
    return propose_count;
}

int32_t Scheduler::SyncResources(const GetResourceSnapshotResponse* response) {
   LOG(INFO, "sync resource from master, update agent_info count %d, deleted count %d", 
             response->agents_size(),
             response->deleted_agents_size());
   for (int32_t i = 0; i < response->deleted_agents_size(); i++) {
       boost::unordered_map<std::string, AgentInfoExtend*>::iterator it =
         resources_.find(response->deleted_agents(i));
       if (it != resources_.end()) {
           delete it->second;
           resources_.erase(it->first);
       }
   }
   for (int32_t i =0 ; i < response->agents_size(); i++) {
       boost::unordered_map<std::string, AgentInfoExtend*>::iterator it =
         resources_.find(response->agents(i).endpoint());
       AgentInfoExtend* agent_ext = NULL;
       if (it == resources_.end()) {
           AgentInfo* agent = new AgentInfo();
           agent->CopyFrom(response->agents(i));
           agent_ext = new AgentInfoExtend(agent);
           agent_ext->CalcExtend();
           resources_.insert(std::make_pair(agent->endpoint(), agent_ext));
       }else {
           agent_ext = it->second;
           agent_ext->agent_info->CopyFrom(response->agents(i));
           // re calc free and unassigned
           agent_ext->CalcExtend();
       }
       LOG(INFO, "sync agent %s with version %d stat:mem total %ld, cpu total %d, mem assigned %ld, cpu assinged %d, mem used %ld, cpu used %d",
              agent_ext->agent_info->endpoint().c_str(), agent_ext->agent_info->version(),
              agent_ext->agent_info->total().memory(), agent_ext->agent_info->total().millicores(),
              agent_ext->agent_info->assigned().memory(), agent_ext->agent_info->assigned().millicores(),
              agent_ext->agent_info->used().memory(), agent_ext->agent_info->used().millicores());

   }
   return resources_.size();
}

int32_t Scheduler::ChoosePods(std::vector<JobInfo*>& pending_jobs,
                              std::vector<PodScaleUpCell*>* pending_pods) {

    // 按照Job优先级进行排序
    std::sort(pending_jobs.begin(), pending_jobs.end(), JobCompare);
    std::vector<JobInfo*>::iterator job_it = pending_jobs.begin();
    int32_t all_agent_needs = 0;
    for (; job_it != pending_jobs.end(); ++job_it) {
        JobInfo* job = *job_it;
        std::vector<PodStatus> job_pending_pods;
        uint32_t deploying_num = 0;
        for (int i = 0; i < job->pods_size(); i++) {
            // use pod state 
            if (job->pods(i).state() == kPodDeploying) {
                deploying_num ++;
                continue;
            }
            if (job->pods(i).stage() == kStagePending) {
                job_pending_pods.push_back(job->pods(i));
            }
        }
        uint32_t max_deploy_pods = job_pending_pods.size();
        if (job->desc().deploy_step() > 0 && 
            (uint32_t)job->desc().deploy_step() < max_deploy_pods) { 
            max_deploy_pods = job->desc().deploy_step();
        }
        LOG(INFO, "job %s deploy stat: pending_size %u, deploying_num %u, max_deploy_pods %u",
          job->jobid().c_str(), job_pending_pods.size(), deploying_num, max_deploy_pods);
        if (job_pending_pods.size() <= 0 
           || deploying_num >= max_deploy_pods) {
            continue;
        }
        std::vector<std::string> need_schedule;
        for (uint32_t i = 0; i < job_pending_pods.size() && deploying_num < max_deploy_pods; i++) {
            need_schedule.push_back(job_pending_pods[i].podid());
            deploying_num++;
        }
        PodDescriptor* pod_desc = NULL;
        for (int i = 0; i < job->pod_descs_size(); ++i) {
            if (job->pod_descs(i).version() != job->latest_version()) {
                continue;
            }
            pod_desc = job->mutable_pod_descs(i);
        }
        if (pod_desc == NULL) {
            LOG(WARNING, "fail to get job %s latest version %s pod descripto",
              job->jobid().c_str(), job->latest_version().c_str());
            continue;
        }
        PodScaleUpCell* need_schedule_cell = new PodScaleUpCell();
        need_schedule_cell->pod = pod_desc;
        need_schedule_cell->job = job;
        need_schedule_cell->pod_ids = need_schedule;
        need_schedule_cell->feasible_limit = job->pods_size() * FLAGS_scheduler_feasibility_factor;
        all_agent_needs += need_schedule_cell->feasible_limit;
        pending_pods->push_back(need_schedule_cell); 
        LOG(INFO, "scheduler choosed %u pod to deploy for job %s" , need_schedule.size(), job->jobid().c_str());
    }
    return all_agent_needs;
}

int32_t Scheduler::ChooseReducingPod(std::vector<JobInfo*>& reducing_jobs,
                                     std::vector<PodScaleDownCell*>* reducing_pods) {
    // 获取scale down的任务
    int reducing_count = 0;
    std::vector<JobInfo*>::iterator job_it = reducing_jobs.begin();
    for (; job_it != reducing_jobs.end(); ++job_it) {
        PodScaleDownCell* cell = new PodScaleDownCell();
        cell->pod = (*job_it)->mutable_desc()->mutable_pod();
        cell->job = *job_it;
        for (int i = 0; i < (*job_it)->pods_size(); ++i) {
            boost::unordered_map<std::string, AgentInfoExtend*>::iterator agt_it =
                    resources_.find((*job_it)->pods(i).endpoint());
            if (agt_it == resources_.end()) {
                LOG(INFO, "scale down pod %s dose not belong to agent %s",
                        (*job_it)->pods(i).podid().c_str(),
                        (*job_it)->pods(i).endpoint().c_str());
            }
            else {
                LOG(INFO, "scale down pod %s  agent %s",
                        (*job_it)->pods(i).podid().c_str(),
                        (*job_it)->pods(i).endpoint().c_str());
                cell->pod_agent_map.insert(
                        std::make_pair((*job_it)->pods(i).podid(), agt_it->second->agent_info));
            }
        }
        // 计算需要scale_down数目
        int32_t scale_down_count = (*job_it)->pods_size() - cell->job->desc().replica();
        cell->scale_down_count = scale_down_count;
        reducing_count += scale_down_count;
        LOG(INFO, "job %s scale down count %d with pod_size %d replica %d", (*job_it)->jobid().c_str(), scale_down_count, (*job_it)->pods_size(), cell->job->desc().replica());
        reducing_pods->push_back(cell);
    }
    return reducing_count;
}

void Scheduler::BuildSyncRequest(GetResourceSnapshotRequest* request) {
    boost::unordered_map<std::string, AgentInfoExtend*>::iterator it =
      resources_.begin();
    for (; it != resources_.end(); ++it) {
        DiffVersion* version = request->add_versions();
        version->set_version(it->second->agent_info->version());
        version->set_endpoint(it->second->agent_info->endpoint());
    }
}

PodScaleUpCell::PodScaleUpCell():pod(NULL), job(NULL),
        schedule_count(0), feasible_limit(0) {}

bool PodScaleUpCell::FeasibilityCheck(const AgentInfoExtend* agent_info_extend) {

    // check label 
    // TODO for simple only check first label   
    if (pod->labels_size() > 0) {
        std::string label = pod->labels(0);
        if (!label.empty() && agent_info_extend->labels_set.find(label) 
                                            == agent_info_extend->labels_set.end()) {
            LOG(INFO, "agent %s with version %d does not fit job %s label check %s", 
                      agent_info_extend->agent_info->endpoint().c_str(),
                      agent_info_extend->agent_info->version(),
                      job->jobid().c_str(),
                      label.c_str());         
            return false;
        }
    }

    LOG(INFO, "agent used port size %d , pod require port size %d",
             agent_info_extend->used_port_set.size(),
             pod->requirement().ports_size());
    for (int port_ind = 0; 
            port_ind < pod->requirement().ports_size(); 
            port_ind++) {
        int32_t port = pod->requirement().ports(port_ind);
        if (agent_info_extend->used_port_set.find(port) != 
                agent_info_extend->used_port_set.end()) {
            LOG(INFO, "agent %s with version %d does not fit job %s port check used %d",
                    agent_info_extend->agent_info->endpoint().c_str(),
                    agent_info_extend->agent_info->version(),
                    job->jobid().c_str(),
                    port);
            return false; 
        }
    }

    // 对于prod任务(kLongRun,kSystem)，根据unassign值check
    // 对于non-prod任务(kBatch)，根据free值check
    if (job->desc().type() == kLongRun ||
            job->desc().type() == kSystem) {
        bool fit = ResourceUtils::GtOrEq(agent_info_extend->unassigned, pod->requirement());
        if (fit) {
            LOG(INFO, "agent %s fit long run job %s resource requirement", 
                agent_info_extend->agent_info->endpoint().c_str(), 
                job->jobid().c_str());
        } else {
            LOG(INFO, "agent %s with version %d does not fit job %s require:mem require %ld, cpu require %d , agent stat:mem total %ld, cpu total %d, mem assigned %ld, cpu assinged %d, mem used %ld, cpu used %d",
              agent_info_extend->agent_info->endpoint().c_str(), agent_info_extend->agent_info->version(),
              job->jobid().c_str(),
              pod->requirement().memory(), pod->requirement().millicores(),
              agent_info_extend->agent_info->total().memory(), agent_info_extend->agent_info->total().millicores(),
              agent_info_extend->agent_info->assigned().memory(), agent_info_extend->agent_info->assigned().millicores(),
              agent_info_extend->agent_info->used().memory(), agent_info_extend->agent_info->used().millicores());
        }
        return fit;
    }
    else if (job->desc().type() == kBatch) {
        bool fit = ResourceUtils::GtOrEq(agent_info_extend->free, pod->requirement());
        if (fit) {
            LOG(INFO, "agent %s fit batch job %s resource requirement", 
                agent_info_extend->agent_info->endpoint().c_str(), job->jobid().c_str());
        } else {
            LOG(INFO, "agent %s does not fit job %s resource requirement, agent unassigned cpu %d, mem %d M, resource require cpu %d, mem %d m",
              agent_info_extend->agent_info->endpoint().c_str(),
              job->jobid().c_str(),
              agent_info_extend->free.millicores(),
              agent_info_extend->free.memory(),
              pod->requirement().millicores(),
              pod->requirement().memory());
        }
        return fit;
    }
    else {
        // 不支持类型
        LOG(INFO, "scheduler does not support job %s", job->jobid().c_str());
        return false;
    }
}

bool PodScaleUpCell::VolumeFit(std::vector<Volume>& unassigned,
                                    std::vector<Volume>& required) {
    // 磁盘需求升序排列, best fit
    std::sort(unassigned.begin(), unassigned.end(), VolumeCompare);
    std::sort(required.begin(), required.end(), VolumeCompare);
    size_t fit_index = 0;
    for (size_t i = 0; i < unassigned.size() && fit_index < required.size(); ++i) {
        if (required[fit_index].quota() <= unassigned[i].quota()) {
            ++fit_index;
        }
    }
    if (fit_index  < (required.size() - 1)) {
        return false;
    }
    return true;
}

int32_t PodScaleUpCell::Score() {
    std::vector<AgentInfoExtend*>::iterator agt_it = feasible.begin();
    for(; agt_it != feasible.end(); ++agt_it) {
        double score = ScoreAgent((*agt_it)->agent_info, pod);
        sorted.push_back(std::make_pair(score, (*agt_it)->agent_info));
    }
    std::sort(sorted.begin(), sorted.end(), AgentCompare);
    return 0;
}

int32_t PodScaleUpCell::Propose(std::vector<ScheduleInfo*>* propose) {
    int propose_count = 0;
    std::vector<std::pair<double, AgentInfo*> >::iterator sorted_it = sorted.begin();
    for (size_t i = 0; i < pod_ids.size(); ++i) {
        if (sorted_it == sorted.end()) {
            break;
        }
        else {
            ScheduleInfo* sched = new ScheduleInfo();
            sched->set_endpoint(sorted_it->second->endpoint());
            sched->set_podid(pod_ids[i]);
            sched->set_jobid(job->jobid());
            sched->set_action(kLaunch);
            propose->push_back(sched);
            LOG(DEBUG, "propose[%d] %s:%s on %s", propose_count,
                    sched->jobid().c_str(),
                    sched->podid().c_str(),
                    sched->endpoint().c_str());
            ++propose_count;
            ++sorted_it;
        }
    }
    return propose_count;
}

double PodScaleUpCell::ScoreAgent(const AgentInfo* agent_info,
                   const PodDescriptor* /*desc*/) {
    // 计算机器当前使用率打分
    double score = Scheduler::CalcLoad(agent_info);
    LOG(DEBUG, "score %s %lf", agent_info->endpoint().c_str(), score);
    return score;
}

PodScaleDownCell::PodScaleDownCell() : pod(NULL), job(NULL), scale_down_count(0) {}

int32_t PodScaleDownCell::Score() {
    std::map<std::string, AgentInfo*>::iterator pod_agt_it = pod_agent_map.begin();
    for(; pod_agt_it != pod_agent_map.end(); ++pod_agt_it) {
        double score = ScoreAgent(pod_agt_it->second, pod);
        sorted_pods.push_back(std::make_pair(score, pod_agt_it->first));
    }
    std::sort(sorted_pods.begin(), sorted_pods.end(), ScoreCompare);
    return 0;
}

double PodScaleDownCell::ScoreAgent(const AgentInfo* agent_info,
                   const PodDescriptor* /*desc*/) {
    // 计算机器当前使用率打分
    double score = Scheduler::CalcLoad(agent_info);
    LOG(DEBUG, "score %s %lf", agent_info->endpoint().c_str(), score);
    return -1 * score;
}

int32_t PodScaleDownCell::Propose(std::vector<ScheduleInfo*>* propose) {
    int propose_count = 0;
    std::map<std::string, AgentInfo*>::iterator pod_agent_it;
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
            ScheduleInfo* sched = new ScheduleInfo();
            sched->set_endpoint(pod_agent_it->second->endpoint());
            sched->set_podid(sorted_it->second);
            sched->set_jobid(job->jobid());
            sched->set_action(kTerminate);
            propose->push_back(sched);
            LOG(INFO, "scale down propose[%d] %s:%s on %s", propose_count,
                    sched->jobid().c_str(),
                    sched->podid().c_str(),
                    sched->endpoint().c_str());
            ++propose_count;
            ++sorted_it;
        }
    }
    return propose_count;
}

// calc agent info free and unassigned properties
void AgentInfoExtend::CalcExtend() {
    unassigned.CopyFrom(agent_info->total());
    bool ret = ResourceUtils::Alloc(agent_info->assigned(), unassigned);
    if (!ret) {
        LOG(WARNING, "fail to calc agent %s unassigned info extend", agent_info->endpoint().c_str());
        unassigned.set_millicores(0);
    }
    free.CopyFrom(agent_info->total());
    ret = ResourceUtils::Alloc(agent_info->used(), free);
    if (!ret) {
         LOG(WARNING, "fail to calc agent %s free info extend", agent_info->endpoint().c_str());
         free.set_millicores(0);
    }
    labels_set.clear();
    for (int i = 0; i < agent_info->tags_size(); i++) {
        labels_set.insert(agent_info->tags(i)); 
    }
    used_port_set.clear();
    for (int i = 0; i < agent_info->assigned().ports_size(); i++) {
        used_port_set.insert(agent_info->assigned().ports(i)); 
    }
}


int32_t Scheduler::UpdateAgent(const AgentInfo* agent_info) {
    boost::unordered_map<std::string, AgentInfoExtend*>::iterator agt_it =
      resources_.find(agent_info->endpoint());
    if (agt_it != resources_.end()) {
        LOG(INFO, "update agent %s", agent_info->endpoint().c_str());
        agt_it->second->agent_info->CopyFrom(*agent_info);
        agt_it->second->CalcExtend();
        return 0;
    }
    else {
        LOG(INFO, "update agent %s not exists", agent_info->endpoint().c_str());
        return 1;
    }
}

int32_t Scheduler::ScheduleUpdate(std::vector<JobInfo*>& update_jobs,
                                  std::vector<ScheduleInfo*>* propose) {
    std::vector<JobInfo*>::iterator job_it = update_jobs.begin();
    for(; job_it != update_jobs.end(); ++job_it) {
        JobInfo* job_info = *job_it; 
        HandleJobUpdate(job_info, propose);
    }
    return 0;
}

void Scheduler::HandleJobUpdate(JobInfo* job_info,
                                std::vector<ScheduleInfo*>* propose) {
    std::vector<PodStatus*> need_update_pods;
    int32_t updating_count = 0;
    for (int32_t i = 0; i < job_info->pods_size(); i++) {
        PodStatus* pod = job_info->mutable_pods(i);
        if (pod->version() != job_info->latest_version()) {
            need_update_pods.push_back(pod); 
        }
        if ( pod->stage() == kStageDeath
           || pod->state() == kPodDeploying) {
            updating_count++;
        }
    }
    LOG(INFO, "job %s update stat: updating_count %d, update_step_size %d, need_update_size %u",
        job_info->jobid().c_str(), updating_count, job_info->desc().deploy_step(), need_update_pods.size());
    int32_t max_update_size = need_update_pods.size();
    if (max_update_size > job_info->desc().deploy_step()) {
        max_update_size = job_info->desc().deploy_step();
    }
    for (size_t i = 0; i < need_update_pods.size() && updating_count < max_update_size ; i++ ) {
        updating_count++;
        ScheduleInfo* sched_info = new ScheduleInfo();
        PodStatus* pod = need_update_pods[i];
        sched_info->set_endpoint(pod->endpoint());
        LOG(INFO, "choose agent %s update pod %s of job %s", pod->endpoint().c_str(), pod->podid().c_str(), pod->jobid().c_str());
        sched_info->set_jobid(pod->jobid());
        sched_info->set_podid(pod->podid());
        sched_info->set_action(kTerminate);
        propose->push_back(sched_info);
    }
}




}// galaxy
}// baidu
