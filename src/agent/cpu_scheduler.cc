// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/cpu_scheduler.h"
#include "gflags/gflags.h"
#include "boost/bind.hpp"
#include "boost/lexical_cast.hpp"
#include "agent/cgroups.h"
#include "timer.h"
#include "logging.h"

DECLARE_int32(cpu_scheduler_intervals);
DECLARE_int32(cpu_scheduler_collector_intervals);
DECLARE_int32(cpu_scheduler_idle_high_limit);
DECLARE_int32(cpu_scheduler_idle_low_limit);
DECLARE_int32(cpu_scheduler_guarantee);
DECLARE_int32(cpu_scheduler_dec);
DECLARE_string(gce_cgroup_root);

namespace baidu {
namespace galaxy {

static int CPU_CFS_PERIOD = 100000;
static int MIN_CPU_CFS_QUOTA = 1000;

pthread_once_t CpuScheduler::ptonce_ = PTHREAD_ONCE_INIT;
CpuScheduler* CpuScheduler::instance_ = NULL;

CpuScheduler::CpuScheduler() : 
    sched_interval_(FLAGS_cpu_scheduler_intervals),
    collect_interval_(FLAGS_cpu_scheduler_collector_intervals),
    cpu_idle_high_limit_(FLAGS_cpu_scheduler_idle_high_limit),
    cpu_idle_low_limit_(FLAGS_cpu_scheduler_idle_low_limit),
    cpu_cores_left_(0),
    scheduler_threads_(NULL),
    lock_(),
    schedule_cells_(),
    extra_queue_() {
    scheduler_threads_ = new ThreadPool(1);
    scheduler_threads_->DelayTask(collect_interval_, 
            boost::bind(&CpuScheduler::ScheduleResourceUsage, this));
}

CpuScheduler::~CpuScheduler() {
    delete scheduler_threads_;
    SchedulerCellsMap::iterator it = schedule_cells_.begin();
    for (; it != schedule_cells_.end(); ++it) {
        CpuSchedulerCell* cell = it->second;
        if (cell->resource_collector != NULL) {
            delete cell->resource_collector; 
            cell->resource_collector = NULL;
        } 

        delete cell;
    }
}

void CpuScheduler::Release(int cpu_cores) {
    MutexLock scoped_lock(&lock_);
    cpu_cores_left_ += cpu_cores;
}

int CpuScheduler::Allocate(int cpu_cores) {
    MutexLock scoped_lock(&lock_);
    if (AllocateCpu(cpu_cores)) {
        return 0; 
    }
    return -1;
}

void CpuScheduler::SetFrozen(const std::string& cgroup_name,
                             int64_t frozen_time) {
    MutexLock scoped_lock(&lock_);
    SchedulerCellsMap::iterator it = 
                    schedule_cells_.find(cgroup_name);  
    if (it == schedule_cells_.end()) {
        LOG(WARNING, "no %s cells schedule", 
                     cgroup_name.c_str());
        return; 
    }

    CpuSchedulerCell* cell = it->second;
    cell->frozen = true;
    if (frozen_time != -1) {
        cell->frozen_time = 
            baidu::common::timer::get_micros() / 1000 
            + frozen_time;
    } else {
        cell->frozen_time = -1;
    }
    KeepQuota(cell);
    return;
}

void CpuScheduler::UnFrozen(const std::string& cgroup_name) {
    MutexLock scoped_lock(&lock_);
    SchedulerCellsMap::iterator it = 
                    schedule_cells_.find(cgroup_name);
    if (it == schedule_cells_.end()) {
        LOG(WARNING, "no %s cells schedule",
                     cgroup_name.c_str()); 
        return;
    }
    CpuSchedulerCell* cell = it->second;
    cell->frozen = false;
    cell->frozen_time = -1;
    return;
}

void CpuScheduler::KeepQuota(CpuSchedulerCell* cell) {
    lock_.AssertHeld();
    if (cell == NULL) {
        return; 
    }

    if (cell->cpu_extra >= 0) {
        cpu_cores_left_ += cell->cpu_extra;    
        cell->cpu_extra = 0;
    } else if (cpu_cores_left_ + cell->cpu_extra >= 0){
        cpu_cores_left_ -= cell->cpu_extra;
        cell->cpu_extra = 0; 
    } else {
        if (!AllocateCpu(-1 * cell->cpu_extra)) {
            TraceSchedule();
            return;
        }    
        cell->cpu_extra = 0;
    }
    return; 
}

void CpuScheduler::TraceSchedule() {
    SchedulerCellsMap::iterator it = schedule_cells_.begin();  
    LOG(WARNING, "FATAL sched cpu cores left : %d", cpu_cores_left_);
    LOG(WARNING, "FATAL sched cells size : %lu", schedule_cells_.size());
    for (; it != schedule_cells_.end(); ++it) {
        CpuSchedulerCell* cell = it->second;
        LOG(WARNING, "%s : %d : %d : %d", 
                cell->cgroup_name.c_str(),
                cell->cpu_quota,
                cell->cpu_extra,
                cell->cpu_extra_need);
    }
    return;
}

bool CpuScheduler::AllocateCpu(int cpu_cores) {
    lock_.AssertHeld();
    if (cpu_cores_left_ >= cpu_cores) {
        cpu_cores_left_ -= cpu_cores;
        return true; 
    }

    if (extra_queue_.size() == 0) {
        LOG(WARNING, "no enough resource");
        return false; 
    }

    int just_need_cpu = cpu_cores;
    if (cpu_cores_left_ > 0) {
        just_need_cpu -= cpu_cores_left_;
    }
    std::vector<CpuSchedulerCell*> sched_vec;
    RebuildExtraQueue();
    while (just_need_cpu > 0) {
        CpuSchedulerCell* cell = extra_queue_.top();
        extra_queue_.pop();
        if (cell->cpu_extra <= 0) {
            extra_queue_.push(cell); 
            break;
        }
        sched_vec.push_back(cell);
        if (just_need_cpu >= cell->cpu_extra) {
            cell->cpu_extra_need = 0 - cell->cpu_extra; 
        } else {
            cell->cpu_extra_need = 0 - just_need_cpu;
        }
        just_need_cpu -= cell->cpu_extra;
    }

    if (just_need_cpu > 0) {
        LOG(WARNING, "no enough resource");  
        for (size_t i = 0; i < sched_vec.size(); ++i) {
            sched_vec[i]->cpu_extra_need = 0;
            extra_queue_.push(sched_vec[i]); 
        }
        return false;
    }

    if (!ExecuteCpuNeed(&sched_vec)) {
        LOG(WARNING, "execute cpu need failed");
        TraceSchedule();
        return false;
    }

    cpu_cores_left_ -= cpu_cores;
    return true;
}

bool CpuScheduler::EnqueueTask(const std::string& cgroup_name,
                               int32_t cpu_quota) {
    MutexLock scoped_lock(&lock_);
    SchedulerCellsMap::iterator it = 
                            schedule_cells_.find(cgroup_name);  
    if (it != schedule_cells_.end()) {
        LOG(WARNING, "cgroup %s already scheduled",
                     cgroup_name.c_str()); 
        return true;
    }
    
    CpuSchedulerCell* cell = new CpuSchedulerCell(); 
    cell->cgroup_name = cgroup_name;
    cell->cpu_quota = cpu_quota;
    cell->cpu_guarantee = FLAGS_cpu_scheduler_guarantee;
    if (cell->cpu_guarantee > cell->cpu_quota) {
        cell->cpu_guarantee = cell->cpu_quota; 
    }
    cell->resource_collector = 
                    new CGroupResourceCollector(cgroup_name);
    cell->last_schedule_time = common::timer::get_micros() / 1000;
    schedule_cells_[cgroup_name] = cell;
    cell->resource_collector->CollectStatistics();
    return true;
}

void CpuScheduler::CalcCpuNeed(std::map<std::string, CpuSchedulerCell*>* sched_cells) {
    lock_.AssertHeld();
    if (sched_cells == NULL) {
        return; 
    }

    int64_t now_time = common::timer::get_micros() / 1000;
    SchedulerCellsMap::iterator it = schedule_cells_.begin();
    for (; it != schedule_cells_.end(); ++it) {
        CpuSchedulerCell* cell = it->second;     
        if (!cell->resource_collector->CollectStatistics()) {
            LOG(WARNING, "%s collect failed", cell->cgroup_name.c_str()); 
            continue;
        }
        if (cell->frozen) {
            if (cell->frozen_time > 0 
                    && now_time > cell->frozen_time) {
                cell->frozen = false; 
            } 
        }

        if (cell->frozen) {
            LOG(DEBUG, "%s in frozen state", 
                       cell->cgroup_name.c_str()); 
            continue;
        }

        cell->cpu_usages.push_back(
                cell->resource_collector->GetCpuUsage());
        LOG(DEBUG, "%s now_time %ld last_schedule_time %ld, cpu_usages %lf cpu cores %lf", 
                        cell->cgroup_name.c_str(),
                        now_time, 
                        cell->last_schedule_time, 
                        cell->cpu_usages.back(),
                        cell->resource_collector->GetCpuCoresUsage());
        if (now_time - cell->last_schedule_time 
                < FLAGS_cpu_scheduler_intervals) {
            continue; 
        }

        cell->last_schedule_time = now_time;
        cell->cpu_extra_need = 0;
        double avg_cpu_usages = 0;
        size_t cpu_usages_len = cell->cpu_usages.size();
        if (cpu_usages_len == 0) {
            LOG(WARNING, "no usages for %s, may schedule_intervals not right", 
                         cell->cgroup_name.c_str());
            continue; 
        }

        while (cell->cpu_usages.size() > 0) {
            avg_cpu_usages += cell->cpu_usages.front();
            cell->cpu_usages.pop_front();
        }
        avg_cpu_usages /= cpu_usages_len;
         
        int32_t cpu_idle_cores = static_cast<int32_t>(
                (1 - avg_cpu_usages) 
                * (cell->cpu_quota + cell->cpu_extra));
        if (cpu_idle_cores < 0) {
            cpu_idle_cores = 0; 
        } 
        if (cpu_idle_cores >= cpu_idle_low_limit_ 
                    && cpu_idle_cores <= cpu_idle_high_limit_) {
            LOG(WARNING, "%s cpu idle %d, [%d, %d]", 
                         cell->cgroup_name.c_str(),
                         cpu_idle_cores, cpu_idle_high_limit_, 
                         cpu_idle_low_limit_);
            continue;
        }

        if (cpu_idle_cores > cpu_idle_high_limit_) {
            if (cell->cpu_extra <= 0) {
                cell->cpu_extra_need = 
                        FLAGS_cpu_scheduler_dec * -1; 
                int32_t delta = cell->cpu_quota + cell->cpu_extra_need + cell->cpu_extra;
                if (delta < cell->cpu_guarantee) {
                    cell->cpu_extra_need = delta - cell->cpu_guarantee;
                    LOG(WARNING, "%s cpu guarantee extra_need %d", cell->cpu_extra_need);
                }
            } else {
                cell->cpu_extra_need = 0;
                cell->cpu_extra_need -= (cpu_idle_cores - cpu_idle_high_limit_);
                if (cell->cpu_extra_need + cell->cpu_extra <= 0) {
                    cell->cpu_extra_need = -1 * cell->cpu_extra; 
                }
            }     
        } else if (cpu_idle_cores < cpu_idle_low_limit_) {
            if (cell->cpu_extra < 0) {
                cell->cpu_extra_need = cell->cpu_extra * -1;
            } else {
                cell->cpu_extra_need = 0;
                cell->cpu_extra_need += cpu_idle_high_limit_;
            }
        }

        LOG(INFO, "%s cpu_extra %d cpu_extra_need %d avg_idle %d avg_cpu_usages_len %d", 
                cell->cgroup_name.c_str(), cell->cpu_extra, 
                cell->cpu_extra_need, cpu_idle_cores, cpu_usages_len); 
        sched_cells->insert(std::make_pair(cell->cgroup_name, cell));
    }
}

bool CpuScheduler::ExecuteCpuNeed(std::vector<CpuSchedulerCell*>* sched_vec) {
    lock_.AssertHeld();
    if (sched_vec == NULL) {
        return false; 
    }

    if (sched_vec->size() == 0) {
        return true; 
    }

    std::vector<CpuSchedulerCell*> extra_give;
    std::vector<CpuSchedulerCell*> extra_require;
    // TODO deque to vector
    std::vector<CpuSchedulerCell*>::iterator it = sched_vec->begin();
    for (; it != sched_vec->end(); ++it) {
        CpuSchedulerCell* cell = *it; 
        if (cell->cpu_extra_need > 0) {
            extra_require.push_back(cell); 
        } else {
            extra_give.push_back(cell); 
        }
    }

    for (size_t i = 0; i < extra_give.size(); ++i) {
        if (!ExecuteSchedulerCell(extra_give[i])) {
            return false; 
        }     
    }
    
    for (size_t i = 0; i < extra_require.size(); ++i) {
        if (!ExecuteSchedulerCell(extra_require[i])) {
            return false; 
        } 
    }
    return true;
}

bool CpuScheduler::ExecuteSchedulerCell(CpuSchedulerCell* cell) {
    lock_.AssertHeld();
    if (cell == NULL) {
        return false; 
    }
    if (cell->cpu_extra_need == 0) {
        return true; 
    }

    if (cell->cpu_extra_need > 0
            && cell->cpu_extra_need > cpu_cores_left_) {
        LOG(WARNING, "FATAL execute %s for need %d more than %d",
                     cell->cgroup_name.c_str(),
                     cell->cpu_extra_need, 
                     cpu_cores_left_); 
        return false;
    }
    long new_cpu_cores = cell->cpu_quota + cell->cpu_extra + cell->cpu_extra_need;
    long new_cpu_limit = new_cpu_cores * (CPU_CFS_PERIOD / 1000);
    LOG(DEBUG, "new cpu limit %ld quota %d cpu_extra %d cpu_extra_need %d",
                new_cpu_limit, cell->cpu_quota, cell->cpu_extra, cell->cpu_extra_need);
    if (new_cpu_limit < 0) {
        // maybe overhead ?
        LOG(WARNING, "FATAL new cpu limit %ld", new_cpu_limit);
        return false; 
    }
    if (new_cpu_limit < MIN_CPU_CFS_QUOTA) {
        new_cpu_limit = MIN_CPU_CFS_QUOTA; 
    }

    std::string hierarchy = FLAGS_gce_cgroup_root + "/cpu/";    
    if (0 != cgroups::Write(hierarchy, 
                        cell->cgroup_name, 
                        "cpu.cfs_quota_us", 
                        boost::lexical_cast<std::string>(new_cpu_limit))) {
        LOG(WARNING, "execute %s for need %d limit %ld failed", 
                cell->cgroup_name.c_str(),
                cell->cpu_extra_need,
                new_cpu_limit); 
        return false;
    } else {
        cpu_cores_left_ -= cell->cpu_extra_need; 
        cell->cpu_extra += cell->cpu_extra_need;
        LOG(WARNING, "execute %s for need %d limit %ld suc",
                cell->cgroup_name.c_str(),
                cell->cpu_extra_need,
                new_cpu_limit);
    }
    cell->resource_collector->Clear();
    return true;
}

bool CpuScheduler::ReCalcCpuNeed(std::map<std::string, CpuSchedulerCell*>* sched_cells) {
    lock_.AssertHeld();    
    if (sched_cells == NULL) {
        return false; 
    }

    if (sched_cells->size() == 0) {
        return true; 
    }

    std::vector<CpuSchedulerCell*> extra_give;
    std::vector<CpuSchedulerCell*> extra_left_require;
    std::vector<CpuSchedulerCell*> extra_right_require;
    int32_t temp_cpu_cores_left = cpu_cores_left_;
    int32_t extra_left_require_count = 0;
    std::map<std::string, CpuSchedulerCell*>::iterator it = sched_cells->begin();
    for (; it != sched_cells->end(); ++it) {
        CpuSchedulerCell* cell = it->second;
        if (cell->cpu_extra_need < 0) {
            extra_give.push_back(cell); 
            temp_cpu_cores_left -= cell->cpu_extra_need;
            continue;
        }

        if (cell->cpu_extra >= 0) {
            extra_right_require.push_back(cell); 
        } else {
            extra_left_require.push_back(cell); 
            extra_left_require_count += cell->cpu_extra_need;
        }
    }

    RebuildExtraQueue();
    while (temp_cpu_cores_left < extra_left_require_count) {
        CpuSchedulerCell* cell = extra_queue_.top();
        extra_queue_.pop();
        if (cell->cpu_extra > 0) {
            int32_t delta = extra_left_require_count - temp_cpu_cores_left;
            if (delta >= cell->cpu_extra) {
                delta = cell->cpu_extra; 
            }
            if (cell->cpu_extra_need > 0) {
                cell->cpu_extra_need = 0; 
            }

            cell->cpu_extra_need -= delta;
            temp_cpu_cores_left += delta;
            LOG(DEBUG, "borrow %s for left require %d", cell->cgroup_name.c_str(), delta);
            if (sched_cells->find(cell->cgroup_name) == sched_cells->end()) {
                sched_cells->insert(std::make_pair(cell->cgroup_name, cell)); 
            }
            continue;
        }    
        break;
    }

    if (temp_cpu_cores_left < extra_left_require_count) {
        return false; 
    }

    temp_cpu_cores_left -= extra_left_require_count;
    Shuffle(&extra_right_require);
    for (size_t i = 0; i < extra_right_require.size(); ++i) {
        CpuSchedulerCell* cell = extra_right_require[i];
        if (cell->cpu_extra_need < 0) {
            // may be used by left require
            continue; 
        }
        if (temp_cpu_cores_left <= 0) {
            cell->cpu_extra_need = 0; 
        } else if (temp_cpu_cores_left > cell->cpu_extra_need) {
            temp_cpu_cores_left -= cell->cpu_extra_need;    
        } else {
            cell->cpu_extra_need = temp_cpu_cores_left;
            temp_cpu_cores_left = 0;
        }
    }

    return true;
}

void CpuScheduler::RebuildExtraQueue() {
    lock_.AssertHeld();    
    while (extra_queue_.size() > 0) {
        extra_queue_.pop(); 
    }
    SchedulerCellsMap::iterator it = schedule_cells_.begin();
    for (; it != schedule_cells_.end(); ++it) {
        extra_queue_.push(it->second);     
    }
    return;
}

bool CpuScheduler::DequeueTask(const std::string& cgroup_name) {
    MutexLock scoped_lock(&lock_);
    SchedulerCellsMap::iterator it = 
                            schedule_cells_.find(cgroup_name);
    if (it == schedule_cells_.end()) {
        LOG(WARNING, "no cgroup %s in scheduler",
                     cgroup_name.c_str()); 
        return true;
    }

    CpuSchedulerCell* cell = it->second;
    schedule_cells_.erase(it);
    RebuildExtraQueue();
    if (cell->cpu_extra >= 0) {
        cpu_cores_left_ += cell->cpu_extra; 
        cell->cpu_extra = 0;
    } else if (cpu_cores_left_ + cell->cpu_extra >= 0) {
        cpu_cores_left_ += cell->cpu_extra; 
        cell->cpu_extra = 0;
    } else {
        if (!AllocateCpu(cell->cpu_extra * -1)) {
            TraceSchedule(); 
            return false;
        }
        cell->cpu_extra = 0;
    }
    delete cell->resource_collector;
    cell->resource_collector = NULL;
    delete cell;
    return true;
}

void CpuScheduler::TraceCpuSchedulerCells(const std::map<std::string, CpuSchedulerCell*> sched_cells) {
    if (sched_cells.size() == 0) {
        return; 
    }
    LOG(DEBUG, "======================");
    LOG(DEBUG, "name\tcpu_quota\tcpu_extra\tcpu_extra_need");
    std::map<std::string, CpuSchedulerCell*>::const_iterator it = sched_cells.begin();
    for (; it != sched_cells.end(); ++it) {
        CpuSchedulerCell* cell = it->second; 
        LOG(DEBUG, "%s\t%d\t%d\t%d",
                cell->cgroup_name.c_str(),
                cell->cpu_quota,
                cell->cpu_extra,
                cell->cpu_extra_need);
    }
    LOG(DEBUG, "=======================");
}

void CpuScheduler::ScheduleResourceUsage() {
    MutexLock scoped_lock(&lock_);
    std::map<std::string, CpuSchedulerCell*> sched_cells;
    std::vector<CpuSchedulerCell*> sched_vec;
    // 1. calc for need
    CalcCpuNeed(&sched_cells);
    do {
        if (!ReCalcCpuNeed(&sched_cells)) {
            LOG(WARNING, "cpu scheduler recalc failed"); 
            break;
        } 
        TraceCpuSchedulerCells(sched_cells);
        for (std::map<std::string, CpuSchedulerCell*>::iterator it = 
                sched_cells.begin(); it != sched_cells.end(); ++it) {
            sched_vec.push_back(it->second); 
        }
        if (!ExecuteCpuNeed(&sched_vec)) {
            LOG(WARNING, "cpu scheduler execute cpu need failed"); 
            break;
        }
    } while (0);

    RebuildExtraQueue();
    LOG(DEBUG, "cpu scheduler sched cpu core lefts %d",
            cpu_cores_left_);
    scheduler_threads_->DelayTask(collect_interval_, 
            boost::bind(&CpuScheduler::ScheduleResourceUsage, this));
    return;
}

}   // ending namespace galaxy
}   // ending namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
