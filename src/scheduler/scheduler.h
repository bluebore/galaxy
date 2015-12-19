// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_SCHEDULER_H
#define BAIDU_GALAXY_SCHEDULER_H
#include <map>
#include <string>
#include <stdlib.h>
#include <algorithm>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/shared_ptr.hpp>
#include "proto/master.pb.h"
#include "mutex.h"
#include "rpc/rpc_client.h"
#include "logging.h"

namespace baidu {
namespace galaxy {

struct PodScaleCell {
    PodScaleCell();
    virtual void Score(){}
    virtual void Propose(std::vector<ScheduleInfo>* propose){}
    virtual ~PodScaleCell();
};

struct PodScaleUpCell : PodScaleCell {
    PodDescriptor pod;
    JobInfo job;
    uint32_t schedule_count;
    uint32_t feasible_limit;
    std::vector<std::string> pod_ids;
    std::vector<AgentInfo> feasible;
    std::vector<std::pair<double, std::string> > sorted;
    bool proposed;
    PodScaleUpCell();

    bool AllocFrom(AgentInfo* agent_info);

    void Score();

    double ScoreAgent(const AgentInfo& agent_info,
                      const PodDescriptor& desc);

    void Propose(std::vector<ScheduleInfo>* propose);
    ~PodScaleUpCell();
};

struct PodPreemptCell  {
    PodDescriptor pod;
    JobInfo job;
    std::string podid;
    bool preempted;
    PreemptEntity pending_pods;
    std::vector<PreemptEntity> preempted_pods;
    std::string agent;
    PodPreemptCell() {}
    bool TryPreempt(AgentInfo* agent_info, boost::unordered_map<std::string, JobDescriptor>* jobs);
    ~PodPreemptCell(){}
};

struct PodUpdateCell : PodScaleCell {
    JobInfo job;
    PodUpdateCell(){}
    void Score();
    void Propose(std::vector<ScheduleInfo>* propose);
    ~PodUpdateCell(){}
};

struct PodScaleDownCell : PodScaleCell{
    PodDescriptor pod;
    JobInfo job;
    uint32_t scale_down_count;
    std::map<std::string, AgentInfo> pod_agent_map;
    std::vector<std::pair<double, std::string> > sorted_pods;
    PodScaleDownCell();
    void Score();

    double ScoreAgent(const AgentInfo& agent_info,
                      const PodDescriptor& desc);

    void Propose(std::vector<ScheduleInfo>* propose);
    ~PodScaleDownCell();
};

class Scheduler {

public:
    static double CalcLoad(const AgentInfo& agent);

    Scheduler() {
        jobs_ = new boost::unordered_map<std::string, JobDescriptor>();
        resources_ = new boost::unordered_map<std::string, AgentInfo>();
    }
    ~Scheduler() {
        delete jobs_;
        delete resources_;
    }

    void ScheduleScaleUp(const std::string& master_addr,
                         std::vector<JobInfo>& pending_jobs);

    void ScheduleScaleDown(const std::string& master_addr,
                           std::vector<JobInfo>& reducing_jobs);

    void ScheduleUpdate(const std::string& master_addr,
                        std::vector<JobInfo>& update_jobs);
    // 从master同步资源数据
    int32_t SyncResources(const GetResourceSnapshotResponse* response);

    // master通知AgentInfo信息过期，需要更新
    int32_t UpdateAgent(const AgentInfo* agent_info);

    void BuildSyncRequest(GetResourceSnapshotRequest* request);

    void BuildSyncJobRequest(GetJobDescriptorRequest* request);
    void SyncJobDescriptor(const GetJobDescriptorResponse* response);
private:

    bool CanPreempt(const AgentInfo& agent_info);
    void SchedulePreempt(const std::string& master_addr,
                         const boost::unordered_map<std::string, AgentInfo>& changed_resource,
                         std::vector<JobInfo>& jobs);
    int32_t ChoosePods(std::vector<JobInfo>& pending_jobs,
                       std::vector<boost::shared_ptr<PodScaleUpCell> >& pending_pods);

    int32_t ChooseReducingPod(std::vector<JobInfo>& reducing_jobs,
                              std::vector<boost::shared_ptr<PodScaleDownCell> >& reducing_pods);

    template<class T>
    void Shuffle(std::vector<T>& list) {
        for (size_t i = list.size(); i > 1; i--) {
            T tmp = list[i-1];
            size_t target_index = ::rand() % i ;
            list[i-1] = list[target_index];
            list[target_index] = tmp;
        }
    }

    void Propose(boost::shared_ptr<PodScaleCell> cell, 
                 const std::string& master_addr);
    void ProposeCallBack(const ProposeRequest* request,
                         ProposeResponse* response,
                         bool failed, int);
    void Preempt(boost::shared_ptr<PodPreemptCell> cell,
                const std::string& master_addr);
    void PreemptCallBack(const PreemptRequest* request,
                         PreemptResponse* response,
                         bool failed, int);
private:
    Mutex sched_mutex_;
    boost::unordered_map<std::string, AgentInfo>* resources_;
    boost::unordered_map<std::string, JobDescriptor>* jobs_;
    RpcClient rpc_client_;
    ThreadPool thread_pool_;
};


} // galaxy
}// baidu
#endif
