// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _SRC_AGENT_RESOURCE_COLLECTOR_H
#define _SRC_AGENT_RESOURCE_COLLECTOR_H

#include <string>

namespace baidu {
namespace galaxy {

// NOTE all interface no thread safe 
class ResourceCollector {
public:
    virtual ~ResourceCollector() {}
    virtual bool CollectStatistics() = 0;
};

struct ResourceStatistics {
    // cpu
    long cpu_user_time; 
    long cpu_nice_time;
    long cpu_system_time;
    long cpu_idle_time;
    long cpu_iowait_time;
    long cpu_irq_time;
    long cpu_softirq_time;
    long cpu_stealstolen;
    long cpu_guest;

    long cpu_cores;

    long memory_rss_in_bytes;
    ResourceStatistics() :
        cpu_user_time(0),
        cpu_nice_time(0),
        cpu_system_time(0),
        cpu_idle_time(0),
        cpu_iowait_time(0),
        cpu_irq_time(0),
        cpu_softirq_time(0),
        cpu_stealstolen(0),
        cpu_guest(0),
        cpu_cores(0),
        memory_rss_in_bytes(0) {
    }
};

// unit jiffies
struct CgroupResourceStatistics {
    long cpu_user_time;
    long cpu_system_time;

    // cpu_cores from cfs_quota_usota_us
    long cpu_cores_limit;

    long memory_rss_in_bytes;
    CgroupResourceStatistics() :
        cpu_user_time(0),
        cpu_system_time(0),
        cpu_cores_limit(0),
        memory_rss_in_bytes(0) {
    }
};

class CGroupResourceCollector : public ResourceCollector {
public:
    explicit CGroupResourceCollector(const std::string& cgroup_name);
    virtual ~CGroupResourceCollector();
    void ResetCgroupName(const std::string& group_name);
    void Clear();
    double GetCpuUsage();
    long GetMemoryUsage();
    double GetCpuCoresUsage();
    virtual bool CollectStatistics();
private:
    double GetCpuUsageInternal();
    ResourceStatistics global_statistics_prev_;
    ResourceStatistics global_statistics_cur_;
    CgroupResourceStatistics cgroup_statistics_prev_;
    CgroupResourceStatistics cgroup_statistics_cur_;
    std::string cgroup_name_;
    double timestamp_prev_; 
    double timestamp_cur_;
    int collector_times_;
};

class ProcResourceCollector : public ResourceCollector {
public:
    explicit ProcResourceCollector(int pid);
    virtual ~ProcResourceCollector();
    void ResetPid(int pid);
    void Clear();
    double GetCpuUsage();
    long GetMemoryUsage();
    double GetCpuCoresUsage();
    virtual bool CollectStatistics();
private:
    int pid_;

    ResourceStatistics global_statistics_prev_;
    ResourceStatistics global_statistics_cur_;
    ResourceStatistics process_statistics_prev_;
    ResourceStatistics process_statistics_cur_;
    double timestamp_prev_; 
    double timestamp_cur_;
    int collector_times_;
};

}   // ending namespace galaxy
}   // ending namespace baidu

#endif  //_SRC_AGENT_RESOURCE_COLLECTOR_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
