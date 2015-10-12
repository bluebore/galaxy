// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _SRC_AGENT_CPU_SCHEDULER_H
#define _SRC_AGENT_CPU_SCHEDULER_H

#include <string>
#include <map>
#include <deque>
#include <queue>
#include "agent/resource_collector.h"
#include "thread_pool.h"

namespace baidu {
namespace galaxy {

struct CpuSchedulerCell {
    int32_t cpu_quota;
    int32_t cpu_extra;
    int32_t cpu_extra_need;
    bool frozen;
    int64_t frozen_time;
    int32_t cpu_guarantee;
    CGroupResourceCollector* resource_collector; 
    std::string cgroup_name;
    std::deque<double> cpu_usages;
    int64_t last_schedule_time;

    CpuSchedulerCell() : 
        cpu_quota(-1),
        cpu_extra(0),
        cpu_extra_need(0),
        frozen(false),
        frozen_time(-1),
        cpu_guarantee(-1),
        resource_collector(NULL),
        cgroup_name(),
        cpu_usages(),
        last_schedule_time(0) {
    }
};

class CpuSchedulerCellComp {
public:
    bool operator() (CpuSchedulerCell* left, 
                     CpuSchedulerCell* right) {
        return left->cpu_extra < right->cpu_extra; 
    }
};

class CpuScheduler {
public:
    static CpuScheduler* GetInstance() {
        pthread_once(&ptonce_, &CpuScheduler::OnceInit); 
        return instance_;
    }

    void Release(int cpu_cores);
    int Allocate(int cpu_cores);

    bool EnqueueTask(const std::string& cgroup_name, 
                     int32_t cpu_quota);
    bool DequeueTask(const std::string& cgroup_name);

    void SetFrozen(const std::string& cgroup_name, 
                   int64_t frozen_time = -1);
    void UnFrozen(const std::string& cgroup_name);
    
    ~CpuScheduler(); 
private:
    void TraceCpuSchedulerCells(
            const std::map<std::string, CpuSchedulerCell*> sched_cells);
    void TraceSchedule();
    CpuScheduler();
    CpuScheduler(const CpuScheduler& /*sched*/) {}
    CpuScheduler& operator = (const CpuScheduler& /*sched*/) {return *this;}

    void KeepQuota(CpuSchedulerCell* cell);
    void RebuildExtraQueue();
    bool AllocateCpu(int cpu_cores);
    void ScheduleResourceUsage();
    // 计算有资源调度需求
    void CalcCpuNeed(std::map<std::string, CpuSchedulerCell*>* scheduler_cells);
    // 调整资源调度结果
    bool ReCalcCpuNeed(std::map<std::string, CpuSchedulerCell*>* scheduler_cells);
    // 执行cpu调度结果,并更新cpu_cores_lefts
    bool ExecuteCpuNeed(std::vector<CpuSchedulerCell*>* sched_vec);
    bool ExecuteSchedulerCell(CpuSchedulerCell* cell);

    static void OnceInit() {
        instance_ = new CpuScheduler();
        ::atexit(CpuScheduler::OnceDestroy);
    }
    static void OnceDestroy() {
        delete instance_; 
        instance_ = NULL;
    }

    void Shuffle(std::vector<CpuSchedulerCell*>* cells) {
        if (cells == NULL) {
            return; 
        } 
        for (size_t i = cells->size(); i > 1; i--) {
            CpuSchedulerCell* tmp = (*cells)[i - 1];  
            size_t target_index = ::rand() % i;
            (*cells)[i - 1] = (*cells)[target_index];
            (*cells)[target_index] = tmp;
        }
        return;
    }

private:

    static pthread_once_t ptonce_;
    static CpuScheduler* instance_;
    int32_t sched_interval_;
    int32_t collect_interval_;
    int32_t cpu_idle_high_limit_;
    int32_t cpu_idle_low_limit_;
    int32_t cpu_cores_left_;
    ThreadPool* scheduler_threads_;
    Mutex lock_; 
    typedef std::map<std::string, CpuSchedulerCell*> SchedulerCellsMap;
    SchedulerCellsMap schedule_cells_;

    typedef std::priority_queue<CpuSchedulerCell*,
                                std::vector<CpuSchedulerCell*>, 
                                CpuSchedulerCellComp> SchedulerPriorityQueue;
    SchedulerPriorityQueue extra_queue_;
};


}   // ending namespace galaxy
}   // ending namespace baidu

#endif  //_SRC_AGENT_CPU_SCHEDULER_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
