// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#include "agent/resource_collector.h"

#include <errno.h>
#include <string.h>
#include <time.h>

#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "common/logging.h"
#include "common/mutex.h"
#include <gflags/gflags.h>

DECLARE_string(cgroup_root);

namespace galaxy {

#define SAFE_FREE(x) \
    do { \
        if (x != NULL) {\
            free(x); \
            x = NULL; \
        } \
    } while(0)


const std::string CPUACT_SUBSYSTEM_PATH = "cpuacct";
const std::string PROC_MOUNT_PATH = "/proc/";

static long SYS_TIMER_HZ = sysconf(_SC_CLK_TCK);
static long CPU_CORES = sysconf(_SC_NPROCESSORS_CONF);
static long MEMORY_PAGE_SIZE = sysconf(_SC_PAGESIZE);

static size_t PROC_STAT_FILE_SPLIT_SIZE = 44;
static const size_t PROC_STATUS_FILE_SPLIT_SIZE = 3;

// unit jiffies
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
};

// unit jiffies
struct CgroupResourceStatistics {
    long cpu_user_time;
    long cpu_system_time;

    // cpu_cores from cfs_quota_usota_us
    double cpu_cores_limit;

    long memory_rss_in_bytes;
};

// get cpu usage from /cgroups/cpuacct/xxxxx/cpuacct.stat
static bool GetCgroupCpuUsage(
                const std::string& group_path, 
                CgroupResourceStatistics* statistics);

// get memory usage from /cgroups/memory/xxxxx/memory.stat
static bool GetCgroupMemoryUsage(
        const std::string& group_path,
        CgroupResourceStatistics* statistics);

// get total cpu usage from /proc/stat
static bool GetGlobalCpuUsage(ResourceStatistics* statistics);


// get cpu limit cores from /cgroups/cpu/xxxxx/cpu.cfs_quota_usota_us and cpu.cfs_
static bool GetCgroupCpuCoresLimit(
        const std::string& group_path, 
        CgroupResourceStatistics* statistics);

// get cpu usage from /proc/pid/stat
static bool GetProcCpuUsage(
        int pid,
        ResourceStatistics* statistics);

// ------------ proc collector impl ----------------

class ProcResourceCollectorImpl {
public:
    explicit ProcResourceCollectorImpl(int pid) 
        : pid_(pid),
          global_statistics_prev_(),
          global_statistics_cur_(),
          process_statistics_prev_(),
          process_statistics_cur_(),
          timestamp_prev_(0.0),
          timestamp_cur_(0.0),
          collector_times_(0),
          mutex_(),
          is_cleared_(true) {
    }
    virtual ~ProcResourceCollectorImpl() {}
    virtual bool CollectStatistics();
    double GetCpuUsage();

    void ResetPid(int pid) {
        pid_ = pid; 
    }

    void Clear() {
        collector_times_ = 0;
        is_cleared_ = true; 
    }

    long GetMemoryUsage() {
        return process_statistics_cur_.memory_rss_in_bytes;
    }

private:
    int pid_;
    ResourceStatistics global_statistics_prev_;
    ResourceStatistics global_statistics_cur_;

    ResourceStatistics process_statistics_prev_;    
    ResourceStatistics process_statistics_cur_;

    double timestamp_prev_;
    double timestamp_cur_;
    long collector_times_;
    common::Mutex mutex_;
    bool is_cleared_;
};

bool ProcResourceCollectorImpl::CollectStatistics() {
    common::MutexLock lock(&mutex_);
    global_statistics_prev_ = global_statistics_cur_;
    process_statistics_prev_ = process_statistics_cur_;

    timestamp_prev_ = timestamp_cur_;
    if (!GetGlobalCpuUsage(&global_statistics_cur_)) {
        return false; 
    }

    if (!GetProcCpuUsage(pid_, &process_statistics_cur_)) {
        return false; 
    }

    collector_times_ ++;
    const int MIN_COLLOECT_TIMES_NEED = 2;        
    if (collector_times_ >= MIN_COLLOECT_TIMES_NEED) {
        is_cleared_ = false; 
    }
    return true;
}

double ProcResourceCollectorImpl::GetCpuUsage() {
    common::MutexLock lock(&mutex_);
    if (is_cleared_) {
        LOG(WARNING, "need try again"); 
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

    if (global_cpu_after - global_cpu_before == 0) {
        return 0.0; 
    }
    double rs = (proc_cpu_after - proc_cpu_before) / (double)(global_cpu_after - global_cpu_before);
    LOG(DEBUG, "p prev :%ld p post:%ld g pre:%ld g post:%ld rs:%f",
            proc_cpu_before,
            proc_cpu_after,
            global_cpu_before,
            global_cpu_after,
            rs);
    return rs;
}

// ------------- ProcResourceCollector Implement ------------

ProcResourceCollector::ProcResourceCollector(int pid) 
    : impl_(NULL) {
    impl_ = new ProcResourceCollectorImpl(pid);   
}

ProcResourceCollector::~ProcResourceCollector() {
    if (impl_ != NULL) {
        delete impl_; 
        impl_ = NULL;
    }
}

double ProcResourceCollector::GetCpuUsage() {
    return impl_->GetCpuUsage();
}

long ProcResourceCollector::GetMemoryUsage() {
    return impl_->GetMemoryUsage();
}

bool ProcResourceCollector::CollectStatistics() {
    return impl_->CollectStatistics();
}

void ProcResourceCollector::ResetPid(int pid) {
    impl_->ResetPid(pid);
}

void ProcResourceCollector::Clear() {
    impl_->Clear();
}

// ------------- cgroup collector impl --------------

class CGroupResourceCollectorImpl {
public:
    explicit CGroupResourceCollectorImpl(const std::string& cgroup_name) 
        : cgroup_name_(cgroup_name),
          global_statistics_prev_(), 
          global_statistics_cur_(),
          cgroup_statistics_prev_(),
          cgroup_statistics_cur_(),
          timestamp_prev_(0.0),
          timestamp_cur_(0.0),
          collector_times_(0),
          mutex_(),
          is_cleared_(true) {
    }

    virtual ~CGroupResourceCollectorImpl() {}

    virtual bool CollectStatistics();

    void ResetCgroupName(const std::string& cgroup_name) {
        cgroup_name_ = cgroup_name; 
    }

    void Clear() {
        collector_times_ = 0;
        is_cleared_ = true; 
    }
    double GetCpuUsage();

    double GetCpuCoresUsage();

    long GetMemoryUsage() {
        return cgroup_statistics_cur_.memory_rss_in_bytes;
    }
private:
    void CalcCpuUsage();
    std::string cgroup_name_;
    // global resource statistics
    ResourceStatistics global_statistics_prev_;
    ResourceStatistics global_statistics_cur_;
    // cgroup resource statistics
    CgroupResourceStatistics cgroup_statistics_prev_;
    CgroupResourceStatistics cgroup_statistics_cur_;

    double timestamp_prev_;
    double timestamp_cur_;
    long collector_times_;
    common::Mutex mutex_;
    bool is_cleared_;
    double cpu_cur_usage_;
    double cpu_cores_cur_usage_;
};

double CGroupResourceCollectorImpl::GetCpuCoresUsage() {
    common::MutexLock lock(&mutex_);
    CalcCpuUsage();
    return cpu_cores_cur_usage_;
}

double CGroupResourceCollectorImpl::GetCpuUsage() {
    common::MutexLock lock(&mutex_);
    CalcCpuUsage();
    return cpu_cur_usage_;
}

void CGroupResourceCollectorImpl::CalcCpuUsage() {
    mutex_.AssertHeld();
    if (is_cleared_) {
        LOG(WARNING, "need try again"); 
        cpu_cur_usage_ = 0.0;
        cpu_cores_cur_usage_ = 0.0;
        return;
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

    double radio = cgroup_statistics_cur_.cpu_cores_limit / global_statistics_cur_.cpu_cores;
    LOG(DEBUG, "radio = limit(%f) / cores(%ld)",
            cgroup_statistics_cur_.cpu_cores_limit,
            global_statistics_cur_.cpu_cores);

    if (global_cpu_after - global_cpu_before == 0) {
        cpu_cur_usage_ = 0.0;
        cpu_cores_cur_usage_ = 0.0;
        return; 
    }

    double rs = (cgroup_cpu_after - cgroup_cpu_before) / (double)(global_cpu_after - global_cpu_before) ; 
    rs /= radio;
    cpu_cur_usage_ = rs;
    cpu_cores_cur_usage_ = 
        rs * cgroup_statistics_cur_.cpu_cores_limit;
    LOG(DEBUG, "p prev :%ld p post:%ld g pre:%ld g post:%ld radir:%f cpu_cur_usage:%lf cpu_cores_cur_usage:%lf", 
            cgroup_cpu_before, 
            cgroup_cpu_after,
            global_cpu_before,
            global_cpu_after,
            radio,
            cpu_cur_usage_,
            cpu_cores_cur_usage_);
    return;
}

bool CGroupResourceCollectorImpl::CollectStatistics() {
    common::MutexLock lock(&mutex_);
    global_statistics_prev_ = global_statistics_cur_;
    cgroup_statistics_prev_ = cgroup_statistics_cur_;
    timestamp_prev_ = timestamp_cur_;

    timestamp_cur_ = time(NULL);

    if (!GetCgroupCpuUsage(cgroup_name_, 
                &cgroup_statistics_cur_)) {
        LOG(WARNING, "cgroup collector collect cpu usage failed");
        return false; 
    }

    if (!GetGlobalCpuUsage(&global_statistics_cur_)) {
        LOG(WARNING, "cgroup collector collect global cpu usage failed");
        return false; 
    }

    if (!GetCgroupCpuCoresLimit(cgroup_name_, 
                &cgroup_statistics_cur_)) {
        LOG(WARNING, "cgroup collector collect cpu limit failed");
        return false; 
    }

    if (!GetCgroupMemoryUsage(cgroup_name_,
                &cgroup_statistics_cur_)) {
        LOG(WARNING, "cgroup collector collect memory failed");
        return false; 
    }

    const int MIN_COLLOECT_TIMES_NEED = 2;        
    collector_times_ ++;
    if (collector_times_ >= MIN_COLLOECT_TIMES_NEED) {
        is_cleared_ = false; 
    }
    return true;
}

// ----------- CGroupResourceCollector implement -----

CGroupResourceCollector::CGroupResourceCollector(
        const std::string& cgroup_name) : impl_(NULL) {
    impl_ = new CGroupResourceCollectorImpl(cgroup_name);
}

bool CGroupResourceCollector::CollectStatistics() {
    return impl_->CollectStatistics();
}

double CGroupResourceCollector::GetCpuUsage() {
    return impl_->GetCpuUsage();
}

long CGroupResourceCollector::GetMemoryUsage() {
    return impl_->GetMemoryUsage();
}

double CGroupResourceCollector::GetCpuCoresUsage() {
    return impl_->GetCpuCoresUsage();
}

CGroupResourceCollector::~CGroupResourceCollector() {
    delete impl_;
}

void CGroupResourceCollector::ResetCgroupName(
        const std::string& cgroup_name) {
    impl_->ResetCgroupName(cgroup_name);
}

void CGroupResourceCollector::Clear() {
    impl_->Clear();
}

// -----------  resource collector utils -------------

bool GetCgroupCpuUsage(
        const std::string& group_path, 
        CgroupResourceStatistics* statistics) {
    if (statistics == NULL) {
        return false; 
    }
    std::string path = FLAGS_cgroup_root + "/" 
        + CPUACT_SUBSYSTEM_PATH + "/" + group_path + "/cpuacct.stat";
    FILE* fin = fopen(path.c_str(), "r");
    if (fin == NULL) {
        LOG(WARNING, "open %s failed", path.c_str());
        return false; 
    }

    ssize_t read;
    size_t len = 0;
    char* line = NULL;

    // read user time
    if ((read = getline(&line, &len, fin)) == -1) {
        LOG(WARNING, "read line failed err[%d: %s]", 
                errno, strerror(errno));
        fclose(fin);
        return false;
    }

    char cpu_tmp_char[20];
    int item_size = sscanf(line, "%s %ld", 
            cpu_tmp_char, &(statistics->cpu_user_time));
    SAFE_FREE(line);

    if (item_size != 2) {
        LOG(WARNING, "read from %s format err", path.c_str()); 
        fclose(fin);
        return false;
    }

    // read sys time
    if ((read = getline(&line, &len, fin)) == -1) {
        LOG(WARNING, "read line failed err[%d: %s]", 
                errno, strerror(errno));  
        fclose(fin);
        return false;
    }

    item_size = sscanf(line, "%s %ld",
            cpu_tmp_char, &(statistics->cpu_system_time));
    SAFE_FREE(line);

    if (item_size != 2) {
        LOG(WARNING, "read from %s format err", path.c_str()); 
        fclose(fin);
        return false;
    }

    fclose(fin);
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

    SAFE_FREE(line);
    if (item_size != 10) {
        LOG(WARNING, "read from /proc/stat format err"); 
        return false;
    }
    return true;
}

bool GetCgroupCpuCoresLimit(
        const std::string& group_path,
        CgroupResourceStatistics* statistics) {
    std::string period_path = FLAGS_cgroup_root 
        + "/cpu/" + group_path + "/cpu.cfs_period_us";
    std::string quota_path = FLAGS_cgroup_root
        + "/cpu/" + group_path + "/cpu.cfs_quota_us";

    FILE* fin = fopen(period_path.c_str(), "r");
    if (fin == NULL) {
        LOG(WARNING, "open %s failed", period_path.c_str());
        return false;
    }

    ssize_t read;
    size_t len = 0;
    char* line = NULL;

    if ((read = getline(&line, &len, fin)) == -1) {
        LOG(WARNING, "read line failed err[%d: %s]", errno, strerror(errno));
        fclose(fin);
        return false;
    }
    fclose(fin);

    long period_time = 0;
    int item_size = sscanf(line, "%ld", &period_time);
    SAFE_FREE(line);
    if (item_size != 1) {
        LOG(WARNING, "read from %s format err", period_path.c_str());
        return false;
    }

    fin = fopen(quota_path.c_str(), "r");
    if (fin == NULL) {
        LOG(WARNING, "open %s failed", quota_path.c_str()); 
        return false;
    }

    line = NULL;
    len = 0;
    if ((read = getline(&line, &len, fin)) == -1) {
        LOG(WARNING, "read line failed err[%d: %s]", errno, strerror(errno)); 
        fclose(fin);
        return false;
    }
    fclose(fin);

    long quota_value = 0;
    item_size = sscanf(line, "%ld", &quota_value);
    SAFE_FREE(line);
    if (item_size != 1) {
        LOG(WARNING, "read from %s format err", quota_path.c_str());
        return false;
    }
    LOG(DEBUG, "cpu cores limit : quota(%ld), period(%ld)",
            quota_value, 
            period_time);
    statistics->cpu_cores_limit = quota_value * 1.0 / period_time;
    return true;
}

bool GetCgroupMemoryUsage(const std::string& group_path,
        CgroupResourceStatistics* statistics) {
    std::string memory_path = FLAGS_cgroup_root 
        + "/memory/" + group_path + "/memory.stat";
    FILE* fin = fopen(memory_path.c_str(), "r");
    if (fin == NULL) {
        LOG(WARNING, "open %s failed", memory_path.c_str());
        return false;
    } 

    ssize_t read;
    char* line = NULL;
    size_t len = 0;

    // escape first line for cache usage
    if ((read = getline(&line, &len, fin)) == -1) {
        LOG(WARNING, "read line failed err[%d: %s]", errno, strerror(errno));
        return false;
    }

    SAFE_FREE(line);
    if ((read = getline(&line, &len, fin)) == -1) {
        LOG(WARNING, "read line failed err[%d: %s]", errno, strerror(errno));
        return false;
    }

    fclose(fin);
    char tmp[10];
    int item_size = sscanf(line, "%s %ld", tmp, &(statistics->memory_rss_in_bytes));
    SAFE_FREE(line);
    if (item_size != 2) {
        LOG(WARNING, "read from %s format err", memory_path.c_str());  
        return false;
    }
    return true;
}

bool GetProcCpuUsage(int pid, ResourceStatistics* statistics) {
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
    SAFE_FREE(line);
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

}   // ending namespace galaxy

/* vim: set ts=4 sw=4 sts=4 tw=100 */
