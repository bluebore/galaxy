// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scheduler/scheduler.h"

#include <math.h>
#include <algorithm>
#include "logging.h"
#include "utils/resource_utils.h"

namespace baidu {
namespace galaxy {

const static int feasibility_factor = 2;

// cpu单位为milli
const static double cpu_used_factor = 10.0f;

const static double cpu_overload_threashold = 0.9f;

const static int agent_overload_turns_threashold = 3;

const static double mem_used_factor = 1.0f;

const static double prod_count_factor = 32.0f;

//const static double non_prod_count_factor = 100.0f;

double Scheduler::CalcLoad(const AgentInfo* agent) {
    double cpu_load = agent->used().millicores() * cpu_used_factor /
            agent->total().millicores();
    double mem_load = agent->used().memory() * mem_used_factor /
            agent->total().memory();
    double prod_load = agent->pods_size() / prod_count_factor;
    // TODO: Agent增加non-prod计数
    //double non_prod_load = agent.pods_size() / prod_count_factor;
    return exp(cpu_load) + exp(mem_load) + exp(prod_load);
}

bool Scheduler::CheckOverLoad(const AgentInfo* agent) {
    return (agent->used().millicores() * 1.0 / agent->total().millicores()
            > cpu_overload_threashold);
}

/*
 * @brief 按照磁盘可用大小升序排列
 */
static bool VolumeCompare(const Volume& volume1, const Volume& volume2) {
    return volume1.quota() < volume2.quota();
}

/*
 * @brief 按照priority降序排序
 *
 * */
static bool JobCompare(const JobInfo* left, const JobInfo* right) {
    return left->desc().priority() > right->desc().priority();
}

int32_t Scheduler::ScheduleScaleUp(std::vector<JobInfo*>& pending_jobs,
                                   std::vector<ScheduleInfo*>* propose) {
    LOG(INFO, "schedule scale up turns: %lld", schedule_turns_);
    ++schedule_turns_;

    int propose_count = 0;
    // 计算job优先级，及其需要调度的pod数量
    std::vector<PodScaleUpCell*> pending_pods;
    std::vector<PodKeepRunningCell*> keep_running_pods;
    // pod 依照优先级进行排序
    int32_t total_feasible_count = ChoosePods(pending_jobs, 
                                              &pending_pods,
                                              &keep_running_pods);
    for (size_t i = 0; i < keep_running_pods.size(); i++) {
        JobInfo* job = keep_running_pods[i]->job;
        std::map<std::string, std::string>::iterator it = 
          keep_running_pods[i]->keep_running_pods.begin();
        for (; it != keep_running_pods[i]->keep_running_pods.end();
          ++it) {
            ScheduleInfo* sched_info = new ScheduleInfo();
            sched_info->set_jobid(job->jobid());
            sched_info->set_podid(it->first);
            sched_info->set_endpoint(it->second);
            sched_info->set_action(kKeepRunning);
            propose->push_back(sched_info);
        }
    }
    LOG(INFO, "feasibility checking count : %d, factor %d"
            , total_feasible_count, feasibility_factor);

    // shuffle resources_
    std::vector<AgentInfoExtend*> resources_to_alloc;
    ChooseResourse(&resources_to_alloc);
    LOG(INFO, "resources choosen : %u", resources_to_alloc.size());

    // 计算feasibility
    std::vector<AgentInfoExtend*>::iterator res_it = resources_to_alloc.begin();
    int cur_feasible_count = 0;
    for (; res_it != resources_to_alloc.end() &&
            cur_feasible_count < total_feasible_count; ++res_it) {
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

    for (std::vector<PodKeepRunningCell*>::iterator pod_it =
      keep_running_pods.begin();
      pod_it != keep_running_pods.end(); ++pod_it) {
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
        LOG(DEBUG, "agent resource free: millicores %d memory %d  #disk %u #ssd %u",
                agent->free.millicores(), agent->free.memory(),
                agent->free.disks_size(), agent->free.ssds_size());
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

    assert(total_reducing_count >= 0);

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
       if (it == resources_.end()) {
           AgentInfo* agent = new AgentInfo();
           agent->CopyFrom(response->agents(i));
           AgentInfoExtend* extend = new AgentInfoExtend(agent);
           resources_.insert(std::make_pair(agent->endpoint(), extend));
       }else {
           it->second->agent_info->CopyFrom(response->agents(i));
           // re calc free and unassigned
           it->second->CalcExtend();
       }
   }
   return resources_.size();
}

int32_t Scheduler::ChoosePods(std::vector<JobInfo*>& pending_jobs,
                              std::vector<PodScaleUpCell*>* pending_pods,
                              std::vector<PodKeepRunningCell*>* keep_running_pods) {
    // 获取scale up的任务
    int feasibility_count = 0;

    // 按照Job优先级进行排序
    std::sort(pending_jobs.begin(), pending_jobs.end(), JobCompare);
    std::vector<JobInfo*>::iterator job_it = pending_jobs.begin();
    for (; job_it != pending_jobs.end(); ++job_it) {
        JobInfo* job = *job_it;
        int32_t pending_size = job->pods_size();
        if (pending_size <= 0) {
            LOG(WARNING, "job %s has no pending pod but it is in master pending queue", job->jobid().c_str());
            continue;
        }
        int32_t deploying_num = 0;
        int32_t deploy_step = pending_size;
        if (job->desc().deploy_step() > 0) {
            std::map<std::string, JobOverview*>::iterator jo_it = job_overview_.find(job->jobid());
            if (jo_it == job_overview_.end()) {
                LOG(WARNING, "job %s does not exist in master jobs ,scheduler skips it",
                  job->jobid().c_str());
                continue;
            }
            JobOverview* job_overview = jo_it->second;
            deploying_num = job_overview->deploying_num();
            deploy_step = job->desc().deploy_step();
        }
        std::vector<std::string> need_schedule;
        std::map<std::string, std::string> no_need_schedule;
        LOG(INFO, "job %s deploy stat: pending_size %d, deploying_num %d, deploy_step %d",
          job->jobid().c_str(), pending_size, deploying_num, deploy_step);
        for (int i = deploying_num; i < deploy_step; ++i) {
            //TODO handle dead agent
            if (job->desc().pod().pin_on_agent() &&
                !job->pods(i).endpoint().empty() ){
                no_need_schedule.insert(std::make_pair(job->pods(i).podid(),
                                        job->pods(i).endpoint()));
                LOG(INFO, "no need to schedule pod %d and keep it running on agent %s ", job->pods(i).podid().c_str(), job->pods(i).endpoint().c_str());
                continue;
            }
            need_schedule.push_back(job->pods(i).podid());
            LOG(INFO, "scheduler pod %s" , job->pods(i).podid().c_str());
        }
        if (no_need_schedule.size() > 0) {
            PodKeepRunningCell* keep_running_cell = new PodKeepRunningCell();
            keep_running_cell->job = job;
            keep_running_cell->keep_running_pods = no_need_schedule;
            keep_running_pods->push_back(keep_running_cell);
        }

        if (need_schedule.size() > 0) {
            PodScaleUpCell* need_schedule_cell = new PodScaleUpCell();
            need_schedule_cell->pod = job->mutable_desc()->mutable_pod();
            need_schedule_cell->job = job;
            need_schedule_cell->pod_ids = need_schedule;
            need_schedule_cell->feasible_limit = job->pods_size() * feasibility_factor;
            feasibility_count += need_schedule_cell->feasible_limit;
            pending_pods->push_back(need_schedule_cell);
            CalcSources(*(need_schedule_cell->pod), &(need_schedule_cell->resource));
        }
    }
    return feasibility_count;
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
                cell->pod_agent_map.insert(
                        std::make_pair((*job_it)->pods(i).podid(), agt_it->second->agent_info));
            }
        }
        // 计算需要scale_down数目
        int32_t scale_down_count = (*job_it)->pods_size() - cell->job->desc().replica();
        cell->scale_down_count = scale_down_count;
        reducing_count += scale_down_count;
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
    // 对于prod任务(kLongRun,kSystem)，根据unassign值check
    // 对于non-prod任务(kBatch)，根据free值check
    if (job->desc().type() == kLongRun ||
            job->desc().type() == kSystem) {
        bool fit = ResourceUtils::GtOrEq(agent_info_extend->unassigned, resource);
        if (fit) {
            LOG(INFO, "agent %s fit long run job %s resource requirement", 
                agent_info_extend->agent_info->endpoint().c_str(), 
                job->jobid().c_str());
        } else {
            LOG(INFO, "agent %s does not fit job %s resource requirement, agent unassigned cpu %d, mem %d M, resource require cpu %d, mem %d m",
              agent_info_extend->agent_info->endpoint().c_str(),
              job->jobid().c_str(),
              agent_info_extend->unassigned.millicores(),
              agent_info_extend->unassigned.memory(),
              resource.millicores(),
              resource.memory());
        }
        return fit;
    }
    else if (job->desc().type() == kBatch) {
        bool fit = ResourceUtils::GtOrEq(agent_info_extend->free, resource);
        if (fit) {
            LOG(INFO, "agent %s fit batch job %s resource requirement", 
                agent_info_extend->agent_info->endpoint().c_str(), job->jobid().c_str());
        } else {
            LOG(INFO, "agent %s does not fit job %s resource requirement, agent unassigned cpu %d, mem %d M, resource require cpu %d, mem %d m",
              agent_info_extend->agent_info->endpoint().c_str(),
              job->jobid().c_str(),
              agent_info_extend->free.millicores(),
              agent_info_extend->free.memory(),
              resource.millicores(),
              resource.memory());
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
        sorted.insert(std::make_pair(score, (*agt_it)->agent_info));
    }
    return 0;
}

int32_t PodScaleUpCell::Propose(std::vector<ScheduleInfo*>* propose) {
    int propose_count = 0;
    std::map<double, AgentInfo*>::iterator sorted_it = sorted.begin();
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
        sorted_pods.insert(std::make_pair(score, pod_agt_it->first));
    }
    return 0;
}

double PodScaleDownCell::ScoreAgent(const AgentInfo* agent_info,
                   const PodDescriptor* desc) {
    // 计算机器当前使用率打分
    double score = Scheduler::CalcLoad(agent_info);
    LOG(DEBUG, "score %s %lf", agent_info->endpoint().c_str(), score);
    return -1 * score;
}

int32_t PodScaleDownCell::Propose(std::vector<ScheduleInfo*>* propose) {
    int propose_count = 0;
    std::map<std::string, AgentInfo*>::iterator pod_agent_it;
    std::map<double, std::string>::iterator sorted_it = sorted_pods.begin();
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

// calc agent info free and unassigned properties
void AgentInfoExtend::CalcExtend() {
    unassigned.CopyFrom(agent_info->total());
    bool ret = ResourceUtils::Alloc(agent_info->assigned(), unassigned);
    assert(ret);
    free.CopyFrom(agent_info->total());
    ret = ResourceUtils::Alloc(agent_info->used(), free);
    assert(ret);
}


int32_t Scheduler::CalcSources(const PodDescriptor& pod, Resource* resource) {
    for (int j = 0; j < pod.tasks_size(); ++j) {
        int32_t millicores = resource->millicores();
        resource->set_millicores(millicores + pod.tasks(j).requirement().millicores());
        int32_t memory = resource->memory();
        resource->set_memory(memory + pod.tasks(j).requirement().memory());
        for (int h = 0; h < pod.tasks(j).requirement().ports_size(); ++h) {
            resource->add_ports(pod.tasks(j).requirement().ports(h));
        }
        for (int h = 0; h < pod.tasks(j).requirement().disks_size(); ++h) {
            Volume* volume = resource->add_disks();
            volume->CopyFrom(pod.tasks(j).requirement().disks(h));
        }
        for (int h = 0; h < pod.tasks(j).requirement().ssds_size(); ++h) {
            Volume* volume = resource->add_ssds();
            volume->CopyFrom(pod.tasks(j).requirement().ssds(h));
        }
    }
    LOG(DEBUG, "pod resource requirement: millicores %d memory %d MB #disk %u #ssd %u",
            resource->millicores(), resource->memory(),
            resource->disks_size(), resource->ssds_size());
    return 0;
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

int32_t Scheduler::SyncJobOverview(const ListJobsResponse* response) {
    //MutexLock lock(mutex_);
    LOG(INFO, "start to sync job overview , old job count %u", job_overview_.size());
    for (std::map<std::string, JobOverview*>::iterator job_it = job_overview_.begin();
       job_it != job_overview_.end(); ++job_it) {
           delete job_it->second;
    }
    job_overview_.clear();

    for (int i = 0; i < response->jobs_size(); ++i) {
        JobOverview* job = new JobOverview();
        job->CopyFrom(response->jobs(i));
        job_overview_.insert(std::make_pair(job->jobid(), job));
    }
    LOG(INFO, "sync job overview successfully , job count %u", job_overview_.size());
    return job_overview_.size();
}

int32_t AgentHistory::PushOverloadAgent(const AgentInfo* agent) {
    int32_t turns = 0;
    std::map<std::string, int32_t>::iterator count_it =
            agent_overload_map_.find(agent->endpoint());
    if (count_it != agent_overload_map_.end()){
        turns = ++count_it->second;
    }
    else {
        turns = 1;
        agent_overload_map_.insert(std::make_pair(agent->endpoint(), turns));

    }
    return turns;
}

int32_t AgentHistory::CleanOverloadAgent(const AgentInfo* agent) {
    agent_overload_map_.erase(agent->endpoint());
    return 0;
}

int32_t AgentHistory::CheckOverloadAgent(const AgentInfo* agent) {
    std::map<std::string, int32_t>::iterator count_it =
            agent_overload_map_.find(agent->endpoint());
    if (count_it != agent_overload_map_.end()){
        return count_it->second;
    }
    else {
        return 0;
    }
}

int32_t Scheduler::ScheduleAgentOverLoad(std::vector<ScheduleInfo*>* propose) {
    LOG(INFO, "start to check agent overload,  job count %u, agent count %u",
        job_overview_.size(), resources_.size());
    int32_t scale_down_count = 0;
    boost::unordered_map<std::string, AgentInfoExtend*>::iterator agt_it = resources_.begin();
    for (; agt_it != resources_.end(); agt_it++) {
        if (CheckOverLoad(agt_it->second->agent_info) == true) {
            int turns = agent_his_.PushOverloadAgent(agt_it->second->agent_info);
            //  连续N次都过载，则需要scale down
            if (turns > agent_overload_turns_threashold) {
                LOG(INFO, "Agent %s has been overloading for %d turns, need schedule",
                    agt_it->first.c_str(), turns);
                //  计算需要ternimate的job
                int32_t count = ScaleDownOverloadAgent(agt_it->second->agent_info, propose);
                if (count > 0) {
                    scale_down_count += count;
                }
            }
        }
        else {
            agent_his_.CleanOverloadAgent(agt_it->second->agent_info);
        }

    }
    return scale_down_count;
}

struct PodToFree {
    std::string jobid;
    std::string podid;
    int32_t cpu_used;
    PodToFree(const std::string& job_id, const std::string& pod_id, int32_t cpu_used)
        : jobid(job_id), podid(pod_id), cpu_used(cpu_used) {}
};

/*
 * @brief 按照cpu实际使用率升序排列
 */
static bool PodToFreeCompare(const PodToFree& l, const PodToFree& r) {
    return l.cpu_used < r.cpu_used;
}

int32_t Scheduler::ScaleDownOverloadAgent(const AgentInfo* agent,
        std::vector<ScheduleInfo*>* propose) {
    int32_t prod_count = 0;
    int32_t non_prod_count = 0;
    int32_t ret = GetPodCountForAgent(agent, &prod_count, &non_prod_count);
    if (ret != 0) {
        LOG(WARNING, "can not get pod count for agent %s", agent->endpoint().c_str());
        return -1;
    }

    int32_t cpu_to_be_free =
            static_cast<int32_t>((agent->used().millicores() * 1.0f / agent->total().millicores() - cpu_overload_threashold)
            * agent->total().millicores());
    if (cpu_to_be_free < 0) {
        LOG(WARNING, "agent %s dose not need to scale down", agent->endpoint().c_str());
        return -1;
    }
    std::vector<PodToFree> pods_to_free;

    std::map<std::string, JobOverview*>::iterator job_it;
    for (int pod_idx = 0; pod_idx < agent->pods_size(); ++pod_idx) {
        const std::string& jobid = agent->pods(pod_idx).jobid();
        const std::string& podid = agent->pods(pod_idx).podid();
        job_it = job_overview_.find(jobid);
        if (job_it == job_overview_.end()) {
            LOG(WARNING, "jobid %s @ agent %s dose not exist",
                jobid.c_str(),
                agent->endpoint().c_str());
            continue;
        }
        JobType type = job_it->second->desc().type();
        if (type != kBatch) {
            continue;
        }
        int cpu_used = agent->pods(pod_idx).resource_used().millicores();
        pods_to_free.push_back(PodToFree(jobid, podid, cpu_used));
    }

    // 按照cpu实际使用率对non-prod任务进行排序
    std::sort(pods_to_free.begin(), pods_to_free.end(), PodToFreeCompare);
    for (size_t pod_idx = 0; pod_idx < pods_to_free.size(); ++pod_idx) {
        if (pods_to_free[pod_idx].cpu_used > cpu_to_be_free) {
            ScheduleInfo* sched = new ScheduleInfo();
            sched->set_endpoint(agent->endpoint());
            sched->set_podid(pods_to_free[pod_idx].podid);
            sched->set_jobid(pods_to_free[pod_idx].jobid);
            sched->set_action(kTerminate);
            propose->push_back(sched);
            LOG(DEBUG, "propose %s:%s on %s",
                    sched->jobid().c_str(),
                    sched->podid().c_str(),
                    sched->endpoint().c_str());
            propose->push_back(sched);
            break;
        }
    }
    return 1;
}


int32_t Scheduler::GetPodCountForAgent(const AgentInfo* agent,
                int32_t* prod_count, int32_t* non_prod_count) {

    return 0;
}

}// galaxy
}// baidu
