// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/resource_collector.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <iostream>
#include "gflags/gflags.h"
#include "boost/lexical_cast.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "logging.h"
#include "agent/cgroups.h"

DECLARE_string(gce_cgroup_root);

namespace baidu {
namespace galaxy {

static long CPU_CORES = sysconf(_SC_NPROCESSORS_CONF);
static long MEMORY_PAGE_SIZE = sysconf(_SC_PAGESIZE);

static const std::string PROC_MOUNT_PATH = "/proc";

static const size_t PROC_STAT_FILE_SPLIT_SIZE = 44;
static const size_t PROC_STATUS_FILE_SPLIT_SIZE = 3;
static const int MIN_COLLECT_TIME = 2;

static bool GetCgroupCpuUsage(const std::string& cpu_path,
                              const std::string& cpu_acc_path,
                              CgroupResourceStatistics* statistics);

static bool GetCgroupMemoryUsage(const std::string& cgroup_path, 
                                 CgroupResourceStatistics* statistics);

// get total cpu usage from /proc/stat
static bool GetGlobalCpuUsage(ResourceStatistics* statistics);

static bool GetProcPidUsage(int pid, ResourceStatistics* statistics);

ProcResourceCollector::ProcResourceCollector(int pid) 
    : pid_(pid),
     global_statistics_prev_(),
     global_statistics_cur_(),
     process_statistics_prev_(),
     process_statistics_cur_(),
     timestamp_prev_(0.0),
     timestamp_cur_(0.0),
     collector_times_(0) {
}

ProcResourceCollector::~ProcResourceCollector() {
}

void ProcResourceCollector::ResetPid(int pid) {
    pid_ = pid;
}

void ProcResourceCollector::Clear() {
    collector_times_ = 0;
}

bool ProcResourceCollector::CollectStatistics() {
    global_statistics_prev_ = global_statistics_cur_;
    process_statistics_prev_ = process_statistics_cur_;
    timestamp_prev_ = timestamp_cur_;
    timestamp_cur_ = ::time(NULL);
    if (!GetGlobalCpuUsage(&global_statistics_cur_)) {
        return false; 
    }

    if (!GetProcPidUsage(pid_, &process_statistics_cur_)) {
        return false;
    }

    collector_times_++;
    return true;
}

double ProcResourceCollector::GetCpuCoresUsage() {
    return GetCpuUsage() * CPU_CORES;
}

long ProcResourceCollector::GetMemoryUsage() {
    return process_statistics_cur_.memory_rss_in_bytes;
}

CGroupResourceCollector::CGroupResourceCollector(const std::string& mem_path, 
                                                 const std::string& cpu_path,
                                                 const std::string& cpu_acc_path) : 
    global_statistics_prev_(),
    global_statistics_cur_(),
    cgroup_statistics_prev_(),
    cgroup_statistics_cur_(),
    mem_path_(mem_path),
    cpu_path_(cpu_path),
    cpu_acc_path_(cpu_acc_path),
    timestamp_prev_(0.0),
    timestamp_cur_(0.0),
    collector_times_(0) {
    LOG(INFO, "create resource collector for mem_path %s, cpu_path %s, cpu_acc_path %s",
            mem_path.c_str(),
            cpu_path.c_str(),
            cpu_acc_path.c_str());
}

CGroupResourceCollector::~CGroupResourceCollector() {
}

void CGroupResourceCollector::ResetCgroupName(
                            const std::string& mem_path,
                            const std::string& cpu_path,
                            const std::string& cpu_acc_path) {
    mem_path_ = mem_path;
    cpu_path_ = cpu_path;
    cpu_acc_path_ = cpu_acc_path;
    collector_times_ = 0;
}

void CGroupResourceCollector::Clear() {
    collector_times_ = 0;
}

double CGroupResourceCollector::GetCpuUsage() {
    if (cgroup_statistics_cur_.cpu_cores_limit == 0) {
        return 0.0; 
    }
    return GetCpuUsageInternal() * CPU_CORES / cgroup_statistics_cur_.cpu_cores_limit * 100000;
}

double CGroupResourceCollector::GetCpuUsageInternal() {
    if (collector_times_ < 2) {
        return 0.0; 
    } 
    long global_cpu_before = 
        global_statistics_prev_.cpu_user_time
        + global_statistics_prev_.cpu_nice_time
        + global_statistics_prev_.cpu_system_time
        + global_statistics_prev_.cpu_idle_time
        + global_statistics_prev_.cpu_iowait_time
        + global_statistics_prev_.cpu_irq_time
        + global_statistics_prev_.cpu_softirq_time
        + global_statistics_prev_.cpu_stealstolen
        + global_statistics_prev_.cpu_guest;

    long global_cpu_after =
        global_statistics_cur_.cpu_user_time
        + global_statistics_cur_.cpu_nice_time
        + global_statistics_cur_.cpu_system_time
        + global_statistics_cur_.cpu_idle_time
        + global_statistics_cur_.cpu_iowait_time
        + global_statistics_cur_.cpu_irq_time
        + global_statistics_cur_.cpu_softirq_time
        + global_statistics_cur_.cpu_stealstolen
        + global_statistics_cur_.cpu_guest;

    long cgroup_cpu_before = cgroup_statistics_prev_.cpu_user_time
        + cgroup_statistics_prev_.cpu_system_time;
    long cgroup_cpu_after = cgroup_statistics_cur_.cpu_user_time
        + cgroup_statistics_cur_.cpu_system_time;

    if (global_cpu_after - global_cpu_before <= 0
            || cgroup_cpu_after - cgroup_cpu_before < 0) {
        return 0.0; 
    }

    double rs = (cgroup_cpu_after - cgroup_cpu_before) 
                / (double) (global_cpu_after - global_cpu_before);
    return rs;
}

long CGroupResourceCollector::GetMemoryUsage() {
    if (collector_times_ < 1) {
        return 0L; 
    }
    return cgroup_statistics_cur_.memory_rss_in_bytes;
}

double CGroupResourceCollector::GetCpuCoresUsage() {
    return GetCpuUsageInternal() * CPU_CORES;
}

bool CGroupResourceCollector::CollectStatistics() {
    ResourceStatistics temp_global_statistics;
    CgroupResourceStatistics temp_cgroup_statistics;
    timestamp_prev_ = timestamp_cur_;
    timestamp_cur_ = ::time(NULL);

    if (!GetCgroupCpuUsage(cpu_path_,
                           cpu_acc_path_, 
                           &temp_cgroup_statistics)) {
        LOG(WARNING, "cgroup collector collect cpu usage failed %s", 
                     cpu_path_.c_str());
        return false;
    }

    if (!GetGlobalCpuUsage(&temp_global_statistics)) {
        LOG(WARNING, "cgroup collector collect global cpu usage failed %s",
                     cpu_path_.c_str()); 
        return false;
    }

    if (!GetCgroupMemoryUsage(mem_path_, &temp_cgroup_statistics)) {
        LOG(WARNING, "cgroup collector collect memory failed %s",
                     mem_path_.c_str()); 
        return false;
    }
    global_statistics_prev_ = global_statistics_cur_;
    cgroup_statistics_prev_ = cgroup_statistics_cur_;
    global_statistics_cur_ = temp_global_statistics;
    cgroup_statistics_cur_ = temp_cgroup_statistics;
    collector_times_ ++;
    return true;
}

double ProcResourceCollector::GetCpuUsage() {
    if (collector_times_ < MIN_COLLECT_TIME) {
        LOG(WARNING, "pid %d need try again", pid_); 
        return 0.0;
    }

    long global_cpu_before = 
            global_statistics_prev_.cpu_user_time
            + global_statistics_prev_.cpu_nice_time
            + global_statistics_prev_.cpu_system_time
            + global_statistics_prev_.cpu_idle_time
            + global_statistics_prev_.cpu_iowait_time
            + global_statistics_prev_.cpu_irq_time
            + global_statistics_prev_.cpu_softirq_time
            + global_statistics_prev_.cpu_stealstolen
            + global_statistics_prev_.cpu_guest;

    long global_cpu_after =  
            global_statistics_cur_.cpu_user_time
            + global_statistics_cur_.cpu_nice_time
            + global_statistics_cur_.cpu_system_time
            + global_statistics_cur_.cpu_idle_time
            + global_statistics_cur_.cpu_iowait_time
            + global_statistics_cur_.cpu_irq_time
            + global_statistics_cur_.cpu_softirq_time
            + global_statistics_cur_.cpu_stealstolen
            + global_statistics_cur_.cpu_guest;

    long proc_cpu_before = process_statistics_prev_.cpu_user_time 
        + process_statistics_prev_.cpu_system_time;
    long proc_cpu_after = process_statistics_cur_.cpu_user_time
        + process_statistics_cur_.cpu_system_time;

    if (global_cpu_after - global_cpu_before <= 0) {
        return 0.0; 
    }

    double rs = (proc_cpu_after - proc_cpu_before) 
                / static_cast<double>(global_cpu_after 
                                        - global_cpu_before);
    LOG(DEBUG, "process prev:%ld post:%ld global pre:%ld post:%ld rs: %lf",
            proc_cpu_before, 
            proc_cpu_after,
            global_cpu_before,
            global_cpu_after,
            rs);
    return rs;
}

bool GetGlobalCpuUsage(ResourceStatistics* statistics) {
    if (statistics == NULL) {
        return false; 
    }

    statistics->cpu_cores = CPU_CORES;
    std::string path = "/proc/stat";
    FILE* fin = fopen(path.c_str(), "r");
    if (fin == NULL) {
        LOG(WARNING, "open %s failed", path.c_str());
        return false; 
    }

    ssize_t read;
    size_t len = 0;
    char* line = NULL;
    if ((read = getline(&line, &len, fin)) == -1) {
        LOG(WARNING, "read line failed err[%d: %s]", 
                errno, strerror(errno)); 
        fclose(fin);
        return false;
    }
    fclose(fin);

    char cpu[5];
    int item_size = sscanf(line, 
                           "%s %ld %ld %ld %ld %ld %ld %ld %ld %ld", 
                           cpu,
                           &(statistics->cpu_user_time),
                           &(statistics->cpu_nice_time),
                           &(statistics->cpu_system_time),
                           &(statistics->cpu_idle_time),
                           &(statistics->cpu_iowait_time),
                           &(statistics->cpu_irq_time),
                           &(statistics->cpu_softirq_time),
                           &(statistics->cpu_stealstolen),
                           &(statistics->cpu_guest)); 

    free(line); 
    line = NULL;
    if (item_size != 10) {
        LOG(WARNING, "read from /proc/stat format err"); 
        return false;
    }
    return true;
}

bool GetProcPidUsage(int pid, ResourceStatistics* statistics) {
    std::string path = PROC_MOUNT_PATH + "/" + boost::lexical_cast<std::string>(pid) + "/stat";    
    FILE* fin = fopen(path.c_str(), "r");
    if (fin == NULL) {
        LOG(WARNING, "open %s failed", path.c_str()); 
        return false;
    }

    ssize_t read;
    size_t len = 0;
    char* line = NULL;

    if ((read = getline(&line, &len, fin)) == -1) {
        LOG(WARNING, "read line failed err[%d: %s]",
                errno, strerror(errno)); 
        fclose(fin);
        return false;
    }
    fclose(fin);

    std::vector<std::string> values;
    std::string str_line(line);
    boost::split(values, str_line, boost::is_any_of(" "));
    LOG(DEBUG, "split items %lu", values.size());
    free(line);
    line = NULL;
    if (values.size() < PROC_STAT_FILE_SPLIT_SIZE) {
        LOG(WARNING, "proc[%ld] stat file format err", pid);    
        return false;
    }
    statistics->cpu_user_time = atol(values[13].c_str());
    statistics->cpu_system_time = atol(values[14].c_str());
    statistics->memory_rss_in_bytes = atol(values[23].c_str()) * MEMORY_PAGE_SIZE;
    LOG(DEBUG, "get proc[%ld] user %ld sys %ld mem %ld", 
            pid, statistics->cpu_user_time, 
            statistics->cpu_system_time, 
            statistics->memory_rss_in_bytes);
    return true;
}

bool GetCgroupCpuUsage(const std::string& cpu_path, 
                       const std::string& cpu_acc_path,
                       CgroupResourceStatistics* statistics) {
    if (statistics == NULL) {
        return false; 
    }
    std::string value;
    if (0 != cgroups::Read(cpu_acc_path, 
                           "cpuacct.stat", 
                           &value)) {
        LOG(WARNING, "get cpuacct stat failed %s", 
                     cpu_acc_path.c_str()); 
        return false;
    }
    std::vector<std::string> lines;
    boost::split(lines, value, boost::is_any_of("\n"));
    int param_count = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string& line = lines[i]; 
        if (line.empty()) {
            continue; 
        }
        std::istringstream ss(line);
        std::string name;
        uint64_t use_time;
        ss >> name >> use_time;
        if (ss.fail()) {
            LOG(WARNING, "line format err %s", line.c_str()); 
            return false;
        }
        if (name == "user") {
            param_count ++;
            statistics->cpu_user_time = use_time; 
        } else if (name == "system") {
            param_count ++;
            statistics->cpu_system_time = use_time; 
        } else {
            LOG(WARNING, "invalid name %s %s", 
                         name.c_str(), line.c_str());
            return false; 
        }
    }
    if (param_count != 2) {
        return false; 
    }
    
    statistics->cpu_cores_limit = 0L;
    value = "";
    if (0 != cgroups::Read(cpu_path,
                           "cpu.cfs_quota_us",
                           &value)) {
        LOG(WARNING, "get cpu cfs_quota_us failed %s",
                     cpu_path.c_str()); 
        return false;
    }
    statistics->cpu_cores_limit = ::atol(value.c_str());
    if (statistics->cpu_cores_limit == 0) {
        return false; 
    }
    return true;
}

bool GetCgroupMemoryUsage(const std::string& group_path,
                          CgroupResourceStatistics* statistics) {
    if (statistics == NULL) {
        return false; 
    }
    std::string value;
    if (0 != cgroups::Read(group_path, 
                           "memory.stat", 
                           &value)) {
        LOG(WARNING, "get memory.stat falied %s",
                     group_path.c_str()); 
        return false;
    }
    std::vector<std::string> lines;
    boost::split(lines, value, boost::is_any_of("\n"));
    if (lines.size() <= 2) {
        LOG(WARNING, "read contents format err %s %s", 
                      group_path.c_str(), value.c_str()); 
        return false;
    }
    int mem_count = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string& line = lines[i]; 
        if (line.empty()) {
            continue; 
        }
        std::istringstream ss(line);
        std::string name;
        uint64_t val;
        ss >> name >> val;
        if (ss.fail()) {
            LOG(WARNING, "line format err %s", line.c_str()); 
            return false;
        }

        if (name == "rss") {
            mem_count++;
            statistics->memory_rss_in_bytes = val; 
        } else if (name == "cache") {
            mem_count++;
            statistics->memory_rss_in_bytes += val;
        } else if (mem_count == 2) {
            break; 
        }
    }
    if (mem_count == 2) {
        return true; 
    }
    return false;
}


}   // ending namespace galaxy
}   // ending namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
