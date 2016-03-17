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
#include <fstream>
#include <sys/stat.h>
#include "gflags/gflags.h"
#include "boost/lexical_cast.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "logging.h"
#include "agent/cgroups.h"
#include "agent/utils.h"

DECLARE_string(gce_cgroup_root);
DECLARE_int32(stat_check_period);
DECLARE_double(max_cpu_usage);
DECLARE_double(max_mem_usage);
DECLARE_double(max_disk_r_kbps);
DECLARE_double(max_disk_w_kbps);
DECLARE_double(max_disk_r_rate);
DECLARE_double(max_disk_w_rate);
DECLARE_double(max_disk_util);
DECLARE_double(max_net_in_bps);
DECLARE_double(max_net_out_bps);
DECLARE_double(max_net_in_pps);
DECLARE_double(max_net_out_pps);
DECLARE_double(max_intr_rate);
DECLARE_double(max_soft_intr_rate);
DECLARE_int32(max_ex_time);
DECLARE_string(agent_work_dir);
DECLARE_string(loop_dev_file);

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

GlobalResourceCollector::GlobalResourceCollector() {
    stat_ = new SysStat();
}

GlobalResourceCollector::~GlobalResourceCollector() {
    delete stat_;
}

SysStat* GlobalResourceCollector::GetStat() {
    return stat_;
}

bool GlobalResourceCollector::IsItemBusy(const double value, 
                    const double threshold,
                    int& ex_time,
                    const int max_ex_time,
                    bool& busy,
                    const std::string title) {
    if (fabs(threshold) < 1e-6) {
        return false;
    }
    if (value > threshold) {
        ex_time++;
        LOG(WARNING, "%s usage %f reach threshold %f ex %d", title.c_str(),
                value, threshold, ex_time);
        if (ex_time >= max_ex_time) {
            ex_time = max_ex_time;
            busy = true;
            LOG(WARNING, "item %s set busy", title.c_str());
        }
    } else {
        ex_time--;
        LOG(WARNING, "%s usage %f under threshold %f ex %d", title.c_str(),
                value, threshold, ex_time);
        if (ex_time <= 0) {
            busy = false;
            ex_time = 0;
            LOG(WARNING, "item %s set idle", title.c_str());
        }
    }
    return busy;
}

int GlobalResourceCollector::CollectStatistics() {
    LOG(INFO, "start collect sys stat");
    stat_->last_stat_ = stat_->cur_stat_;        
    bool ok = GetGlobalCpuStat();
    if (!ok) {
        LOG(WARNING, "fail to get cpu usage");
        return 1;
    }
    ok = GetGlobalMemStat();
    if (!ok) {
        LOG(WARNING, "fail to get mem usage");
        return 1;
    }
    ok = GetGlobalIntrStat();
    if (!ok) {
        LOG(WARNING, "fail to get interupt usage");
        return 1;
    }
    ok = GetGlobalIOStat();
    if (!ok) {
        LOG(WARNING, "fail to get IO usage");
        return 1;    
    }
    ok = GetGlobalNetStat();
    if (!ok) {
        LOG(WARNING, "fail to get Net usage");
        return 1;
    }
    stat_->collect_times_++;
    if (stat_->collect_times_ < MIN_COLLECT_TIME) {
        LOG(WARNING, "collect times not reach %d", MIN_COLLECT_TIME);
        return 2;
    }

    bool ret = false;
    ret |= IsItemBusy(stat_->cpu_used_, FLAGS_max_cpu_usage, stat_->cpu_used_ex_,
                      FLAGS_max_ex_time, stat_->cpu_used_busy_, "cpu");
    ret |= IsItemBusy(stat_->mem_used_, FLAGS_max_mem_usage, stat_->mem_used_ex_,
                      FLAGS_max_ex_time, stat_->mem_used_busy_, "mem");
    ret |= IsItemBusy(stat_->disk_read_Bps_, FLAGS_max_disk_r_kbps, stat_->disk_read_Bps_ex_,
                      FLAGS_max_ex_time, stat_->disk_read_Bps_busy_, "disk read kBps");
    ret |= IsItemBusy(stat_->disk_write_Bps_, FLAGS_max_disk_w_kbps, stat_->disk_write_Bps_ex_,
                      FLAGS_max_ex_time, stat_->disk_write_Bps_busy_, "disk write kBps");
    ret |= IsItemBusy(stat_->disk_read_times_, FLAGS_max_disk_r_rate, stat_->disk_read_times_ex_,
                      FLAGS_max_ex_time, stat_->disk_read_times_busy_, "disk read rate");
    ret |= IsItemBusy(stat_->disk_write_times_, FLAGS_max_disk_w_rate, stat_->disk_write_times_ex_,
                      FLAGS_max_ex_time, stat_->disk_write_times_busy_, "disk write rate");
    ret |= IsItemBusy(stat_->disk_io_util_, FLAGS_max_disk_util, stat_->disk_io_util_ex_,
                      FLAGS_max_ex_time, stat_->disk_io_util_busy_, "disk io util");
    ret |= IsItemBusy(stat_->net_in_bps_, FLAGS_max_net_in_bps, stat_->net_in_bps_ex_,
                      FLAGS_max_ex_time, stat_->net_in_bps_busy_, "net in bps");
    ret |= IsItemBusy(stat_->net_out_bps_, FLAGS_max_net_out_bps, stat_->net_out_bps_ex_,
                      FLAGS_max_ex_time, stat_->net_out_bps_busy_, "net out bps");
    ret |= IsItemBusy(stat_->net_in_pps_, FLAGS_max_net_in_pps, stat_->net_in_pps_ex_,
                      FLAGS_max_ex_time, stat_->net_in_pps_busy_, "net in pps");
    ret |= IsItemBusy(stat_->net_out_pps_, FLAGS_max_net_out_pps, stat_->net_out_pps_ex_,
                      FLAGS_max_ex_time, stat_->net_out_pps_busy_, "net out pps");
    ret |= IsItemBusy(stat_->intr_rate_, FLAGS_max_intr_rate, stat_->intr_rate_ex_,
                      FLAGS_max_ex_time, stat_->intr_rate_busy_, "interupt rate");
    ret |= IsItemBusy(stat_->soft_intr_rate_, FLAGS_max_soft_intr_rate, stat_->soft_intr_rate_ex_,
                      FLAGS_max_ex_time, stat_->soft_intr_rate_busy_, "soft interupt rate");
    if (ret) {
        return 3;
    } else {
        return 0;
    }
}

bool GlobalResourceCollector::GetGlobalCpuStat() {
    ResourceStatistics statistics;
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
                           &(stat_->cur_stat_.cpu_user_time),
                           &(stat_->cur_stat_.cpu_nice_time),
                           &(stat_->cur_stat_.cpu_system_time),
                           &(stat_->cur_stat_.cpu_idle_time),
                           &(stat_->cur_stat_.cpu_iowait_time),
                           &(stat_->cur_stat_.cpu_irq_time),
                           &(stat_->cur_stat_.cpu_softirq_time),
                           &(stat_->cur_stat_.cpu_stealstolen),
                           &(stat_->cur_stat_.cpu_guest)); 

    free(line); 
    line = NULL;
    if (item_size != 10) {
        LOG(WARNING, "read from /proc/stat format err"); 
        return false;
    }
    long total_cpu_time_last = 
    stat_->last_stat_.cpu_user_time
    + stat_->last_stat_.cpu_nice_time
    + stat_->last_stat_.cpu_system_time
    + stat_->last_stat_.cpu_idle_time
    + stat_->last_stat_.cpu_iowait_time
    + stat_->last_stat_.cpu_irq_time
    + stat_->last_stat_.cpu_softirq_time
    + stat_->last_stat_.cpu_stealstolen
    + stat_->last_stat_.cpu_guest;
    long total_cpu_time_cur =
    stat_->cur_stat_.cpu_user_time
    + stat_->cur_stat_.cpu_nice_time
    + stat_->cur_stat_.cpu_system_time
    + stat_->cur_stat_.cpu_idle_time
    + stat_->cur_stat_.cpu_iowait_time
    + stat_->cur_stat_.cpu_irq_time
    + stat_->cur_stat_.cpu_softirq_time
    + stat_->cur_stat_.cpu_stealstolen
    + stat_->cur_stat_.cpu_guest;
    long total_cpu_time = total_cpu_time_cur - total_cpu_time_last;
    if (total_cpu_time < 0) {
        LOG(WARNING, "invalide total cpu time cur %ld last %ld", total_cpu_time_cur, total_cpu_time_last);
        return false;
    }     

    long total_used_time_last = 
    stat_->last_stat_.cpu_user_time 
    + stat_->last_stat_.cpu_system_time
    + stat_->last_stat_.cpu_nice_time
    + stat_->last_stat_.cpu_irq_time
    + stat_->last_stat_.cpu_softirq_time
    + stat_->last_stat_.cpu_stealstolen
    + stat_->last_stat_.cpu_guest;

    long total_used_time_cur =
    stat_->cur_stat_.cpu_user_time
    + stat_->cur_stat_.cpu_nice_time
    + stat_->cur_stat_.cpu_system_time
    + stat_->cur_stat_.cpu_irq_time
    + stat_->cur_stat_.cpu_softirq_time
    + stat_->cur_stat_.cpu_stealstolen
    + stat_->cur_stat_.cpu_guest;
    long total_cpu_used_time = total_used_time_cur - total_used_time_last;
    if (total_cpu_used_time < 0)  {
        LOG(WARNING, "invalude total cpu used time cur %ld last %ld", total_used_time_cur, total_used_time_last);
        return false;
    }
    double rs = total_cpu_used_time / static_cast<double>(total_cpu_time);
    stat_->cpu_used_ = rs;
    return true;
}

bool GlobalResourceCollector::GetGlobalMemStat(){
    FILE* fp = fopen("/proc/meminfo", "rb");
    if (fp == NULL) {
        return false;
    }
    std::string content;
    char buf[1024];
    int len = 0;
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        content.append(buf, len);
    }
    std::vector<std::string> lines;
    boost::split(lines, content, boost::is_any_of("\n"));
    int64_t total_mem = 0;
    int64_t free_mem = 0;
    int64_t buffer_mem = 0;
    int64_t cache_mem = 0;
    int64_t tmpfs_mem = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string line = lines[i];
        std::vector<std::string> parts;
        if (line.find("MemTotal:") == 0) {
            boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
            if (parts.size() < 2) {
                fclose(fp);
                return false;
            }
            total_mem = boost::lexical_cast<int64_t>(parts[parts.size() - 2]);
        }else if (line.find("MemFree:") == 0) {
            boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
            if (parts.size() < 2) {
                fclose(fp);
                return false;
            }
            free_mem = boost::lexical_cast<int64_t>(parts[parts.size() - 2]);
        }else if (line.find("Buffers:") == 0) {
            boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
            if (parts.size() < 2) {
                fclose(fp);
                return false;
            }
            buffer_mem = boost::lexical_cast<int64_t>(parts[parts.size() - 2]);
        }else if (line.find("Cached:") == 0) {
            boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
            if (parts.size() < 2) {
                fclose(fp);
                return false;
            }
            cache_mem = boost::lexical_cast<int64_t>(parts[parts.size() - 2]);
        }
    }
    fclose(fp);
    
    std::string path = "/etc/mtab";
    std::ifstream stat(path.c_str());
    if (!stat.is_open()) {
        LOG(WARNING, "open proc stat fail.");
        return false;
    }
    std::ostringstream tmp;
    tmp << stat.rdbuf();
    std::string mtab = tmp.str();
    stat.close();
    boost::split(lines, mtab, boost::is_any_of("\n"));
    for (size_t n = 0; n < lines.size(); n++) {
        std::string line = lines[n];
        if (line.find("tmpfs") == 0) {
            boost::trim(line);
            int tmpfs_size = 0;
            std::vector<std::string> parts;
            boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
            if (0 != sscanf(parts[3].c_str(), "rw,size=%dG", &tmpfs_size)) {
                tmpfs_mem += tmpfs_size * 1024 * 1024;
            } else if (0 != sscanf(parts[3].c_str(), "rw,size=%dM", &tmpfs_size)) {
                tmpfs_mem += tmpfs_size * 1024;
            }
        }
    }

    stat_->mem_used_ = (total_mem - free_mem - buffer_mem - cache_mem + tmpfs_mem) / boost::lexical_cast<double>(total_mem);
    return true;
}

bool GlobalResourceCollector::GetGlobalIntrStat() {
    uint64_t intr_cnt = 0;
    uint64_t softintr_cnt = 0;
    std::string path = "/proc/stat";
    std::ifstream stat(path.c_str());
    if (!stat.is_open()) {
        LOG(WARNING, "open proc stat fail.");
        return false;
    } 
    std::vector<std::string> lines;
    std::string content; 
    stat >> content;
    stat.close();
    boost::split(lines, content, boost::is_any_of("\n"));
    for (size_t n = 0; n < lines.size(); n++) {
        std::string line = lines[n];
        if (line.find("intr") != std::string::npos) {
            boost::trim(line);
            std::vector<std::string> parts;
            boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
            intr_cnt = boost::lexical_cast<int64_t>(parts[1]);
        } else if (line.find("softirq") != std::string::npos) {
            std::vector<std::string> parts;
            boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
            softintr_cnt = boost::lexical_cast<int64_t>(parts[1]);
        }
        continue;
    }
    stat_->cur_stat_.interupt_times = intr_cnt;
    stat_->cur_stat_.soft_interupt_times = softintr_cnt;
    stat_->intr_rate_ = (stat_->cur_stat_.interupt_times - stat_->last_stat_.interupt_times) / FLAGS_stat_check_period * 1000; 
    stat_->soft_intr_rate_ = (stat_->cur_stat_.soft_interupt_times - stat_->last_stat_.soft_interupt_times) / FLAGS_stat_check_period * 1000;
    return true;
}

bool GlobalResourceCollector::GetGlobalIOStat() {
    struct stat stat_buf;
    if (stat(FLAGS_loop_dev_file.c_str(), &stat_buf) != 0) {
        return false;
    }
    int dev_major = major(stat_buf.st_dev);
    int dev_minor = minor(stat_buf.st_dev);
    std::ifstream partition("/proc/partitions");
    if (!partition.is_open()) {
        LOG(WARNING, "open partitions file fail.");
        return false;
    }
    std::vector<std::string> lines;
    std::string dev_name;
    std::ostringstream tmp;
    tmp << partition.rdbuf();
    std::string content = tmp.str();
    partition.close();
    boost::split(lines, content, boost::is_any_of("\n"));
    for (size_t n = 0; n < lines.size(); n++) {
        if (n < 2) {
            continue;
        }
        std::string line = lines[n];
        boost::trim(line);
        std::vector<std::string> parts;
        boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
        if (boost::lexical_cast<int64_t>(parts[0]) == dev_major
                && boost::lexical_cast<int64_t>(parts[1]) == dev_minor) {
            if (parts[3].find("sda") != std::string::npos) {
                dev_name = "sda";
                break;
            } else if (parts[3].find("cciss") != std::string::npos) {
                dev_name = "cciss!c0d0";
                break;
            } else {
                LOG(WARNING, "can not find dev name");
                return false;
            }
        } else {
            continue;
        }
    }
    std::string path = "/sys/block/" + dev_name + "/stat";
    std::ifstream stat(path.c_str());
    if (!stat.is_open()) {
        LOG(WARNING, "open proc stat fail.");
        return false;
    }
    tmp.str("");
    tmp << stat.rdbuf(); 
    content = tmp.str();
    stat.close();
    boost::trim(content);
    std::vector<std::string> parts;
    boost::split(parts, content, boost::is_any_of(" "), boost::token_compress_on);
    stat_->cur_stat_.rd_ios = boost::lexical_cast<int64_t>(parts[0]);
    stat_->cur_stat_.rd_sectors = boost::lexical_cast<int64_t>(parts[2]);
    stat_->cur_stat_.wr_ios = boost::lexical_cast<int64_t>(parts[4]);
    stat_->cur_stat_.wr_sectors = boost::lexical_cast<int64_t>(parts[6]);
    stat_->disk_read_times_ = (stat_->cur_stat_.rd_ios - stat_->last_stat_.rd_ios) / FLAGS_stat_check_period * 1000;
    stat_->disk_write_times_ = (stat_->cur_stat_.wr_ios - stat_->last_stat_.wr_ios) / FLAGS_stat_check_period * 1000;
    stat_->disk_read_Bps_ = (stat_->cur_stat_.rd_sectors - stat_->last_stat_.rd_sectors) / FLAGS_stat_check_period * 1000;
    stat_->disk_write_Bps_ = (stat_->cur_stat_.wr_sectors - stat_->last_stat_.wr_sectors) / FLAGS_stat_check_period * 1000;
    return true;
}

bool GlobalResourceCollector::GetGlobalNetStat() {
    std::string path = "/proc/net/dev";
    std::ifstream stat(path.c_str());
    if (!stat.is_open()) {
        LOG(WARNING, "open dev stat fail.");
        return false;
    }
    std::ostringstream tmp;
    tmp << stat.rdbuf();
    std::string content = tmp.str();
    stat.close();
    std::vector<std::string> lines;
    boost::split(lines, content, boost::is_any_of("\n"));
    for (size_t n = 0; n < lines.size(); n++) {
        std::string line = lines[n];
        if (line.find("eth") != std::string::npos || 
                line.find("xgbe") != std::string::npos) {
            boost::trim(line);
            std::vector<std::string> parts;
            boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
            if (boost::lexical_cast<int64_t>(parts[1]) == 0) {
                continue;
            }
            std::vector<std::string> tokens;
            boost::split(tokens, parts[0], boost::is_any_of(":"));
            stat_->cur_stat_.net_in_bits = boost::lexical_cast<int64_t>(tokens[1]);
            stat_->cur_stat_.net_in_packets = boost::lexical_cast<int64_t>(parts[1]);
            stat_->cur_stat_.net_out_bits = boost::lexical_cast<int64_t>(parts[8]);
            stat_->cur_stat_.net_out_packets = boost::lexical_cast<int64_t>(parts[9]);
        }
        continue;
    }
    stat_->net_in_bps_ = (stat_->cur_stat_.net_in_bits - stat_->last_stat_.net_in_bits) / FLAGS_stat_check_period * 1000;
    stat_->net_out_bps_ = (stat_->cur_stat_.net_out_bits - stat_->last_stat_.net_out_bits) / FLAGS_stat_check_period * 1000;
    stat_->net_in_pps_ = (stat_->cur_stat_.net_in_packets - stat_->last_stat_.net_in_packets) / FLAGS_stat_check_period * 1000;
    stat_->net_out_pps_ = (stat_->cur_stat_.net_out_packets - stat_->last_stat_.net_out_packets) / FLAGS_stat_check_period * 1000;
    return true;
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
            //statistics->memory_rss_in_bytes += val; 
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

CGroupIOCollector::CGroupIOCollector(){}

CGroupIOCollector::~CGroupIOCollector(){}

bool CGroupIOCollector::Collect(const std::string& freezer_path,
                                const std::string& blkio_path,
                                CGroupIOStatistics* stat) {
    // get ProcIOStat
    std::vector<int> pids;
    bool ok = ::baidu::galaxy::cgroups::GetPidsFromCgroup(freezer_path, &pids);
    if (!ok) {
        LOG(WARNING, "fail to get pid from cgroup %s", freezer_path.c_str());
        return false;
    }

    std::vector<int>::iterator pid_it = pids.begin();
    for (; pid_it != pids.end(); ++pid_it) {
        ProcIOStatistics proc_stat;
        ok = GetProcIOStat(*pid_it, &proc_stat);
        if (!ok) {
            LOG(WARNING, "fail to get pid %d io stat", *pid_it);
            continue;
        }
        stat->processes.insert(std::make_pair(*pid_it, proc_stat));
    }

    // get BlkioStat
    ok = GetBlkioStat(blkio_path, &(stat->blkio));
    if (!ok) {
       LOG(WARNING, "fail to get blkio statistics from cgroup %s", blkio_path.c_str());
        return false;
    }

    return true;
}

bool CGroupIOCollector::GetProcIOStat(int pid, ProcIOStatistics* stat) {
    std::string io_path = "/proc/" + boost::lexical_cast<std::string>(pid) + "/io";
    bool ok = ::baidu::galaxy::file::IsExists(io_path);
    if (!ok) {
        LOG(WARNING, "io %s does not exist", io_path.c_str());
        return false;
    }
    std::ifstream ifs;
    ifs.open(io_path.c_str(), std::ifstream::binary);
    while (ifs.good()) {
        if (ifs.eof()) {
            break;
        }
        std::string line;
        std::getline(ifs, line);
        std::vector<std::string> parts;
        boost::split(parts, line, boost::is_any_of(": "));
        if (parts.size() != 3) {
            continue;
        }
        if (parts[0] == "rchar") {
            stat->rchar = boost::lexical_cast<int64_t>(parts[2]);
        } else if (parts[0] == "wchar") { 
            stat->wchar = boost::lexical_cast<int64_t>(parts[2]);
        } else if (parts[0] == "syscr") {
            stat->syscr = boost::lexical_cast<int64_t>(parts[2]);
        } else if (parts[0] == "syscw") {
            stat->syscw = boost::lexical_cast<int64_t>(parts[2]);
        } else if (parts[0] == "read_bytes") {
            stat->read_bytes = boost::lexical_cast<int64_t>(parts[2]);
        } else if (parts[0] == "write_bytes") {
            stat->write_bytes = boost::lexical_cast<int64_t>(parts[2]);
        } else if (parts[0] == "cancelled_write_bytes") {
            stat->cancelled_write_bytes = boost::lexical_cast<int64_t>(parts[2]);
        }
    }
    ifs.close();
    return true;
}

bool CGroupIOCollector::GetBlkioStat(const std::string& group_path,
                                     BlkioStatistics* blkio) {
    if (blkio == NULL) {
        return false;
    }

    int32_t major_number;
    bool ok = file::GetDeviceMajorNumberByPath(FLAGS_agent_work_dir, major_number);
    if (!ok) {
        LOG(WARNING, "get device major fail");
        return false;
    }
    std::string device_name = boost::lexical_cast<std::string>(major_number) + ":0";

    std::string value;
    if (0 != cgroups::Read(group_path,
                           "blkio.throttle.io_serviced",
                           &value)) {
        LOG(WARNING, "get blkio.throttle.io_serviced falied %s",
                     group_path.c_str());
        return false;
    }

    std::vector<std::string> lines;
    boost::split(lines, value, boost::is_any_of("\n"));
    if (lines.size() <= 1) {
        LOG(WARNING, "read contents format err %s %s",
                      group_path.c_str(), value.c_str());
        return false;
    }

    for (size_t i = 0; i < lines.size(); i++) {
        std::string& line = lines[i];
        if (line.empty()) {
            continue;
        }
        std::istringstream ss(line);
        std::string name;
        std::string type;
        uint64_t val;
        ss >> name >> type >> val;
        if (ss.fail()) {
            LOG(WARNING, "line format err %s", line.c_str());
            return false;
        }

        if (name == device_name) {
            if ("Read" == type) {
                blkio->read = val;
            } else if ("Write" == type) {
                blkio->write = val;
            } else if ("Sync" == type) {
                blkio->sync = val;
            } else if ("Async" == type) {
                blkio->async = val;
            } else if ("Total" == type) {
                blkio->total = val;
                break;
            }
        }
    }

    return true;
}

}   // ending namespace galaxy
}   // ending namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
