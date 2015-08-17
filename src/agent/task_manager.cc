// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/task_manager.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/bind.hpp>

#include "gflags/gflags.h"
#include "agent/utils.h"
#include "logging.h"
#include "timer.h"
#include "agent/cgroups.h"

DECLARE_string(gce_cgroup_root);
DECLARE_string(gce_support_subsystems);
DECLARE_string(agent_work_dir);

namespace baidu {
namespace galaxy {

TaskManager::TaskManager() : 
    tasks_mutex_(),
    tasks_(),
    background_thread_(5), 
    cgroup_root_(FLAGS_gce_cgroup_root),
    hierarchies_(),
    rpc_client_(NULL) {
    rpc_client_ = new RpcClient();
}

TaskManager::~TaskManager() {
    if (rpc_client_ != NULL) {
        delete rpc_client_; 
        rpc_client_ = NULL;
    }
}

int TaskManager::Init() {
    if (!FLAGS_gce_support_subsystems.empty()) {
        std::vector<std::string> sub_systems;
        boost::split(sub_systems,
                FLAGS_gce_support_subsystems,
                boost::is_any_of(","),
                boost::token_compress_on);      
        for (size_t i = 0; i < sub_systems.size(); i++) {
            std::string hierarchy = 
                FLAGS_gce_cgroup_root + "/" + sub_systems[i];         
            if (!file::IsExists(hierarchy)) {
                LOG(WARNING, "hierarchy %s not exists",
                        hierarchy.c_str());
                return -1;
            }
            hierarchies_.push_back(hierarchy);
        }  
    }
    return 0;
}

int TaskManager::CreateTask(const TaskInfo& task) {
    MutexLock scope_lock(&tasks_mutex_);    
    std::map<std::string, TaskInfo*>::iterator it = 
        tasks_.find(task.task_id);
    if (it != tasks_.end()) {
        LOG(WARNING, "task %s already added",
                task.task_id.c_str()); 
        return 0;
    }
    TaskInfo* task_info = new TaskInfo(task);     
    task_info->stage = kStagePENDING;
    task_info->status.set_state(kPodPending);
    tasks_[task_info->task_id] = task_info;
    // 1. prepare workspace
    if (PrepareWorkspace(task_info) != 0) {
        LOG(WARNING, "task %s prepare workspace failed",
                task_info->task_id.c_str());
        return -1;
    }
    if (PrepareCgroupEnv(task_info) != 0) {
        LOG(WARNING, "task %s prepare cgroup failed",
                task_info->task_id.c_str()); 
        return -1;
    }
    if (PrepareResourceCollector(task_info) != 0) {
        LOG(WARNING, "task %s prepare resource collector failed",
                task_info->task_id.c_str()); 
        return -1;
    }
    if (PrepareVolumeEnv(task_info) != 0) {
        LOG(WARNING, "task %s prepare volume env failed",
                task_info->task_id.c_str()); 
        return -1;
    }
    LOG(INFO, "task %s is add", task.task_id.c_str());
    background_thread_.DelayTask(
                    50, 
                    boost::bind(
                        &TaskManager::DelayCheckTaskStageChange,
                        this, task_info->task_id));
    return 0;
}

int TaskManager::DeleteTask(const std::string& task_id) {
    MutexLock scope_lock(&tasks_mutex_);    
    // change stage to stop and delay to end
    std::map<std::string, TaskInfo*>::iterator it = 
        tasks_.find(task_id); 
    if (it == tasks_.end()) {
        LOG(WARNING, "delete task %s already not exists",
                task_id.c_str());
        return 0; 
    }

    LOG(INFO, "task %s is add to delete", task_id.c_str());
    TerminateTask(it->second);
    return 0;
}

int TaskManager::RunTask(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }
    task_info->stage = kStageRUNNING;
    task_info->status.set_state(kPodRunning);
    task_info->main_process.set_key(task_info->task_id + "_main");
    // send rpc to initd to execute main process
    ExecuteRequest initd_request; 
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->main_process.key());
    initd_request.set_commands(task_info->desc.start_command());
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->cgroup_path);
    std::string* pod_id = initd_request.add_envs();
    pod_id->append("POD_ID=");
    pod_id->append(task_info->pod_id);
    std::string* task_id = initd_request.add_envs();
    task_id->append("TASK_ID=");
    task_id->append(task_info->task_id);
    Initd_Stub* initd;
    if (!rpc_client_->GetStub(
                task_info->initd_endpoint, &initd)) {
        LOG(WARNING, "get initd stub failed for %s",
                task_info->task_id.c_str()); 
        return -1;
    }

    bool ret = rpc_client_->SendRequest(initd,
                                        &Initd_Stub::Execute, 
                                        &initd_request, 
                                        &initd_response, 5, 1);
    if (!ret) {
        LOG(WARNING, "start command [%s] rpc failed for %s",
                task_info->desc.start_command().c_str(),
                task_info->task_id.c_str()); 
        return -1;
    } else if (initd_response.has_status()
            && initd_response.status() != kOk) {
        LOG(WARNING, "start command [%s] failed %s for %s",
                task_info->desc.start_command().c_str(),
                Status_Name(initd_response.status()).c_str(),
                task_info->task_id.c_str()); 
        return -1;
    }
    LOG(INFO, "start command [%s] execute success for %s",
            task_info->desc.start_command().c_str(),
            task_info->task_id.c_str());
    return 0;
}

int TaskManager::QueryTask(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }
    MutexLock scope_lock(&tasks_mutex_);    
    std::map<std::string, TaskInfo*>::iterator it = 
        tasks_.find(task_info->task_id);      
    if (it == tasks_.end()) {
        LOG(WARNING, "query task %s not exists", 
                task_info->task_id.c_str());
        return -1; 
    }
    task_info->status.CopyFrom(it->second->status);
    return 0;
}

int TaskManager::TerminateTask(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }
    std::string stop_command = task_info->desc.stop_command();
    task_info->stage = kStageSTOPPING;
    task_info->stop_process.set_key(task_info->task_id + "_stop");
    // send rpc to initd to execute stop process
    ExecuteRequest initd_request; 
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->stop_process.key());
    initd_request.set_commands(stop_command);
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->cgroup_path);
    Initd_Stub* initd;
    if (!rpc_client_->GetStub(task_info->initd_endpoint, &initd)) {
        LOG(WARNING, "get stub failed"); 
        return -1;
    }

    bool ret = rpc_client_->SendRequest(initd,
                                        &Initd_Stub::Execute,
                                        &initd_request,
                                        &initd_response,
                                        5, 1);
    if (!ret) {
        LOG(WARNING, "stop command [%s] rpc failed for %s",
                stop_command.c_str(),
                task_info->task_id.c_str()); 
        return -1;
    } else if (initd_response.has_status()
                && initd_response.status() != kOk) {
        LOG(WARNING, "stop command [%s] failed %s for %s",
                stop_command.c_str(),
                Status_Name(initd_response.status()).c_str(),
                task_info->task_id.c_str()); 
        return -1;
    }
    LOG(INFO, "stop command [%s] execute success for %s",
            stop_command.c_str(),
            task_info->task_id.c_str());
    int32_t now_time = common::timer::now_time();
    // TODO config stop timeout len
    task_info->stop_timeout_point = now_time + 100;
    return 0;
}

int TaskManager::PrepareCgroupEnv(TaskInfo* task) {
    if (task == NULL) {
        return -1; 
    }
    if (hierarchies_.size() == 0) {
        return 0; 
    }
    std::vector<std::string>::iterator hier_it = 
        hierarchies_.begin();
    std::string cgroup_name = task->task_id;
    for (; hier_it != hierarchies_.end(); ++ hier_it) {
        std::string cgroup_dir = *hier_it;
        cgroup_dir.append("/");
        cgroup_dir.append(cgroup_name);
        if (!file::Mkdir(cgroup_dir)) {
            LOG(WARNING, "create dir %s failed for %s",
                    cgroup_dir.c_str(), task->task_id.c_str()); 
            return -1;
        }              
    }
    // TODO use cpu share ?
    std::string cpu_hierarchy = FLAGS_gce_cgroup_root + "/cpu"; 
    std::string mem_hierarchy = FLAGS_gce_cgroup_root + "/memory";
    // set cpu share 
    int32_t cpu_share = 
        task->desc.requirement().millicores() * 512;
    if (cgroups::Write(cpu_hierarchy, 
                cgroup_name, 
                "cpu.share", 
                boost::lexical_cast<std::string>(cpu_share)
                ) != 0) {
        LOG(WARNING, "set cpu share %d failed for %s", 
                cpu_share,
                cgroup_name.c_str()); 
        return -1;
    }
    // set memory limit
    int64_t memory_limit = 
            1024L * 1024 * task->desc.requirement().memory();
    if (cgroups::Write(mem_hierarchy, 
                cgroup_name, 
                "memory.limit_in_bytes", 
                boost::lexical_cast<std::string>(memory_limit)
                ) != 0) {
        LOG(WARNING, "set memory limit %ld failed for %s",
                memory_limit,
                cgroup_name.c_str()); 
        return -1;
    } 

    const int GROUP_KILL_MODE = 1;
    if (file::IsExists(mem_hierarchy 
                + "/" + cgroup_name + "/memory.kill_mode")
            && cgroups::Write(mem_hierarchy, 
                cgroup_name, 
                "memory.kill_mode", 
                boost::lexical_cast<std::string>(GROUP_KILL_MODE)
                ) != 0) {
        LOG(WARNING, "set memory kill mode failed for %s",
                cgroup_name.c_str());
        return -1;
    }

    return 0;
}

int TaskManager::PrepareResourceCollector(TaskInfo* /*task_info*/) {
    return 0;
}

int TaskManager::PrepareVolumeEnv(TaskInfo* /*task_info*/) {
    return 0;
}

int TaskManager::CleanVolumeEnv(TaskInfo* /*task_info*/) {
    return 0;
}

int TaskManager::CleanResourceCollector(TaskInfo* /*task_info*/) {
    return 0;
}

int TaskManager::CleanCgroupEnv(TaskInfo* task) {
    if (task == NULL) {
        return -1; 
    } 
    if (hierarchies_.size() == 0) {
        return 0; 
    }
    // TODO do set control_file  frozen ?
    std::vector<std::string>::iterator hier_it = 
        hierarchies_.begin();
    std::string cgroup = task->task_id;
    for (; hier_it != hierarchies_.end(); ++hier_it) {
        std::string cgroup_dir = *hier_it;
        cgroup_dir.append("/");
        cgroup_dir.append(cgroup);
        if (!file::IsExists(cgroup_dir)) {
            LOG(INFO, "%s %s not exists",
                    (*hier_it).c_str(), cgroup.c_str()); 
            continue;
        }
        std::vector<std::string> pids;
        if (!cgroups::GetPidsFromCgroup(*hier_it, cgroup, &pids)) {
            LOG(WARNING, "get pids from %s failed",
                    cgroup.c_str());  
            return -1;
        }
        std::vector<std::string>::iterator pid_it = pids.begin();
        for (; pid_it != pids.end(); ++pid_it) {
            int pid = ::atoi((*pid_it).c_str());
            if (pid != 0) {
                ::kill(pid, SIGKILL); 
            }
        }
        if (::rmdir(cgroup_dir.c_str()) != 0
                && errno != ENOENT) {
            LOG(WARNING, "rmdir %s failed err[%d: %s]",
                    cgroup_dir.c_str(), errno, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int TaskManager::CleanProcess(TaskInfo* task_info) {
    if (task_info->deploy_process.has_status() 
            && task_info->deploy_process.status() 
                                == kProcessRunning) {
        // TODO query first?
        ::kill(task_info->deploy_process.pid(), SIGKILL); 
    }  
    if (task_info->main_process.has_status() 
            && task_info->main_process.status()
                                == kProcessRunning) {
        ::kill(task_info->main_process.pid(), SIGKILL); 
    }
    if (task_info->stop_process.has_status() 
            && task_info->stop_process.status()
                                == kProcessRunning) {
        ::kill(task_info->stop_process.pid(), SIGKILL); 
    }
    return 0;
}

int TaskManager::CleanTask(TaskInfo* task_info) {
    if (CleanProcess(task_info) != 0) {
        LOG(WARNING, "task %s clean process failed",
                task_info->task_id.c_str()); 
        return -1;
    }
    if (CleanVolumeEnv(task_info) != 0) {
        LOG(WARNING, "task %s clean volume env failed",
                task_info->task_id.c_str()); 
        return -1;
    }
    if (CleanResourceCollector(task_info) != 0) {
        LOG(WARNING, "task %s clean resource collector failed",
                task_info->task_id.c_str()); 
        return -1;
    }
    if (CleanCgroupEnv(task_info) != 0) {
        LOG(WARNING, "task %s clean cgroup env failed",
                task_info->task_id.c_str()); 
        return -1;
    }
    if (CleanWorkspace(task_info) != 0) {
        LOG(WARNING, "task %s clean workspace failed",
                task_info->task_id.c_str());
        return -1;
    }
    LOG(INFO, "task %s clean success", task_info->task_id.c_str());
    return 0;
}

int TaskManager::DeployTask(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }
    std::string deploy_command;
    task_info->stage = kStageDEPLOYING;
    task_info->status.set_state(kPodDeploy);
    if (task_info->desc.source_type() == kSourceTypeBinary) {
        // TODO write binary directly
        std::string tar_packet = task_info->task_workspace;
        tar_packet.append("/tmp.tar.gz");
        if (file::IsExists(tar_packet)) {
            file::Remove(tar_packet);     
        }
        if (DownloadByDirectWrite(
                    task_info->desc.binary(), 
                    tar_packet) != 0) {
            LOG(WARNING, "write binary failed for %s", 
                    task_info->task_id.c_str()); 
            return -1;
        }
        deploy_command = "tar -xzf "; 
        deploy_command.append(tar_packet);
    } else if (task_info->desc.source_type() 
            == kSourceTypeFTP) {
        // TODO add speed limit
        deploy_command = "wget "; 
        deploy_command.append(task_info->desc.binary());
        deploy_command.append(" -O tmp.tar.gz && tar -xzf tmp.tar.gz");
    }

    task_info->stage = kStageDEPLOYING;
    task_info->deploy_process.set_key(task_info->task_id + "_deploy");
    task_info->status.set_state(kPodDeploy);
    // send rpc to initd to execute deploy process; 
    ExecuteRequest initd_request;      
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->deploy_process.key());
    initd_request.set_commands(deploy_command);
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->cgroup_path);
    Initd_Stub* initd;
    if (!rpc_client_->GetStub(
                task_info->initd_endpoint, &initd)) {
        LOG(WARNING, "get initd stub failed for %s",
                task_info->task_id.c_str()); 
        return -1;
    }
    bool ret = rpc_client_->SendRequest(
                    initd, &Initd_Stub::Execute,
                    &initd_request,
                    &initd_response,
                    5, 1);
    if (!ret) {
        LOG(WARNING, "deploy command "
                "[%s] execute rpc failed for %s",
                deploy_command.c_str(),
                task_info->task_id.c_str());
        return -1;
    } else if (initd_response.has_status() 
            && initd_response.status() != kOk) {
        LOG(WARNING, "deploy command "
                "[%s] execute failed %s for %s",
                deploy_command.c_str(),
                Status_Name(initd_response.status()).c_str(),
                task_info->task_id.c_str()); 
        return -1;
    }
    LOG(INFO, "deploy command [%s] execute success for %s",
            deploy_command.c_str(),
            task_info->task_id.c_str());
    return 0;
}

int TaskManager::QueryProcessInfo(const std::string& initd_endpoint, 
                                  ProcessInfo* info) {
    if (info == NULL) {
        return -1; 
    }

    if (!info->has_key()) {
        return -1; 
    }
    GetProcessStatusRequest initd_request; 
    GetProcessStatusResponse initd_response;
    initd_request.set_key(info->key());
    Initd_Stub* initd;
    if (!rpc_client_->GetStub(initd_endpoint, &initd)) {
        LOG(WARNING, "get rpc stub failed"); 
        return -1;
    }

    bool ret = rpc_client_->SendRequest(initd,
                                        &Initd_Stub::GetProcessStatus,
                                        &initd_request,
                                        &initd_response,
                                        5, 1);
    if (!ret) {
        LOG(WARNING, "query key %s to %s rpc failed",
                info->key().c_str(), initd_endpoint.c_str()); 
        return -1;
    } else if (initd_response.has_status() 
            && initd_response.status() != kOk) {
        LOG(WARNING, "query key %s to %s failed %s",
                info->key().c_str(), initd_endpoint.c_str(),
                Status_Name(initd_response.status()).c_str()); 
        return -1;
    }

    info->CopyFrom(initd_response.process());
    return 0;
}

int TaskManager::PrepareWorkspace(TaskInfo* task) {
    std::string workspace_root = FLAGS_agent_work_dir;
    workspace_root.append("/");
    workspace_root.append(task->pod_id);
    if (!file::Mkdir(workspace_root)) {
        LOG(WARNING, "mkdir workspace root failed"); 
        return -1;
    }

    std::string task_workspace = workspace_root;
    task_workspace.append("/");
    task_workspace.append(task->task_id);
    if (!file::Mkdir(task_workspace)) {
        LOG(WARNING, "mkdir task workspace failed");
        return -1;
    }
    task->task_workspace = task_workspace;
    LOG(INFO, "task %s workspace %s",
            task->task_id.c_str(), task->task_workspace.c_str());
    return 0;
}

int TaskManager::CleanWorkspace(TaskInfo* task) {
    if (file::IsExists(task->task_workspace)
            && !file::Remove(task->task_workspace)) {
        LOG(WARNING, "Remove task %s workspace failed",
                task->task_id.c_str());
        return -1;
    }
    std::string workspace_root = FLAGS_agent_work_dir;
    workspace_root.append("/");
    workspace_root.append(task->pod_id);
    if (file::IsExists(workspace_root)
            && !file::Remove(workspace_root)) {
        return -1;     
    }
    return 0;
}

int TaskManager::TerminateProcessCheck(TaskInfo* task_info) {
    // check stop process       
    if (QueryProcessInfo(task_info->initd_endpoint,
                &(task_info->stop_process)) != 0) {
        LOG(WARNING, "task %s query stop process failed",
                task_info->task_id.c_str()); 
        task_info->status.set_state(kPodError);
        return -1;
    }
    // 1. check stop process status
    if (task_info->stop_process.status() == kProcessRunning) {
        LOG(DEBUG, "task %s stop process is still running",
                task_info->task_id.c_str()); 
        int32_t now_time = common::timer::now_time();
        if (task_info->stop_timeout_point >= now_time) {
            return -1; 
        }
        return 0;
    }
    // stop process exit_code is neccessary ?
    return -1;
}

int TaskManager::DeployProcessCheck(TaskInfo* task_info) {
    // 1. query deploy process status 
    if (QueryProcessInfo(task_info->initd_endpoint, 
                &(task_info->deploy_process)) != 0) {
        LOG(WARNING, "task %s check deploy state failed",
                task_info->task_id.c_str());
        task_info->status.set_state(kPodError);
        return -1;
    }
    // 2. check deploy process status 
    if (task_info->deploy_process.status() 
                                    == kProcessRunning) {
        LOG(DEBUG, "task %s deploy process still running",
                task_info->task_id.c_str());
        return 0;
    }
    // 3. check deploy process exit code
    if (task_info->deploy_process.exit_code() != 0) {
        LOG(WARNING, "task %s deploy process failed exit code %d",
                task_info->task_id.c_str(), 
                task_info->deploy_process.exit_code());         
        task_info->status.set_state(kPodError);
        return -1;
    }
    return 1;     
}

int TaskManager::InitdProcessCheck(TaskInfo* task_info) {
    const int MAX_INITD_CHECK_TIME = 10;
    if (task_info == NULL) {
        return -1; 
    }
    std::string initd_endpoint = task_info->initd_endpoint; 
    // only use to check rpc connected
    GetProcessStatusRequest initd_request; 
    GetProcessStatusResponse initd_response;
    Initd_Stub* initd;
    if (!rpc_client_->GetStub(initd_endpoint, &initd)) {
        LOG(WARNING, "get rpc stub failed"); 
        return -1;
    }

    bool ret = rpc_client_->SendRequest(initd,
                                        &Initd_Stub::GetProcessStatus,
                                        &initd_request,
                                        &initd_response,
                                        5, 1);
    if (!ret) {
        task_info->initd_check_failed ++;
        if (task_info->initd_check_failed 
                >= MAX_INITD_CHECK_TIME) {
            LOG(WARNING, "check initd times %d more than %d",
                    task_info->initd_check_failed, MAX_INITD_CHECK_TIME);
            return -1; 
        }
        return 0;
    }     
    return 1;
}

int TaskManager::RunProcessCheck(TaskInfo* task_info) {
    // 1. query main_process status
    if (QueryProcessInfo(task_info->initd_endpoint, 
                &(task_info->main_process)) != 0) {
        LOG(WARNING, "task %s check deploy state failed",
                task_info->task_id.c_str());
        task_info->status.set_state(kPodError);
        return -1;
    }
    // 2. check main process status
    if (task_info->main_process.status() == kProcessRunning) {
        LOG(DEBUG, "task %s check running",
                task_info->task_id.c_str());
        return 0;
    }

    // 3. check exit_code
    if (task_info->main_process.exit_code() != 0) {
        LOG(WARNING, "task %s main process run failed exit code %d",
                task_info->task_id.c_str(),
                task_info->main_process.exit_code()); 
        task_info->status.set_state(kPodError);
        return -1;
    }
    LOG(INFO, "task %s main process exit 0",
            task_info->task_id.c_str());
    task_info->status.set_state(kPodTerminate); 
    return 1;
}

void TaskManager::DelayCheckTaskStageChange(const std::string& task_id) {
    MutexLock scope_lock(&tasks_mutex_);    
    std::map<std::string, TaskInfo*>::iterator it = 
        tasks_.find(task_id);
    if (it == tasks_.end()) {
        return; 
    }

    TaskInfo* task_info = it->second;
    // switch task stage
    if (task_info->stage == kStagePENDING 
            && task_info->status.state() != kPodError) {
        int chk_res = InitdProcessCheck(task_info);
        if (chk_res == 1 && DeployTask(task_info) != 0) {
            LOG(WARNING, "task %s deploy failed",
                    task_info->task_id.c_str());
            task_info->status.set_state(kPodError); 
        } else if (chk_res == -1) {
            LOG(WARNING, "task %s check initd failed",
                    task_info->task_id.c_str()); 
            task_info->status.set_state(kPodError);
        }
    } else if (task_info->stage == kStageDEPLOYING
            && task_info->status.state() != kPodError) {
        int chk_res = DeployProcessCheck(task_info);
        // deploy success and run
        if (chk_res == 1 && RunTask(task_info) != 0) {
            LOG(WARNING, "task %s run failed",
                    task_info->task_id.c_str()); 
            task_info->status.set_state(kPodError);
        } else if (chk_res == -1) {
            LOG(WARNING, "task %s deploy failed",
                    task_info->task_id.c_str()); 
            task_info->status.set_state(kPodError);
        }
    } else if (task_info->stage == kStageRUNNING
            && task_info->status.state() != kPodError) {
        int chk_res = RunProcessCheck(task_info);
        if (chk_res == -1 && 
                task_info->fail_retry_times 
                    < task_info->max_retry_times) {
            task_info->fail_retry_times++;
            RunTask(task_info);         
        }
    } else if (task_info->stage == kStageSTOPPING) {
        int chk_res = TerminateProcessCheck(task_info);
        if (chk_res != 0) {
            if (0 == CleanTask(task_info)) {
                delete task_info;
                task_info = NULL;
                tasks_.erase(it);     
            } else {
                LOG(WARNING, "clean task %s failed",
                        task_info->task_id.c_str());    
                task_info->status.set_state(kPodError);
            }
        }
    }
    background_thread_.DelayTask(
                    500, 
                    boost::bind(
                        &TaskManager::DelayCheckTaskStageChange,
                        this, task_id));
    return; 
}

}   // ending namespace galaxy
}   // ending namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
