// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#ifndef _SRC_AGENT_DYNAMIC_RESOURCE_SCHEDULER_H
#define _SRC_AGENT_DYNAMIC_RESOURCE_SCHEDULER_H

#include <map>
#include <deque>
#include <queue>
#include "common/mutex.h"
#include "common/thread_pool.h"
#include "agent/resource_collector.h"
#include "agent/resource_manager.h"

namespace galaxy {

struct ScheduleCell {
    std::string cgroup_name;
    long cpu_quota;
    long cpu_limit; // limit == quota
    long cpu_extra;
    int64_t collector_id;
    CGroupResourceCollector* collector;
    int64_t last_scheduler_time;
    std::deque<double> cpu_usages;
    double cpu_usage;
    // for execute 
    long cpu_extra_need;
    // frozen_schedule only can't keep cpu_extra > 0
    bool frozen_schedule;
    int64_t frozen_time;
};

class ScheduleCellComp {
public:
    bool operator() (ScheduleCell* left, ScheduleCell* right) {
        return left->cpu_extra < right->cpu_extra;
    }
};

class DynamicResourceScheduler;
DynamicResourceScheduler* GetDynamicResourceScheduler();

class DynamicResourceScheduler {
public:
    explicit DynamicResourceScheduler(int64_t schedule_interval);

    ~DynamicResourceScheduler(); 
    // resource_mgr unusage resource to schedule
    // NOTE struct requirement cpu unit 
    void Release(const TaskResourceRequirement& requirement);    
    // resource_mgr requirement for new task
    int Allocate(const TaskResourceRequirement& requirement);

    void RegistCgroupPath(const std::string& cgroup_name, 
            double cpu_quota, double cpu_limit);
    bool UnRegistCgroupPath(const std::string& cgroup_name);

    void SetFrozen(const std::string& cgroup_name, 
            int64_t frozen_time = -1);
    void UnFrozen(const std::string& cgroup_name); 
    void KeepQuota(const std::string& cgroup_name);

    bool LoadPersistenceInfo(const DynamicSchedulerPersistence& info);
    bool DumpPersistenceInfo(DynamicSchedulerPersistence* info);

    bool Stop() {
        stop_ = true; 
        return scheduler_thread_->Stop(false);
    }
private:

    void ScheduleResourceUsage();
    bool ReCalcCpuNeed(std::vector<ScheduleCell*>* schedule_vector);
    bool ExecuteScheduleCell(ScheduleCell* cell);
    bool ExecuteCpuNeed(std::vector<ScheduleCell*>* schedule_vector);
    void CalcCpuNeed(std::vector<ScheduleCell*>* schedule_vector);

    int64_t schedule_interval_;
    common::ThreadPool* scheduler_thread_;
    typedef std::map<std::string, ScheduleCell> ScheduleCellsMap;
    ScheduleCellsMap schedule_cells_;

    common::Mutex lock_;
     
    // cpu cores that not alloc for quota
    long cpu_cores_left_;
    // cpu cores that save from quota
    long dynamic_quota_left_;

    // threshold value for scheduler when cpu_limit <= quota
    // (left_threshold_in_quota_, right_threshold_in_quota_)
    int64_t left_threshold_in_quota_;
    int64_t right_threshold_in_quota_;
    
    // TODO threshold value for scheduler when cpu_limit > quota not used
    int64_t left_threshold_out_of_quota_;
    int64_t right_threshold_out_of_quota_;
    typedef std::priority_queue<ScheduleCell*,
                std::vector<ScheduleCell*>,
                ScheduleCellComp> SchedulePriorityQueue;
    SchedulePriorityQueue extra_queue_;
    bool stop_;
};

}   // ending namespace galaxy

#endif  //_SRC_AGENT_DYNAMIC_RESOURCE_LIMITER_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
