// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _SRC_AGENT_RESOURCE_COLLECTOR_H
#define _SRC_AGENT_RESOURCE_COLLECTOR_H

#include <string>
#include <stdint.h>
#include <map>
#include <vector>
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
    long tmpfs_in_bytes;
        
    //io
    long rd_ios;
    long wr_ios;
    long rd_sectors;
    long wr_sectors;

    //interupt
    long interupt_times;
    long soft_interupt_times;

    //net
    long net_in_bits;
    long net_out_bits;
    long net_in_packets;
    long net_out_packets;
	
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
        memory_rss_in_bytes(0),
        tmpfs_in_bytes(0),
        rd_ios(0),
        wr_ios(0),
        rd_sectors(0),
        wr_sectors(0),
        interupt_times(0),
        soft_interupt_times(0),
        net_in_bits(0),
        net_out_bits(0),
        net_in_packets(0),
        net_out_packets(0) {
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

struct SysStat {
    ResourceStatistics last_stat_;
    ResourceStatistics cur_stat_;
    double cpu_used_;
    double mem_used_;
    double disk_read_Bps_;
    double disk_write_Bps_;
    double disk_read_times_;
    double disk_write_times_;
    double disk_io_util_;
    double net_in_bps_;
    double net_out_bps_;
    double net_in_pps_;
    double net_out_pps_;
    double intr_rate_;
    double soft_intr_rate_;
    int cpu_used_ex_;
    int mem_used_ex_;
    int disk_read_Bps_ex_;
    int disk_write_Bps_ex_;
    int disk_read_times_ex_;
    int disk_write_times_ex_;
    int disk_io_util_ex_;
    int net_in_bps_ex_;
    int net_out_bps_ex_;
    int net_in_pps_ex_;
    int net_out_pps_ex_;
    int intr_rate_ex_;
    int soft_intr_rate_ex_;
    bool cpu_used_busy_;
    bool mem_used_busy_;
    bool disk_read_Bps_busy_;
    bool disk_write_Bps_busy_;
    bool disk_read_times_busy_;
    bool disk_write_times_busy_;
    bool disk_io_util_busy_;
    bool net_in_bps_busy_;
    bool net_out_bps_busy_;
    bool net_in_pps_busy_;
    bool net_out_pps_busy_;
    bool intr_rate_busy_;
    bool soft_intr_rate_busy_;
    int collect_times_;
    SysStat():last_stat_(),
              cur_stat_(),
              cpu_used_(0.0),
              mem_used_(0.0),
              disk_read_Bps_(0.0),
              disk_write_Bps_(0.0),
              disk_read_times_(0.0),
              disk_write_times_(0.0),
              disk_io_util_(0.0),
              net_in_bps_(0.0),
              net_out_bps_(0.0),
              net_in_pps_(0.0),
              net_out_pps_(0.0),
              intr_rate_(0.0),
              soft_intr_rate_(0.0),
              cpu_used_ex_(0),
              mem_used_ex_(0),
              disk_read_Bps_ex_(0),
              disk_write_Bps_ex_(0),
              disk_read_times_ex_(0),
              disk_write_times_ex_(0),
              disk_io_util_ex_(0),
              net_in_bps_ex_(0),
              net_out_bps_ex_(0),
              net_in_pps_ex_(0),
              net_out_pps_ex_(0),
              intr_rate_ex_(0),
              soft_intr_rate_ex_(0),
              cpu_used_busy_(false),
              mem_used_busy_(false),
              disk_read_Bps_busy_(false),
              disk_write_Bps_busy_(false),
              disk_read_times_busy_(false),
              disk_write_times_busy_(false),
              disk_io_util_busy_(false),
              net_in_bps_busy_(false),
              net_out_bps_busy_(false),
              net_in_pps_busy_(false),
              net_out_pps_busy_(false),
              intr_rate_busy_(false),
              soft_intr_rate_busy_(false),
              collect_times_(0) {
        }
    ~SysStat(){
        }
};
//
struct ProcIOStatistics {
    int64_t rchar;
    int64_t wchar;
    int64_t syscr;
    int64_t syscw;
    int64_t read_bytes;
    int64_t write_bytes;
    int64_t cancelled_write_bytes;
};

struct BlkioStatistics {
    int64_t read;
    int64_t write;
    int64_t sync;
    int64_t async;
    int64_t total;
};

struct CGroupIOStatistics{
    std::map<int, ProcIOStatistics> processes;
    BlkioStatistics blkio;
};

class CGroupIOCollector{

public:
    CGroupIOCollector();
    ~CGroupIOCollector();
    static bool Collect(const std::string& freezer_path,
                        const std::string& blkio_path,
                        CGroupIOStatistics* stat);
private:
    static bool GetProcIOStat(int pid, ProcIOStatistics* stat);
    static bool GetBlkioStat(const std::string& group_path,
                             BlkioStatistics* blkio);
};

class CGroupResourceCollector : public ResourceCollector {
public:
    explicit CGroupResourceCollector(const std::string& mem_path, 
                                     const std::string& cpu_path,
                                     const std::string& cpu_acc_path);
    virtual ~CGroupResourceCollector();
    void ResetCgroupName(const std::string& mem_path,
                         const std::string& cpu_path,
                         const std::string& cpu_acc_path);
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
    std::string mem_path_;
    std::string cpu_path_;
    std::string cpu_acc_path_;
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

class GlobalResourceCollector {
public:
    explicit GlobalResourceCollector();
    virtual ~GlobalResourceCollector();
    int CollectStatistics();
    SysStat* GetStat(); 
private:
    bool GetGlobalCpuStat();
    bool GetGlobalMemStat();
    bool GetGlobalIntrStat();
    bool GetGlobalNetStat();
    bool GetGlobalIOStat();
    bool CheckSysHealth();
    bool IsItemBusy(const double value,
                    const double threshold,
                    int& ex_time,
                    const int max_ex_time,
                    bool& busy,
                    const std::string title);
private:
    SysStat* stat_;
};
}   // ending namespace galaxy
}   // ending namespace baidu

#endif  //_SRC_AGENT_RESOURCE_COLLECTOR_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
