// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#include "agent/dynamic_resource_scheduler.h"

#include <pthread.h>
#include <math.h>
#include <gflags/gflags.h>
#include <boost/bind.hpp>
#include "agent/resource_collector_engine.h"
#include "common/logging.h"
#include "common/util.h"

DECLARE_string(cgroup_root);
DECLARE_int32(dynamic_resource_scheduler_interval);
DECLARE_int32(max_cpu_usage_history_len);

DECLARE_int32(max_cpu_deinc_delta);

namespace galaxy {

static int CPU_CFS_PERIOD = 100000;
static int MIN_CPU_CFS_QUOTA = 1000;

static DynamicResourceScheduler* g_dynamic_scheduler = NULL;
static pthread_once_t  g_once = PTHREAD_ONCE_INIT;

static void DestroyDynamicResourceScheduler() {
    delete g_dynamic_scheduler;
    g_dynamic_scheduler = NULL;
    return;
}

static void InitDynamicResourceScheduler() {
    g_dynamic_scheduler = 
        new DynamicResourceScheduler(
                FLAGS_dynamic_resource_scheduler_interval);
    ::atexit(DestroyDynamicResourceScheduler);
    return;
}

DynamicResourceScheduler* GetDynamicResourceScheduler() {
    pthread_once(&g_once, InitDynamicResourceScheduler);
    return g_dynamic_scheduler;
}

DynamicResourceScheduler::DynamicResourceScheduler(
        int64_t schedule_interval)
    : schedule_interval_(schedule_interval),
      scheduler_thread_(NULL),
      lock_(),
      cpu_cores_left_(0),
      dynamic_quota_left_(0),
      left_threshold_in_quota_(60),
      right_threshold_in_quota_(100),
      left_threshold_out_of_quota_(60),
      right_threshold_out_of_quota_(100),
      extra_queue_(),
      stop_(false) {
    scheduler_thread_ = new ThreadPool(1);
    scheduler_thread_->DelayTask(schedule_interval_, 
            boost::bind(&DynamicResourceScheduler::ScheduleResourceUsage, 
                this));
}

DynamicResourceScheduler::~DynamicResourceScheduler() {
    if (scheduler_thread_ != NULL) {
        delete scheduler_thread_; 
        scheduler_thread_ = NULL;
    }
    ScheduleCellsMap::iterator it = 
        schedule_cells_.begin(); 
    ResourceCollectorEngine* engine =
        GetResourceCollectorEngine();
    if (engine == NULL) {
        // engine destroy at exit no need to clear memory
        return; 
    }
    for (; it != schedule_cells_.end(); ++it) {
        int64_t collector_id = it->second.collector_id;
        engine->DelCollector(collector_id);
        delete it->second.collector;
        it->second.collector = NULL;
    }
}

void DynamicResourceScheduler::SetFrozen(const std::string& cgroup_name,
        int64_t frozen_time) {
    common::MutexLock scope_lock(&lock_);
    int64_t now_time = common::timer::now_time();
    ScheduleCellsMap::iterator it = schedule_cells_.find(cgroup_name);
    if (it != schedule_cells_.end()) {
        LOG(WARNING, "%s set frozen time %ld", cgroup_name.c_str(), frozen_time);
        it->second.frozen_schedule = true;     
        if (frozen_time > 0) {
            it->second.frozen_time = frozen_time + now_time;
        } else {
            it->second.frozen_time = frozen_time;
        }
    }
}

void DynamicResourceScheduler::UnFrozen(const std::string& cgroup_name) {
    common::MutexLock scope_lock(&lock_);
    ScheduleCellsMap::iterator it = schedule_cells_.find(cgroup_name);
    if (it != schedule_cells_.end()) {
        LOG(WARNING, "%s unfrozen", cgroup_name.c_str());
        it->second.frozen_schedule = false;     
        it->second.frozen_schedule = -1;
    }
}

void DynamicResourceScheduler::KeepQuota(const std::string& /*cgroup_name*/) {
    // TODO
}

void DynamicResourceScheduler::RegistCgroupPath(
        const std::string& cgroup_name, double cpu_quota, double cpu_limit) {
    ScheduleCell cell;
    cell.cgroup_name = cgroup_name;
    cell.cpu_quota = int64_t(cpu_quota * 100);
    cell.cpu_limit = int64_t(cpu_limit * 100);
    cell.cpu_extra = 0;
    cell.collector_id = -1;
    cell.last_scheduler_time = 0;
    cell.cpu_usage = 0.0;
    cell.cpu_extra_need = 0;
    cell.frozen_schedule = true;
    cell.frozen_time = -1;
    common::MutexLock scope_lock(&lock_);
    CGroupResourceCollector* collector = 
        new CGroupResourceCollector(cgroup_name);
    ResourceCollectorEngine* engine = 
        GetResourceCollectorEngine();
    cell.collector_id = engine->AddCollector(collector);
    cell.collector = collector;
    schedule_cells_[cgroup_name] = cell;
    LOG(INFO, "[DYNAMIC_SCHEDULE] regist %s cpu_limit %ld frozen time %ld", 
            cell.cgroup_name.c_str(), cell.cpu_limit, cell.frozen_time);
    return;
}

bool DynamicResourceScheduler::UnRegistCgroupPath(
        const std::string& cgroup_name) {
    common::MutexLock scope_lock(&lock_);
    ResourceCollectorEngine* engine =
        GetResourceCollectorEngine();
    ScheduleCellsMap::iterator it = 
        schedule_cells_.find(cgroup_name); 
    LOG(WARNING, "unregist %s", cgroup_name.c_str());
    if (it != schedule_cells_.end()) {
        ScheduleCell& cell = it->second;
        long tmp_cpu_cores_left_ = cpu_cores_left_;
        tmp_cpu_cores_left_ += cell.cpu_extra;
        while (tmp_cpu_cores_left_ < 0) {
            if (extra_queue_.size() == 0
                    || extra_queue_.top()->cpu_extra < 0) {
                LOG(WARNING, "[DYNAMIC_SCHEDULE] no extra meet for left"); 
                return false;
            }
            ScheduleCell* extra_cell = extra_queue_.top();         
            extra_queue_.pop();
            if (extra_cell->cgroup_name == cgroup_name) {
                continue;     
            }
            extra_cell->cpu_extra_need = 0;
            if (extra_cell->cpu_extra + tmp_cpu_cores_left_ >= 0) {
                LOG(DEBUG, "cgroup %s extra %ld can meet left %ld",
                        extra_cell->cgroup_name.c_str(), 
                        extra_cell->cpu_extra, tmp_cpu_cores_left_);
                extra_cell->cpu_extra_need = tmp_cpu_cores_left_;
                if (!ExecuteScheduleCell(extra_cell)) {
                    LOG(WARNING, "[DYNAMIC_SCHEDULE] unregist "
                            "failed for execute %s",
                            extra_cell->cgroup_name.c_str());    
                    return false;
                }
                tmp_cpu_cores_left_ = 0;
            } else {
                extra_cell->cpu_extra_need -= extra_cell->cpu_extra; 
                if (!ExecuteScheduleCell(extra_cell)) {
                    LOG(WARNING, "[DYNAMIC_SCHEDULE] unregist "
                            "failed for execute %s", 
                            extra_cell->cgroup_name.c_str()); 
                    return false;
                }
                LOG(DEBUG, "cgroup %s extra %ld can meet left %ld",
                        extra_cell->cgroup_name.c_str(), extra_cell->cpu_extra,
                        tmp_cpu_cores_left_);
                tmp_cpu_cores_left_ -= extra_cell->cpu_extra;
            }
            extra_queue_.push(extra_cell);
        }
        if (cpu_cores_left_ + cell.cpu_extra >= 0) {
            cpu_cores_left_ += cell.cpu_extra;
            int64_t collector_id = cell.collector_id; 
            engine->DelCollector(collector_id);
            delete cell.collector;
            cell.collector = NULL;
            schedule_cells_.erase(it);
            LOG(DEBUG, "unregist to core left %ld",
                    cpu_cores_left_);
            return true; 
        } else {
            return false; 
        }
    }
    LOG(WARNING, "[DYNAMIC_SCHEDULE] unregist not in map %s",
            cgroup_name.c_str());
    return false;
}

void DynamicResourceScheduler::Release(
        const TaskResourceRequirement& requirement) {
    // use by resource_manager get "quota cpu"  
    common::MutexLock scope_lock(&lock_);
    cpu_cores_left_ += static_cast<long>(
            requirement.cpu_limit * 100);
    LOG(WARNING, "release to %ld cpu cores", cpu_cores_left_);
    return;
}


int DynamicResourceScheduler::Allocate(
        const TaskResourceRequirement& requirement) {
    common::MutexLock scope_lock(&lock_); 
    long require = static_cast<long>(requirement.cpu_limit * 100); 
    if (cpu_cores_left_ >= require) {
        cpu_cores_left_ -= require; 
        return 0;
    }
    long need = require - cpu_cores_left_;
    while (need > 0) {
        if (extra_queue_.size() == 0
                || extra_queue_.top()->cpu_extra <= 0) {
            LOG(WARNING, "[DYNAMIC_SCHEDULE] require(%ld) "
                    "more than scheudle",
                    require);     
            return -1;
        }
        ScheduleCell* cell = extra_queue_.top();
        extra_queue_.pop();
        if (cell->cpu_extra >= need) {
            cell->cpu_extra_need = 0; 
            cell->cpu_extra_need -= need;
            if (!ExecuteScheduleCell(cell)) {
                LOG(WARNING, "[DYNAMIC_SCHEDULE] setup need failed %s",
                        cell->cgroup_name.c_str());
                return -1;
            }
            need = 0;
        } else {
            cell->cpu_extra_need -= cell->cpu_extra;
            if (!ExecuteScheduleCell(cell)) {
                LOG(WARNING, "[DYNAMIC_SCHEDULE] setup need failed %s",
                        cell->cgroup_name.c_str());
                return -1;
            }
            need -= cell->cpu_extra;
        }
        extra_queue_.push(cell);
    }
    
    if (cpu_cores_left_ >= require) {
        cpu_cores_left_ -= require; 
        return 0;
    }

    return -1;
}

static bool ScheduleExtraNeedComp(
        ScheduleCell* left, ScheduleCell* right) {
    long left_val = 0;
    long right_val = 0;
    if (left->cpu_extra_need < 0) {
        left_val = labs(left->cpu_extra_need); 
    } 
    if (right->cpu_extra_need < 0) {
        right_val = labs(right->cpu_extra_need); 
    }

    if (left_val == 0 && right_val == 0) {
        return left->cpu_extra > right->cpu_extra; 
    }

    return left_val > right_val;
}

bool DynamicResourceScheduler::ReCalcCpuNeed(
        std::vector<ScheduleCell*>* schedule_vector) {
    lock_.AssertHeld();
    if (schedule_vector == NULL) {
        return false; 
    }

    LOG(DEBUG, "recalc cpu need vector %u", 
            schedule_vector->size());
    if (schedule_vector->size() == 0) {
        return true;
    }

    std::vector<ScheduleCell*>::iterator it = 
        schedule_vector->begin();
    // cpu_extra < 0 == left
    std::vector<ScheduleCell*> extra_left;
    // cpu_extra >= 0 == right
    std::vector<ScheduleCell*> extra_right;
    long extra_left_need = 0;
    long extra_right_need = 0;
    for (; it != schedule_vector->end(); ++it) {
        ScheduleCell* cell = *it;
        if (cell->cpu_extra >= 0) {
            extra_right.push_back(cell); 
            extra_right_need += cell->cpu_extra_need;
        } else {
            extra_left.push_back(cell); 
            extra_left_need += cell->cpu_extra_need;
        }
    }

    LOG(DEBUG, "[DYNAMIC_SCHEDULE] extra left %lu extra right %lu", 
            extra_left.size(), extra_right.size());

    long cpu_cores_may_left = cpu_cores_left_;
    if (cpu_cores_may_left < 0) {
        LOG(WARNING, "[DYNAMIC_SCHEDULE] cpu cores "
                "left(%ld) < 0, recalc failed",
                cpu_cores_left_); 
        return false;
    }

    // each quota keep
    if (extra_left_need > 0) {
        if (cpu_cores_left_ > 0 
                && extra_left_need < cpu_cores_may_left) {
            LOG(DEBUG, "[DYNAMIC_SCHEDULE] extra left need[%ld] meet by left cores[%ld]",
                    extra_left_need, cpu_cores_may_left);
            cpu_cores_may_left -= extra_left_need;
        } else {
            // recalc extra_right_need 
            extra_right_need = 0;
            std::sort(extra_right.begin(), extra_right.end(), 
                    ScheduleExtraNeedComp);
            long tmp_extra_left_need = 
                extra_left_need - cpu_cores_may_left;
            cpu_cores_may_left = 0;
            for (size_t i = 0; i < extra_right.size(); ++i) {
                if (extra_right[i]->cpu_extra_need < 0) {
                    LOG(DEBUG, "[DYNAMIC_SCHEDULE] need left[%ld] extra right cpu_extra_need[%ld]",
                            tmp_extra_left_need, extra_right[i]->cpu_extra_need);
                    tmp_extra_left_need += 
                        extra_right[i]->cpu_extra_need;     
                } else {
                    extra_right[i]->cpu_extra_need = 0;
                    LOG(DEBUG, "[DYNAMIC_SCHEDULE] bf need left[%ld] extra right "
                            "cpu_extra[%ld] cpu_extra_need[%ld] %s",
                            tmp_extra_left_need, extra_right[i]->cpu_extra, 
                            extra_right[i]->cpu_extra_need, 
                            extra_right[i]->cgroup_name.c_str());
                    if (extra_right[i]->cpu_extra >= 0) {
                        if (extra_right[i]->cpu_extra 
                                >= tmp_extra_left_need) {
                            extra_right[i]->cpu_extra_need 
                                -= tmp_extra_left_need;     
                            tmp_extra_left_need = 0;
                        } else {
                            extra_right[i]->cpu_extra_need 
                                -= extra_right[i]->cpu_extra;
                        }   
                    } else {
                        // TODO 
                        LOG(WARNING, "[DYNAMIC_SCHEDULE] extra_right vector "
                                "should not cpu_extra(%ld) < 0 cgroup(%s)",
                                extra_right[i]->cpu_extra, 
                                extra_right[i]->cgroup_name.c_str());
                        return false;
                    }
                    LOG(DEBUG, "[DYNAMIC_SCHEDULE] af need left[%ld] extra right "
                            "cpu_extra[%ld] cpu_extra_need[%ld] %s",
                            tmp_extra_left_need, extra_right[i]->cpu_extra, 
                            extra_right[i]->cpu_extra_need,
                            extra_right[i]->cgroup_name.c_str());
                } 
                extra_right_need += extra_right[i]->cpu_extra_need;
            }

            if (tmp_extra_left_need > 0) {
                // TODO
                LOG(WARNING, "[DYNAMIC_SCHEDULE] quota need(%ld) more than left(%ld)",
                        tmp_extra_left_need, cpu_cores_may_left);
                return false;
            }
        }
    } else {
        cpu_cores_may_left -= extra_left_need; 
    }

    // overmuch require fix
    if (extra_right_need > 0) {
        for (size_t i = 0; i < extra_right.size(); i++) {
            if (extra_right[i]->cpu_extra_need <= 0) {
                continue; 
            }
            if (cpu_cores_may_left <= 0
                    && extra_right[i]->cpu_extra_need > 0) {
                LOG(DEBUG, "[DYNAMIC_SCHEDULE] extra right need[%ld] can't meet by cores left[%ld] %s",
                        extra_right[i]->cpu_extra_need, cpu_cores_may_left,
                        extra_right[i]->cgroup_name.c_str());
                // TODO cpu extra > 0 schedule
                extra_right[i]->cpu_extra_need = 0;     
                continue;
            }

            if (extra_right[i]->cpu_extra_need >= cpu_cores_may_left) {
                long tmp_extra_need = extra_right[i]->cpu_extra_need;
                extra_right[i]->cpu_extra_need = cpu_cores_may_left;
                cpu_cores_may_left = 0;     
                LOG(DEBUG, "[DYNAMIC_SCHEDULE] extra right %s need[%ld] to need[%ld]",
                        extra_right[i]->cgroup_name.c_str(), tmp_extra_need, 
                        extra_right[i]->cpu_extra_need);
            } else {
                cpu_cores_may_left -= extra_right[i]->cpu_extra_need;
            }
        } 
    } 
    return true; 
}

static void LogTrace(const std::vector<ScheduleCell*> sched_vector) {
    if (sched_vector.size() == 0) {
        return; 
    }
    LOG(DEBUG, "================================");
    LOG(DEBUG, "index\tcgroup\tcpu_limit\tcpu_extra\tcpu_extra_need");
    for (size_t i = 0; i < sched_vector.size(); i++) {
        ScheduleCell* cell = sched_vector[i]; 
        LOG(DEBUG, "%d\t%s\t%ld\t%ld\t%ld",
                i,
                cell->cgroup_name.c_str(),
                cell->cpu_limit,
                cell->cpu_extra,
                cell->cpu_extra_need);
    } 
    LOG(DEBUG, "================================");
}

bool DynamicResourceScheduler::ExecuteScheduleCell(
        ScheduleCell* cell) {
    if (cell == NULL) {
        return false; 
    }

    if (cell->cpu_extra_need > 0 
            && cell->cpu_extra_need > cpu_cores_left_) {
        LOG(WARNING, "[DYNAMIC_SCHEDULE] execute %s for "
                "need(%ld) more than left(%ld)",
                cell->cgroup_name.c_str(), 
                cell->cpu_extra_need, cpu_cores_left_);
        return false; 
    }
    long new_cpu_limit = (cell->cpu_limit + cell->cpu_extra 
                            + cell->cpu_extra_need) * CPU_CFS_PERIOD / 100;
    if (new_cpu_limit < MIN_CPU_CFS_QUOTA) {
        new_cpu_limit = MIN_CPU_CFS_QUOTA; 
    }

    std::string cpu_cfs_quota_us = 
        FLAGS_cgroup_root + "/cpu/" 
        + cell->cgroup_name + "/cpu.cfs_quota_us";
    int ret = common::util::WriteIntToFile(
            cpu_cfs_quota_us, new_cpu_limit);
    if (ret == 0) {
        LOG(WARNING, "[DYNAMIC_SCHEDULE] execute %s for need(%ld) suc",
                cell->cgroup_name.c_str(), cell->cpu_extra_need);
        cpu_cores_left_ -= cell->cpu_extra_need; 
        cell->cpu_extra += cell->cpu_extra_need;
        // CLEAR usages in old config
        cell->cpu_usages.clear();
        return true;
    } 
    LOG(WARNING, "[DYNAMIC_SCHEDULE] execute %s for need(%ld) failed",
            cell->cgroup_name.c_str(), cell->cpu_extra_need); 
    return false;
}

bool DynamicResourceScheduler::ExecuteCpuNeed(
        std::vector<ScheduleCell*>* schedule_vector) { 
    lock_.AssertHeld();
    if (schedule_vector == NULL) {
        return false; 
    }
    LOG(DEBUG, "execute cpu need vector %u", schedule_vector->size());
    if (schedule_vector->size() == 0) {
        return true; 
    }
    std::vector<ScheduleCell*> extra_give;
    std::vector<ScheduleCell*> extra_require;

    std::vector<ScheduleCell*>::iterator it = schedule_vector->begin();
    for (; it != schedule_vector->end(); ++it) {
        ScheduleCell* cell = *it;     
        if (cell->cpu_extra_need < 0) {
            extra_give.push_back(cell); 
        } else if (cell->cpu_extra_need > 0) {
            extra_require.push_back(cell); 
        }
    }
    
    for (size_t i = 0; i < extra_give.size(); i++) {
        ScheduleCell* cell = extra_give[i];     
        if (cell->cpu_extra_need > 0) {
            LOG(WARNING, 
                    "[DYNAMIC_SCHEDULE] extra cpu need(%ld) err cgroup(%s)",
                    cell->cpu_extra_need, cell->cgroup_name.c_str());
            return false; 
        } 

        if (!ExecuteScheduleCell(cell)) {
            return false;
        }
    }

    for (size_t i = 0; i < extra_require.size(); i++) {
        ScheduleCell* cell = extra_require[i]; 
        if (!ExecuteScheduleCell(cell)) {
            return false; 
        }
    }
    return true;
}

void DynamicResourceScheduler::CalcCpuNeed(
        std::vector<ScheduleCell*>* schedule_vector) {
    lock_.AssertHeld();
    LOG(DEBUG, "[DYNAMIC_SCHEDULE] calc cpu need schedule vector size %lu",
            schedule_cells_.size());
    if (schedule_vector == NULL) {
        return; 
    }
    int64_t now_time = common::timer::now_time();
    ScheduleCellsMap::iterator cell_it = schedule_cells_.begin();  
    for (; cell_it != schedule_cells_.end(); ++cell_it) {
        ScheduleCell& cell = cell_it->second;                  
        if (cell.frozen_schedule
                && (cell.frozen_time > 0 
                    && cell.frozen_time < now_time)) {
            LOG(DEBUG, "[DYNAMIC_SCHEDULE] %s cpu schedule frozen state",
                    cell.cgroup_name.c_str());
            cell.cpu_extra_need = 0; 
            schedule_vector->push_back(&cell);
            continue; 
        }

        if (cell.frozen_time > 0 
                && cell.frozen_time >= now_time) {
            cell.frozen_schedule = false; 
            cell.cpu_usages.clear();
        }

        cell.cpu_usage = cell.collector->GetCpuUsage();
        cell.cpu_usages.push_back(cell.cpu_usage);
        if ((int)cell.cpu_usages.size() 
                <= FLAGS_max_cpu_usage_history_len) {
            LOG(DEBUG, "[DYNAMIC_SCHEDULE] %s cpu "
                    "usages check point len %lu usage %lf",
                    cell.cgroup_name.c_str(), cell.cpu_usages.size(),
                    cell.cpu_usage);
            cell.cpu_extra_need = 0; 
            schedule_vector->push_back(&cell);
            continue; 
        } else {
            cell.cpu_usages.pop_front();
        }

        std::deque<double> temp_cpu_usage = cell.cpu_usages;
        double avg_usage = 0.0;
        while (temp_cpu_usage.size() > 0) {
            avg_usage += temp_cpu_usage.front();     
            temp_cpu_usage.pop_front();
        }
        
        // TODO calc avg value for idle check
        avg_usage = avg_usage / cell.cpu_usages.size();
        double avg_idle_usage = 1 - avg_usage;
        if (avg_idle_usage < 0) {
            avg_idle_usage = 0;     
        }
        long avg_idle_cores = long(avg_idle_usage * 
                (cell.cpu_limit + cell.cpu_extra));
        cell.cpu_extra_need = 0;
        LOG(DEBUG, "[DYNAMIC_SCHEDULE] avg_idle_cores %ld avg_usage %lf(%ld) cgroup %s", 
                avg_idle_cores, avg_usage, cell.cpu_extra + cell.cpu_limit, cell.cgroup_name.c_str());

        // TODO need update for need calc
        if (cell.cpu_extra < 0) {
            if (avg_idle_cores > right_threshold_in_quota_) {
                cell.cpu_extra_need -= FLAGS_max_cpu_deinc_delta;
            } else if (avg_idle_cores < left_threshold_in_quota_) {
                cell.cpu_extra_need += labs(cell.cpu_extra);
            }
        } else if (cell.cpu_extra > 0) {
            if (avg_idle_cores > right_threshold_in_quota_) {
                cell.cpu_extra_need -= 
                    (avg_idle_cores - right_threshold_in_quota_);
            } else if (avg_idle_cores < left_threshold_in_quota_) {
                cell.cpu_extra_need += right_threshold_in_quota_; 
            } 
        } else {
            if (avg_idle_cores > right_threshold_in_quota_) {
                cell.cpu_extra_need -= FLAGS_max_cpu_deinc_delta;
            } else if (avg_idle_cores < left_threshold_in_quota_) {
                cell.cpu_extra_need += right_threshold_in_quota_; 
            }
        }
        LOG(DEBUG, "[DYNAMIC_SCHEDULE] cpu_extra_need %ld cgroup %s", 
                cell.cpu_extra_need, cell.cgroup_name.c_str());
        schedule_vector->push_back(&cell);
    } 
    LOG(DEBUG, "schedule vector size %u", schedule_vector->size());
    return;
}

void DynamicResourceScheduler::ScheduleResourceUsage() {
    common::MutexLock scope_lock(&lock_);    
    if (stop_) {
        return; 
    }
    std::vector<ScheduleCell*> schedule_vector;
    // 1. calc for need
    CalcCpuNeed(&schedule_vector);
    LogTrace(schedule_vector);
    do {
        // 2. recalc for alloc 
        if (!ReCalcCpuNeed(&schedule_vector)) {
            LOG(WARNING, "[DYNAMIC_SCHEDULE] recalc failed");    
            break;
        }
        LogTrace(schedule_vector);
        // 3. execute
        if (!ExecuteCpuNeed(&schedule_vector)) {
            LOG(WARNING, "[DYNAMIC_SCHEDULE] execute cpu need failed"); 
            break;
        }         
    } while (0);
    // 4. requeue 
    SchedulePriorityQueue queue;
    std::vector<ScheduleCell*>::iterator it = schedule_vector.begin();
    for (; it != schedule_vector.end(); ++it) {
        queue.push(*it); 
    }

    extra_queue_ = queue;
    LOG(DEBUG, "[DYNAMIC_SCHEDULE] schedule cpu cores left %ld",
            cpu_cores_left_);
    scheduler_thread_->DelayTask(schedule_interval_, 
            boost::bind(&DynamicResourceScheduler::ScheduleResourceUsage, 
                this));
    return;
}

bool DynamicResourceScheduler::DumpPersistenceInfo(
        DynamicSchedulerPersistence* info) {
    if (info == NULL) {
        return false; 
    }

    common::MutexLock scope_lock(&lock_);    
    info->set_cpu_cores_left(cpu_cores_left_);
    info->set_left_threshold(left_threshold_in_quota_);
    info->set_right_threshold(right_threshold_in_quota_);
    
    ScheduleCellsMap::iterator it = schedule_cells_.begin();
    for (; it != schedule_cells_.end(); ++it) {
        ScheduleCell& cell = it->second;     
        SchedulerCellPersistence* p_cell = info->add_cells();
        p_cell->set_cgroup_name(cell.cgroup_name);
        p_cell->set_cpu_quota(cell.cpu_quota);
        p_cell->set_cpu_limit(cell.cpu_limit);
        p_cell->set_cpu_extra(cell.cpu_extra);
        p_cell->set_frozen_schedule(cell.frozen_schedule);
        p_cell->set_frozen_time(cell.frozen_time);
    }
    return true;
}

bool DynamicResourceScheduler::LoadPersistenceInfo(
        const DynamicSchedulerPersistence& info) {
    common::MutexLock scope_lock(&lock_);    
    if (info.has_cpu_cores_left()) {
        cpu_cores_left_ = info.cpu_cores_left();
    } else {
        return false; 
    }
    if (info.has_left_threshold()) {
        left_threshold_in_quota_ = info.left_threshold();
    } else {
        return false; 
    }
    if (info.has_right_threshold()) {
        right_threshold_in_quota_ = info.right_threshold();
    } else {
        return false; 
    }

    schedule_cells_.clear();
    for (int i = 0; i < info.cells_size(); i++) {
        SchedulerCellPersistence p_cell = info.cells(i); 
        ScheduleCell cell;  
        cell.cgroup_name = p_cell.cgroup_name();
        cell.cpu_quota = p_cell.cpu_quota();
        cell.cpu_limit = p_cell.cpu_limit();
        cell.cpu_extra = p_cell.cpu_extra();
        cell.last_scheduler_time = 0;
        cell.collector = NULL;
        cell.collector_id = -1;
        cell.cpu_extra_need = 0;
        cell.frozen_schedule = p_cell.frozen_schedule();
        cell.frozen_time = p_cell.frozen_time();
        CGroupResourceCollector* collector = 
            new CGroupResourceCollector(cell.cgroup_name);
        ResourceCollectorEngine* engine = 
            GetResourceCollectorEngine();
        cell.collector_id = engine->AddCollector(collector);
        cell.collector = collector;
        schedule_cells_[cell.cgroup_name] = cell;
        LOG(INFO, "[DYNAMIC_SCHEDULE] load %s "
                "cpu_limit %ld cpu_extra %ld", 
                cell.cgroup_name.c_str(), 
                cell.cpu_limit, cell.cpu_extra);
    }
    return true; 
}

}   // ending namespace galaxy

/* vim: set ts=4 sw=4 sts=4 tw=100 */
