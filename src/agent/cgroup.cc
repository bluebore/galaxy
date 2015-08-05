// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#include "agent/cgroup.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <pwd.h>
#include "common/logging.h"
#include "common/util.h"
#include "common/this_thread.h"
#include "agent/downloader_manager.h"
#include "agent/resource_collector.h"
#include "agent/resource_collector_engine.h"
#include "agent/utils.h"
#include <gflags/gflags.h>
#include "agent/dynamic_resource_scheduler.h"

DECLARE_string(task_acct);
DECLARE_string(cgroup_root); 
DECLARE_int32(agent_cgroup_clear_retry_times);
DECLARE_bool(agent_dynamic_scheduler_switch);
DECLARE_string(agent_guarder_addr);

namespace galaxy {

static const std::string RUNNER_META_PREFIX = "task_runner_";
static const std::string MONITOR_META_PREFIX = "task_monitor_";
static int CPU_CFS_PERIOD = 100000;
static int MIN_CPU_CFS_QUOTA = 1000;
static int CPU_SHARE_PER_CPU = 1024;
static int MIN_CPU_SHARE = 10;

int CGroupCtrl::Create(int64_t task_id, std::map<std::string, std::string>& sub_sys_map) {
    if (_support_cg.size() <= 0) {
        LOG(WARNING, "no subsystem is support");
        return -1;
    }

    std::vector<std::string>::iterator it = _support_cg.begin();

    for (; it != _support_cg.end(); ++it) {
        std::stringstream ss ;
        ss << _cg_root << "/" << *it << "/" << task_id;
        int status = mkdir(ss.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        if (status != 0) {
            if (errno == EEXIST) {
                // TODO
                LOG(WARNING, "cgroup already there");
            } else {
                LOG(FATAL, "fail to create subsystem %s ,status is %d", ss.str().c_str(), status);
                return status;
            }
        }

        sub_sys_map[*it] = ss.str();
        LOG(INFO, "create subsystem %s successfully", ss.str().c_str());
    }

    return 0;
}


static int GetCgroupTasks(const std::string& task_path, std::vector<int>* pids) {
    if (pids == NULL) {
        return -1; 
    }
    FILE* fin = fopen(task_path.c_str(), "r");
    if (fin == NULL) {
        LOG(WARNING, "open %s failed err[%d: %s]", 
                task_path.c_str(), errno, strerror(errno)); 
        return -1;
    }
    ssize_t read;
    char* line = NULL;
    size_t len = 0;
    while ((read = getline(&line, &len, fin)) != -1) {
        int pid = atoi(line);
        if (pid <= 0) {
            continue; 
        }
        pids->push_back(pid);
    }
    if (line != NULL) {
        free(line);
    }
    fclose(fin);
    return 0;
}

//目前不支持递归删除
//删除前应该清空tasks文件
int CGroupCtrl::Destroy(int64_t task_id) {
    if (_support_cg.size() <= 0) {
        LOG(WARNING, "no subsystem is support");
        return -1;
    }

    std::vector<std::string>::iterator it = _support_cg.begin();
    int ret = 0;
    for (; it != _support_cg.end(); ++it) {
        std::stringstream ss ;
        ss << _cg_root << "/" << *it << "/" << task_id;
        std::string sub_cgroup_path = ss.str().c_str();
        // TODO maybe cgroup.proc ?
        std::string task_path = sub_cgroup_path + "/tasks";
        int clear_retry_times = 0;
        for (; clear_retry_times < FLAGS_agent_cgroup_clear_retry_times; 
                ++clear_retry_times) {
            int status = rmdir(sub_cgroup_path.c_str());
            if (status == 0 || errno == ENOENT) {
                break; 
            }
            LOG(FATAL,"fail to delete subsystem %s status %d err[%d: %s]",
                    sub_cgroup_path.c_str(),
                    status, 
                    errno,
                    strerror(errno));

            // clear task in cgroup
            std::vector<int> pids;
            if (GetCgroupTasks(task_path, &pids) != 0) {
                LOG(WARNING, "fail to clear task file"); 
                return -1;
            }
            LOG(WARNING, "get pids %ld from subsystem %s", 
                    pids.size(), sub_cgroup_path.c_str());
            if (pids.size() != 0) {
                std::vector<int>::iterator it = pids.begin();
                for (;it != pids.end(); ++it) {
                    if (::kill(*it, SIGKILL) == -1)  {
                        if (errno != ESRCH) {
                            LOG(WARNING, "kill process %d failed", *it);     
                        }     
                    }
                }
                common::ThisThread::Sleep(50);
            }
        }
        if (clear_retry_times 
                >= FLAGS_agent_cgroup_clear_retry_times) {
            ret = -1;
        }
    }
    return ret;
}

int AbstractCtrl::AttachTask(pid_t pid) {
    std::string task_file = _my_cg_root + "/" + "tasks";
    int ret = common::util::WriteIntToFile(task_file, pid);

    if (ret < 0) {
        //LOG(FATAL, "fail to attach pid  %d for %s", pid, _my_cg_root.c_str());
        return -1;
    }else{
        //LOG(INFO,"attach pid %d for %s successfully",pid, _my_cg_root.c_str());
    }

    return 0;
}

int MemoryCtrl::SetKillMode(int mode) {
#ifndef _BD_KERNEL_
    // memory.kill_mode is only support in internal kernel
    LOG(WARNING, "kill mode set only support in BD internal kernel");
    return 0;
#endif
    std::string kill_mode = _my_cg_root + "/" + "memory.kill_mode";
    int ret = common::util::WriteIntToFile(kill_mode, mode);
    if (ret < 0) {
        LOG(FATAL, "fail to set kill_mode %d for %s",
                mode, _my_cg_root.c_str()); 
        return -1;
    }
    return 0;
}

int MemoryCtrl::SetLimit(int64_t limit) {
    std::string limit_file = _my_cg_root + "/" + "memory.limit_in_bytes";
    int ret = common::util::WriteIntToFile(limit_file, limit);

    if (ret < 0) {
        LOG(FATAL, "fail to set limt %ld for %s", 
                limit, 
                _my_cg_root.c_str());
        return -1;
    }

    return 0;
}

int MemoryCtrl::SetSoftLimit(int64_t soft_limit) {
    std::string soft_limit_file = _my_cg_root + "/" + "memory.soft_limit_in_bytes";
    int ret = common::util::WriteIntToFile(soft_limit_file, soft_limit);

    if (ret < 0) {
        LOG(FATAL, "fail to set soft limt %lld for %s", soft_limit, _my_cg_root.c_str());
        return -1;
    }

    return 0;
}


int CpuCtrl::SetCpuShare(int64_t cpu_share) {
    std::string cpu_share_file = _my_cg_root + "/" + "cpu.shares";
    int ret = common::util::WriteIntToFile(cpu_share_file, cpu_share);
    if (ret < 0) {
        LOG(FATAL, "fail to set cpu share %ld for %s", 
                cpu_share, 
                _my_cg_root.c_str());
        return -1;
    }

    return 0;

}

int CpuCtrl::SetCpuPeriod(int64_t cpu_period) {
    std::string cpu_period_file = _my_cg_root + "/" + "cpu.cfs_period_us";
    int ret = common::util::WriteIntToFile(cpu_period_file, cpu_period);
    if (ret < 0) {
        LOG(FATAL, "fail to set cpu period %lld for %s", cpu_period, _my_cg_root.c_str());
        return -1;
    }

    return 0;
}

int CpuCtrl::SetCpuQuota(int64_t cpu_quota) {
    std::string cpu_quota_file = _my_cg_root + "/" + "cpu.cfs_quota_us";
    int ret = common::util::WriteIntToFile(cpu_quota_file, cpu_quota);
    if (ret < 0) {
        LOG(FATAL, "fail to set cpu quota  %ld for %s", 
                cpu_quota, 
                _my_cg_root.c_str());
        return -1;
    }

    return 0;
}

ContainerTaskRunner::ContainerTaskRunner(TaskInfo task_info,
        std::string cg_root,
        DefaultWorkspace* workspace,
        RpcClient* rpc_client) : AbstractTaskRunner(task_info, workspace),
                                 _cg_root(cg_root),
                                 _cg_ctrl(NULL),
                                 _mem_ctrl(NULL),
                                 _cpu_ctrl(NULL),
                                 _cpu_acct_ctrl(NULL),
                                 collector_(NULL),
                                 collector_id_(-1),
                                 persistence_path_dir_(),
                                 sequence_id_(0),
                                 rpc_client_(rpc_client) {
    SetStatus(ERROR);
    support_cg_.push_back("memory");
    support_cg_.push_back("cpu");
    support_cg_.push_back("cpuacct");
}

int CpuCtrl::GetCpuQuota(int64_t * cpu_quota) {
    if (NULL == cpu_quota) {
        return -2;
    }
    std::string cpu_quota_file = _my_cg_root + "/" + "cpu.cfs_quota_us";
    std::ifstream quota(cpu_quota_file.c_str());
    if (quota.is_open()) {
        std::string line;
        if (getline(quota, line)) {
            int64_t icpu_quota = boost::lexical_cast<int64_t>(line);
            *cpu_quota = icpu_quota;
            return 0;
        }
    }
    return -1;
}

int CpuCtrl::AdjustCpuQuota() {
    int64_t old_cpu_quota = 0;
    int ret = GetCpuQuota(&old_cpu_quota);
    if (ret != 0) {
        LOG(FATAL,"fail to get %s quota", _my_cg_root.c_str());
        return ret;
    }
    // add 0.5 cpu core
    int64_t new_cpu_quota = boost::lexical_cast<int64_t>(old_cpu_quota +  CPU_CFS_PERIOD * 0.5);
    ret = SetCpuQuota(new_cpu_quota);
    if (ret != 0) {
        LOG(FATAL, "fail to set %s quota %ld", _my_cg_root.c_str(), new_cpu_quota);
        return ret;
    }
    return 0;
}

ContainerTaskRunner::~ContainerTaskRunner() {
    if (collector_ != NULL) {
        ResourceCollectorEngine* engine
            = GetResourceCollectorEngine();
        engine->DelCollector(collector_id_);
        delete collector_;
        collector_ = NULL;
    }
    delete _cg_ctrl;
    delete _mem_ctrl;
    delete _cpu_ctrl;
    delete _cpu_acct_ctrl;
}

int ContainerTaskRunner::Init() {
    _cg_ctrl = new CGroupCtrl(_cg_root, support_cg_);

    std::map<std::string, std::string> sub_sys_map;
    if (_cg_ctrl->Create(m_task_info.task_id(), sub_sys_map) != 0) {
        LOG(WARNING, "init cgroup for task %ld job %ld failed",
                m_task_info.task_id(), m_task_info.job_id()); 
        return -1;
    }
    _mem_ctrl = new MemoryCtrl(sub_sys_map["memory"]);
    _cpu_ctrl = new CpuCtrl(sub_sys_map["cpu"]);
    _cpu_acct_ctrl = new CpuAcctCtrl(sub_sys_map["cpuacct"]);
    if (collector_ == NULL) {
        std::string group_path = boost::lexical_cast<std::string>(m_task_info.task_id());
        collector_ = new CGroupResourceCollector(group_path);
        ResourceCollectorEngine* engine
                = GetResourceCollectorEngine();
        collector_id_ = engine->AddCollector(collector_);
    }
    else {
        collector_->ResetCgroupName(
                boost::lexical_cast<std::string>(m_task_info.task_id()));
    }

    return 0;
}

int ContainerTaskRunner::Prepare() {
    LOG(INFO, "prepare container for task %ld", m_task_info.task_id());
    // NOTE multi thread safe
    
    // setup cgroup 
    int64_t mem_size = m_task_info.required_mem(); 
    double cpu_core = m_task_info.required_cpu();
    double cpu_limit = cpu_core;

    if (m_task_info.has_limited_cpu()) {
        cpu_limit = m_task_info.limited_cpu(); 
        if (cpu_limit < cpu_core) {
            cpu_limit = cpu_core;
        }
    }

    const int GROUP_KILL_MODE = 1;
    if (_mem_ctrl->SetKillMode(GROUP_KILL_MODE) != 0) {
        LOG(FATAL, "fail to set memory kill mode for task %ld",
                m_task_info.task_id());
        return -1;
    }

    if (_mem_ctrl->SetLimit(mem_size) != 0) {
        LOG(FATAL, "fail to set memory limit for task %ld", 
                m_task_info.task_id()); 
        return -1;
    }

    int64_t limit = static_cast<int64_t>(cpu_limit * CPU_CFS_PERIOD);
    if (limit < MIN_CPU_CFS_QUOTA) {
        limit = MIN_CPU_CFS_QUOTA;
    }

    if (_cpu_ctrl->SetCpuQuota(limit) != 0) {
        LOG(FATAL, "fail to set cpu quota for task %ld", 
                m_task_info.task_id()); 
        return -1;
    }
    int64_t quota = static_cast<int64_t>(cpu_core * CPU_SHARE_PER_CPU);
    if (quota < MIN_CPU_SHARE) {
        quota = MIN_CPU_SHARE; 
    }
    if (_cpu_ctrl->SetCpuShare(quota) != 0) {
        LOG(FATAL, "fail to set cpu share for task %ld",
                m_task_info.task_id()); 
        return -1;
    }

    std::string cgroup_name = 
        boost::lexical_cast<std::string>(m_task_info.task_id());
    // do dynamic scheduler regist
    if (FLAGS_agent_dynamic_scheduler_switch) {
        DynamicResourceScheduler* scheduler = 
            GetDynamicResourceScheduler();
        scheduler->RegistCgroupPath(cgroup_name, cpu_core, cpu_limit);
        //scheduler->SetFrozen(cgroup_name, 12 * 1000);
        scheduler->UnFrozen(cgroup_name);
    }

    std::string cpu_limit_file = FLAGS_cgroup_root +"/cpu/" + cgroup_name + "/cpu.cfs_quota_us";
    envs_.insert(std::pair<std::string, std::string>("CPU_LIMIT_FILE", cpu_limit_file));

    int ret = Start();
    if (0 != ret) {
        return ret;
    }
    if (m_task_info.has_monitor_conf() && 
            !m_task_info.monitor_conf().empty()) {
        LOG(INFO, "task %ld job %ld with monitor conf %s", 
                m_task_info.monitor_conf().c_str());
        StartMonitor();
    }
    return ret;
}

void ContainerTaskRunner::PutToCGroup(){
    pid_t my_pid = getpid();
    assert(_mem_ctrl->AttachTask(my_pid) == 0);
    assert(_cpu_ctrl->AttachTask(my_pid) == 0);
    assert(_cpu_acct_ctrl->AttachTask(my_pid) == 0);
}

std::string ContainerTaskRunner::GetProcessId() {
    std::string process_id;
    process_id = boost::lexical_cast<std::string>(m_task_info.task_id());
    process_id.append("-");
    process_id.append(boost::lexical_cast<std::string>(m_task_info.job_id()));
    return process_id;
}

std::string ContainerTaskRunner::GetMonitorProcessId() {
    std::string process_id;
    process_id = boost::lexical_cast<std::string>(m_task_info.task_id());
    process_id.append("-");
    process_id.append(boost::lexical_cast<std::string>(m_task_info.job_id()));
    process_id.append("-MONITOR");
    return process_id;
}

int ContainerTaskRunner::IsRunning() {
    return CheckProcess(GetProcessId());
}

int ContainerTaskRunner::CheckProcess(const std::string& process_id) {
    if (rpc_client_ == NULL) {
        return -1;     
    }    
    Guarder_Stub* guarder; 
    rpc_client_->GetStub(FLAGS_agent_guarder_addr, &guarder);
    CheckProcessRequest request;
    CheckProcessResponse response;
    request.set_process_id(process_id);
    bool ret = rpc_client_->SendRequest(guarder,
                    &galaxy::Guarder_Stub::CheckProcess,
                    &request, &response, 5, 1);
    if (!ret
            || (response.has_status()
                && response.status() != 0)) {
        LOG(WARNING, "check task %ld job %ld failed status %d", 
                m_task_info.task_id(),
                m_task_info.job_id(),
                response.status());
        return -1;    
    } 
    if (response.process_state() == kProcessStateRunning) {
        return 0; 
    }
    LOG(WARNING, "check task %ld job "
            "%ld not run exit_code %d",
            m_task_info.task_id(),
            m_task_info.job_id(),
            response.process_exit_code()); 
    if (response.has_process_exit_code() 
            && response.process_exit_code() == 0) {
        return 1; 
    }
    return -1;
}


void ContainerTaskRunner::InitExecEnvs(std::vector<std::string>* envs) {
    if (envs == NULL) {
        return; 
    }

    std::string TASK_ID = "TASK_ID=";
    TASK_ID.append(
            boost::lexical_cast<std::string>(
                m_task_info.task_offset()));
    envs->push_back(TASK_ID);
    std::string TASK_NUM = "TASK_NUM=";
    TASK_NUM.append(
            boost::lexical_cast<std::string>(
                m_task_info.job_replicate_num()));
    envs->push_back(TASK_NUM);
    return;
}

int ContainerTaskRunner::Start() {
    LOG(INFO, "start a task with id %ld", m_task_info.task_id());

    if (IsRunning() == 0) {
        LOG(WARNING, "task with id %ld has been runing", m_task_info.task_id());
        return -1;
    }
    SetStatus(RUNNING);
    if (rpc_client_ == NULL) {
        return -1; 
    }

    Guarder_Stub* guarder;  
    rpc_client_->GetStub(FLAGS_agent_guarder_addr, &guarder);
    RunProcessRequest request;
    RunProcessResponse response;

    request.set_process_id(GetProcessId());
    request.set_start_cmd(m_task_info.cmd_line());
    request.set_user(FLAGS_task_acct);
    request.set_cgroup_path(boost::lexical_cast<std::string>(m_task_info.task_id()));
    request.set_pwd(m_workspace->GetPath());
    std::vector<std::string> envs;
    InitExecEnvs(&envs);
    for (size_t i = 0; i < envs.size(); i++) {
        request.add_envs(envs[i]);     
    }
    bool ret = rpc_client_->SendRequest(guarder,
                                        &Guarder_Stub::RunProcess, 
                                        &request, 
                                        &response, 5, 1);
    if (!ret 
            || (response.has_status()
                && response.status() != 0)) {
        LOG(WARNING, "task %ld job %ld run process failed",
                m_task_info.task_id(),
                m_task_info.job_id()); 
        return -1;
    }
    LOG(INFO, "task %ld job %ld run process pid %d",
            m_task_info.task_id(),
            m_task_info.job_id(),
            response.pid());
    // TODO for monitor fail should fail?
    if (m_task_info.has_monitor_conf() && 
            !m_task_info.monitor_conf().empty()) {
        LOG(INFO, "task %ld job %ld with monitor conf %s", 
                m_task_info.monitor_conf().c_str());
        StartMonitor();
    }
    return 0;
}

int ContainerTaskRunner::StartMonitor() {
    LOG(INFO, "start a monitor with id %ld", m_task_info.task_id());

    if (CheckProcess(GetMonitorProcessId()) == 0) {
        LOG(WARNING, "task [%ld] job [%ld] has been monitoring", 
                m_task_info.task_id(), 
                m_task_info.job_id());
        return 0;
    }
    std::string monitor_conf = m_workspace->GetPath() + "/galaxy_monitor/monitor.conf";
    int conf_fd = open(monitor_conf.c_str(), O_WRONLY | O_CREAT, S_IRWXU);
    if (conf_fd == -1) {
        LOG(FATAL, "open monitor_conf %s failed [%d:%s]", 
                monitor_conf.c_str(), errno, strerror(errno));
        return -1;
    } else {
        int len = write(conf_fd, (void*)m_task_info.monitor_conf().c_str(),
                m_task_info.monitor_conf().size());
        if (len == -1) {
            LOG(FATAL, "write monitor_conf %s failed [%d:%s]",monitor_conf.c_str(),
                    errno, strerror(errno));
            close(conf_fd);
            return -1;

        }
        close(conf_fd);
    }
    Guarder_Stub* guarder;
    rpc_client_->GetStub(
            FLAGS_agent_guarder_addr, &guarder);
    RunProcessRequest request; 
    RunProcessResponse response;
    request.set_process_id(GetMonitorProcessId());
    request.set_start_cmd("/home/galaxy/monitor/monitor_agent "
            "--monitor_conf_path=" + monitor_conf);
    request.set_user(FLAGS_task_acct);
    request.set_cgroup_path(boost::lexical_cast<std::string>(m_task_info.task_id()));
    std::string env_task_id = "TASK_ID=";
    env_task_id.append(
            boost::lexical_cast<std::string>(
                m_task_info.task_id()));

    request.add_envs(env_task_id);
    std::string monitor_pwd = m_workspace->GetPath();
    monitor_pwd.append("/galaxy_monitor/");
    request.set_pwd(monitor_pwd);
    bool ret = rpc_client_->SendRequest(guarder,
            &Guarder_Stub::RunProcess,
            &request,
            &response, 5, 1);
    if (!ret 
            || (response.has_status()
                && response.status() != 0)) {
        LOG(WARNING, "start task %ld job %ld monitor failed",
                m_task_info.task_id(),
                m_task_info.job_id());
        return -1;
    } 
    LOG(INFO, "monitor task %ld job %ld run process pid %d",
            m_task_info.task_id(),
            m_task_info.job_id(),
            response.pid());
    return 0;
}

void ContainerTaskRunner::Status(TaskStatus* status) {
    if (collector_ != NULL) {
        status->set_cpu_usage(collector_->GetCpuCoresUsage());
        status->set_memory_usage(collector_->GetMemoryUsage());
        LOG(WARNING, "cpu usage %f memory usage %ld",
                status->cpu_usage(), status->memory_usage());
    }
    status->set_job_id(m_task_info.job_id());
    
    if (m_task_state == KILLED) {
        // be kill by master no need check running
        status->set_status(KILLED);
        LOG(WARNING, "task with id %ld state %s", 
            m_task_info.task_id(), 
            TaskState_Name(TaskState(m_task_state)).c_str());
        return;
    }
    // check if it is running
    int ret = IsRunning();
    if (ret == 0) {
        SetStatus(RUNNING);
        status->set_status(RUNNING);
    }
    else if (ret == 1) {
        SetStatus(COMPLETE);
        status->set_status(COMPLETE);
    }
    // last state is running ==> download finish
    else if (m_task_state == RUNNING
             || m_task_state == RESTART) {
        SetStatus(RESTART);
        if (ReStart() == 0) {
            status->set_status(RESTART); 
        }
        else {
            SetStatus(ERROR);
            status->set_status(ERROR); 
        }
    }
    // other state
    else {
        status->set_status(m_task_state); 
    }
    LOG(WARNING, "task with id %ld state %s", 
            m_task_info.task_id(), 
            TaskState_Name(TaskState(m_task_state)).c_str());
    return;
}

void ContainerTaskRunner::StopPost() {
    if (collector_ != NULL) {
        collector_->Clear();
    }
    std::string meta_file = persistence_path_dir_ 
        + "/" + RUNNER_META_PREFIX 
        + boost::lexical_cast<std::string>(sequence_id_);
    if (!file::Remove(meta_file)) {
        LOG(WARNING, "remove %s failed", meta_file.c_str()); 
    }
    std::string monitor_meta = persistence_path_dir_
        + "/" + MONITOR_META_PREFIX
        + boost::lexical_cast<std::string>(sequence_id_);
    if (!file::Remove(monitor_meta)) {
        LOG(WARNING, "rm monitor meta failed rm %s",
                monitor_meta.c_str());
    }
    if (!file::Remove(persistence_path_dir_)) {
        LOG(WARNING, "rm persisten dir failed rm %s",
                persistence_path_dir_.c_str()); 
    }
    return;
}

int ContainerTaskRunner::Stop() {
    int ret = StopInternal();
    if (ret != 0) {
        if (_cpu_ctrl->AdjustCpuQuota() == 0) {
            ret = StopInternal();         
        }
    }

    if (ret == 0) {
        StopPost(); 
    }
    return ret;
}

int ContainerTaskRunner::StopInternal() {
    if (m_task_state == DEPLOYING) {
        // do download stop
        DownloaderManager* downloader_handler = DownloaderManager::GetInstance();
        downloader_handler->KillDownload(downloader_id_);
        LOG(DEBUG, "task id %ld stop failed with deploying", m_task_info.task_id());
        return -1;
    }
    
    LOG(INFO, "start to kill process task %ld job %ld", 
            m_task_info.task_id(),
            m_task_info.job_id());
    Guarder_Stub* guarder; 
    rpc_client_->GetStub(
                FLAGS_agent_guarder_addr, &guarder);
    KillProcessRequest request;
    KillProcessResponse response;
    request.set_process_id(GetProcessId());
    request.set_signal(9);
    request.set_is_kill_group(true);
    
    bool ret = rpc_client_->SendRequest(guarder,
                                &Guarder_Stub::KillProcess,
                                &request, &response,
                                5, 1);
    if (!ret ||
            (response.has_status()
             && response.status() != 0)) {
        LOG(WARNING, "task %ld job %ld kill process failed status %d",
                m_task_info.task_id(),
                m_task_info.job_id(),
                response.status()); 

        if (response.has_kill_errno() 
                && response.kill_errno() != ESRCH) {
            return -1; 
        }
    }

    if (IsRunning() == 0) {
        LOG(WARNING, "task %ld job %ld is still run after kill",
                m_task_info.task_id(),
                m_task_info.job_id()); 
        return -1;
    }

    LOG(INFO, "start to kill monitor task %ld job %ld",
            m_task_info.task_id(), 
            m_task_info.job_id());

    if (m_task_info.has_monitor_conf() 
            && !m_task_info.monitor_conf().empty()) {
        KillProcessRequest monitor_request;
        KillProcessResponse monitor_response;
        monitor_request.set_process_id(GetMonitorProcessId());
        monitor_request.set_signal(9);
        monitor_request.set_is_kill_group(true);
        ret = rpc_client_->SendRequest(guarder,
                &Guarder_Stub::KillProcess,
                &monitor_request, &monitor_response, 5, 1);
        if (!ret ||
                (monitor_response.has_status()
                 && monitor_response.status() != 0)) {
            LOG(WARNING, "task %ld job %ld kill process failed status %d",
                    m_task_info.task_id(),
                    m_task_info.job_id(),
                    response.status()); 
            if (monitor_response.has_kill_errno() 
                    && monitor_response.kill_errno() != ESRCH) {
                return -1;
            } 
        }
        if (CheckProcess(GetMonitorProcessId()) == 0) {
            LOG(WARNING, "task %ld job %ld is still run after kill",
                    m_task_info.task_id(),
                    m_task_info.job_id()); 
            return -1;
        }
    }

    return 0;
}

bool ContainerTaskRunner::RecoverRunner(const std::string& persistence_path) {
    std::vector<std::string> files;
    if (!file::GetDirFilesByPrefix(
                persistence_path,
                RUNNER_META_PREFIX,
                &files)) {
        LOG(WARNING, "get meta files failed"); 
        return false;
    }

    if (files.size() == 0) {
        return true; 
    }

    int max_seq_id = -1;
    std::string last_meta_file;
    for (size_t i = 0; i < files.size(); i++) {
        std::string file = files[i]; 
        size_t pos = file.find(RUNNER_META_PREFIX);
        if (pos == std::string::npos) {
            continue; 
        }

        if (pos + RUNNER_META_PREFIX.size() >= file.size()) {
            LOG(WARNING, "meta file format err %s", file.c_str()); 
            continue;
        }

        int cur_id = atoi(file.substr(
                    pos + RUNNER_META_PREFIX.size()).c_str());
        if (max_seq_id < cur_id) {
            max_seq_id = cur_id; 
            last_meta_file = file;
        }
    }

    if (max_seq_id < 0) {
        return false; 
    }

    std::string meta_file = last_meta_file;
    LOG(DEBUG, "start to recover %s", meta_file.c_str());
    int fin = open(meta_file.c_str(), O_RDONLY);
    if (fin == -1) {
        LOG(WARNING, "open meta file failed %s err[%d: %s]", 
                meta_file.c_str(),
                errno,
                strerror(errno)); 
        return false;
    }

    size_t value;
    int len = read(fin, (void*)&value, sizeof(value));
    if (len == -1) {
        LOG(WARNING, "read meta file failed err[%d: %s]",
                errno,
                strerror(errno)); 
        close(fin);
        return false;
    }

    value = 0;
    len = read(fin, (void*)&value, sizeof(value));
    if (len == -1) {
        LOG(WARNING, "read meta file failed err[%d: %s]",
                errno,
                strerror(errno));
        close(fin);
        return false;
    }

    close(fin);

    std::vector<std::string> support_cgroup;  
    // TODO configable
    support_cgroup.push_back("memory");
    support_cgroup.push_back("cpu");
    support_cgroup.push_back("cpuacct");
    
    LOG(DEBUG, "destroy cgroup %lu", value);
    CGroupCtrl ctl(FLAGS_cgroup_root, support_cgroup);
    std::stringstream cpu_quota_group;
    cpu_quota_group << FLAGS_cgroup_root << "/cpu/" << value;
    CpuCtrl cpu_ctrl(cpu_quota_group.str());
    int max_retry_times = 10;
    int ret = -1;
    while (max_retry_times-- > 0) {
        ret = ctl.Destroy(value);
        if (ret != 0) {
            cpu_ctrl.AdjustCpuQuota();
            common::ThisThread::Sleep(100);
            continue;
        }    
        break;
    }
    if (ret == 0) {
        LOG(DEBUG, "destroy cgroup %lu success", value);
        return true; 
    }
    return false;
}

bool ContainerTaskRunner::RecoverMonitor(const std::string& persistence_path) {
    std::vector<std::string> files;
    if (!file::GetDirFilesByPrefix(
                persistence_path,
                MONITOR_META_PREFIX,
                &files)) {
        LOG(WARNING, "get meta files failed"); 
        return false;
    }

    if (files.size() == 0) {
        return true; 
    }

    int max_seq_id = -1;
    std::string last_meta_file;
    for (size_t i = 0; i < files.size(); i++) {
        std::string file = files[i]; 
        size_t pos = file.find(MONITOR_META_PREFIX);
        if (pos == std::string::npos) {
            continue; 
        }

        if (pos + MONITOR_META_PREFIX.size() >= file.size()) {
            LOG(WARNING, "meta file format err %s", file.c_str()); 
            continue;
        }

        int cur_id = atoi(file.substr(
                    pos + MONITOR_META_PREFIX.size()).c_str());
        if (max_seq_id < cur_id) {
            max_seq_id = cur_id; 
            last_meta_file = file;
        }
    }

    if (max_seq_id < 0) {
        return false; 
    }

    std::string meta_file = last_meta_file;
    LOG(DEBUG, "start to recover %s", meta_file.c_str());
    int fin = open(meta_file.c_str(), O_RDONLY);
    if (fin == -1) {
        LOG(WARNING, "open meta file failed %s err[%d: %s]", 
                meta_file.c_str(),
                errno,
                strerror(errno)); 
        return false;
    }

    size_t value;
    int len = read(fin, (void*)&value, sizeof(value));
    if (len == -1) {
        LOG(WARNING, "read meta file failed err[%d: %s]",
                errno,
                strerror(errno)); 
        close(fin);
        return false;
    }
    LOG(DEBUG, "recove monitor gpid %lu", value);
    if (0 != value) {
        int ret = killpg((pid_t)value, 9);
        if (ret != 0 && errno != ESRCH) {
            LOG(WARNING, "fail to kill monitor group %lu", value); 
            return false;
        }
    }
    close(fin);
    return true;
}

int ContainerTaskRunner::Clean() {
    // dynamic scheduler unregist
    if (FLAGS_agent_dynamic_scheduler_switch) {
        DynamicResourceScheduler* scheduler = 
            GetDynamicResourceScheduler();
        std::string cgroup_name = 
            boost::lexical_cast<std::string>(m_task_info.task_id());
        scheduler->UnRegistCgroupPath(cgroup_name);
    }
    if (_cg_ctrl != NULL) {
        int status = _cg_ctrl->Destroy(m_task_info.task_id());
        LOG(INFO,"destroy cgroup for task %ld with status %d",m_task_info.task_id(),status);
        if (status != 0) {
            status = _cpu_ctrl->AdjustCpuQuota();
            if (status == 0) {
                status = _cg_ctrl->Destroy(m_task_info.task_id());
            }
        }
        if (status != 0) {
            return status;
        }
    }
    LOG(WARNING, "cgroup not inited for task %ld", m_task_info.task_id());
    
    // clean guarder status 
    Guarder_Stub* guarder = NULL; 
    rpc_client_->GetStub(FLAGS_agent_guarder_addr, &guarder);
    RemoveProcessStateRequest request;
    RemoveProcessStateResponse response;

    request.set_process_id(GetProcessId());
    bool ret = rpc_client_->SendRequest(guarder,
            &Guarder_Stub::RemoveProcessState,
            &request, &response, 5, 1);
    if (!ret
            || (response.has_status()
                && response.status() != 0)) {
        LOG(WARNING, "remove process status failed task %ld job %ld",
                m_task_info.task_id(),
                m_task_info.job_id());    
        return -1;
    }
    
    if (m_task_info.has_monitor_conf()
            && !m_task_info.monitor_conf().empty()) {
        RemoveProcessStateRequest monitor_request;
        RemoveProcessStateResponse monitor_response;
        monitor_request.set_process_id(GetMonitorProcessId());
        ret = rpc_client_->SendRequest(guarder,
                &Guarder_Stub::RemoveProcessState,
                &request, &response, 5, 1);
        if (!ret 
                || (response.has_status() 
                    && response.status() != 0)) {
            LOG(WARNING, "remove monitor process status failed task %ld job %ld",
                    m_task_info.task_id(),
                    m_task_info.job_id()); 
            return -1;
        }
    }

    
    return 0;
}

}
