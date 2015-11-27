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
#include "agent/cgroups.h"
#include "agent/resource_collector.h"
#include "logging.h"
#include "timer.h"

DECLARE_string(gce_cgroup_root);
DECLARE_string(gce_support_subsystems);
DECLARE_string(agent_work_dir);
DECLARE_string(agent_global_cgroup_path);
DECLARE_string(agent_global_hardlimit_path);
DECLARE_string(agent_global_softlimit_path);
DECLARE_int32(agent_detect_interval);
DECLARE_int32(agent_millicores_share);
DECLARE_int64(agent_mem_share);
DECLARE_string(agent_default_user);
DECLARE_bool(agent_namespace_isolation_switch);
DECLARE_int32(send_bps_quota);
DECLARE_int32(recv_bps_quota);

namespace baidu {
namespace galaxy {
const static int CPU_CFS_PERIOD = 100000;
TaskManager::TaskManager() : 
    tasks_mutex_(),
    tasks_(),
    background_thread_(5), 
    cgroup_root_(FLAGS_gce_cgroup_root),
    hierarchies_(),
    rpc_client_(NULL),
    hardlimit_cores_(0){
    rpc_client_ = new RpcClient();
}

TaskManager::~TaskManager() {
    if (rpc_client_ != NULL) {
        delete rpc_client_; 
        rpc_client_ = NULL;
    }
}

int TaskManager::Init() {
    std::vector<std::string> sub_systems; 
    boost::split(sub_systems,
            FLAGS_gce_support_subsystems,
            boost::is_any_of(","),
            boost::token_compress_on);
    hierarchies_.clear();
    for (size_t i = 0; i < sub_systems.size(); i++) {
        if (sub_systems[i].empty()) {
            continue;
        }
        if (sub_systems[i] == "cpu") {
            bool ok = InitCpuSubSystem();
            if (!ok) {
                LOG(WARNING, "fail to init cpu sub system");
                return -1;
            }
            cgroup_funcs_.insert(std::make_pair("cpu", 
                        boost::bind(&TaskManager::HandleInitTaskCpuCgroup, this, sub_systems[i], _1)));
            continue;
        } else if (sub_systems[i] == "memory") {
            cgroup_funcs_.insert(std::make_pair("memory", 
                        boost::bind(&TaskManager::HandleInitTaskMemCgroup, this, sub_systems[i], _1)));
        } else if (sub_systems[i] == "tcp_throt") {
            InitTcpthrotEnv();
            cgroup_funcs_.insert(std::make_pair("tcp_throt",
                        boost::bind(&TaskManager::HandleInitTaskTcpCgroup, this, sub_systems[i], _1)));
        } else {
            cgroup_funcs_.insert(std::make_pair(sub_systems[i], 
             boost::bind(&TaskManager::HandleInitTaskComCgroup, this, sub_systems[i], _1)));
        }

        // no deal with freezer
        std::string hierarchy =
            FLAGS_gce_cgroup_root + "/" 
            + sub_systems[i];
        if (!file::IsExists(hierarchy)) {
            LOG(WARNING, "hierarchy %s not exists", hierarchy.c_str()); 
            return -1;
        }
        if (!file::Mkdir(hierarchy + "/" + FLAGS_agent_global_cgroup_path)) {
            LOG(WARNING, "mkdir global cgroup path failed"); 
            return -1;
        }
        LOG(INFO, "support cgroups hierarchy %s", hierarchy.c_str());
        hierarchies_[sub_systems[i]] = hierarchy;
    }

    LOG(INFO, "support cgroups types %u", sub_systems.size());
    return 0;
}

bool TaskManager::InitCpuSubSystem() {
    std::vector<std::string> sub_systems; 
    boost::split(sub_systems,
            FLAGS_gce_support_subsystems,
            boost::is_any_of(","),
            boost::token_compress_on);
    if (std::find(sub_systems.begin(), sub_systems.end(), "cpu") == sub_systems.end()) { 
        LOG(WARNING, "cpu sub system is disable");
        return true;
    }
    std::string hierarchy = FLAGS_gce_cgroup_root + "/cpu";
    if (!file::IsExists(hierarchy)) {
        LOG(WARNING, "cpu subfolder does not exist");
        return false;
    }
    hierarchies_["cpu"] = hierarchy;
    std::string hardlimit_folder = hierarchy + "/" + FLAGS_agent_global_hardlimit_path;
    if (!file::IsExists(hardlimit_folder)) {
        // add hard_limit folder
        bool mkdir_ok = file::Mkdir(hardlimit_folder);
        if (!mkdir_ok) {
            LOG(WARNING, "mkdir global cpu hardlimit path %s failed", hardlimit_folder.c_str());
            return false;
        } 
    }
    int write_ok = cgroups::Write(hardlimit_folder, 
                                  "cpu.cfs_quota_us",
                                  boost::lexical_cast<std::string>("-1"));

    if (write_ok != 0) {
        LOG(WARNING, "fail to write cpu global limit %d %s/%s ",
                -1,
                hardlimit_folder.c_str(), "cpu.cfs_quota_us");
        return false;
    }
    std::string softlimit_folder = hierarchy + "/" + FLAGS_agent_global_softlimit_path;
    // add soft_limit
    if (!file::IsExists(softlimit_folder)) {
        bool mkdir_ok = file::Mkdir(softlimit_folder);
        if (!mkdir_ok) {
            LOG(WARNING, "mkdir global cpu softlimit path %s failed", softlimit_folder.c_str());
            return false;
        }
    }
    int32_t softlimit_cores = FLAGS_agent_millicores_share * (CPU_CFS_PERIOD / 1000); 
    write_ok = cgroups::Write(softlimit_folder, 
                                  "cpu.cfs_quota_us",
                                  boost::lexical_cast<std::string>(softlimit_cores));
    if (write_ok != 0) {
        LOG(WARNING, "fail to write softlimit quota %d to %s", softlimit_cores, softlimit_folder.c_str());
        return false;
    }
    return true;
}

bool TaskManager::HandleHardlimitChange(int32_t hardlimit_cores) {
    tasks_mutex_.AssertHeld();
    if (hierarchies_.find("cpu") == hierarchies_.end()) {
        LOG(WARNING, "cpu subsystem is disabled");
        return true;
    }
    std::string softlimit_folder = hierarchies_["cpu"] + "/" + FLAGS_agent_global_softlimit_path;
    int32_t softlimit_cores = (FLAGS_agent_millicores_share - hardlimit_cores) * (CPU_CFS_PERIOD / 1000);
    int write_ok = cgroups::Write(softlimit_folder, 
                                  "cpu.cfs_quota_us",
                                  boost::lexical_cast<std::string>(softlimit_cores));
    if (write_ok != 0) {
        LOG(WARNING, "fail to write softlimit quota %d to %s", softlimit_cores, softlimit_folder.c_str());
        return false;
    }
    return true;
}

int TaskManager::ReloadTask(const TaskInfo& task) {
    MutexLock scope_lock(&tasks_mutex_);
    std::map<std::string, TaskInfo*>::iterator it = 
        tasks_.find(task.task_id);
    if (it != tasks_.end()) {
        LOG(WARNING, "task %s already added",
                task.task_id.c_str()); 
        return 0;
    }

    TaskInfo* task_info = new TaskInfo(task);
    tasks_[task.task_id] = task_info;
    task_info->status.set_state(kTaskPending);
    if (PrepareWorkspace(task_info) != 0) {
        LOG(WARNING, "task %s prepare workspace failed in reload",
                task_info->task_id.c_str()); 
        // TODO  should do some other prepare such as cgroups
        task_info->status.set_state(kTaskError);
        return 0;
    }
    if (PrepareCgroupEnv(task_info) != 0) {
        LOG(WARNING, "task %s prepare cgroup failed in reload",
                task_info->task_id.c_str()); 
        task_info->status.set_state(kTaskError);
        return 0;
    }

    if (PrepareResourceCollector(task_info) != 0) {
        LOG(WARNING, "task %s prepare resource collector failed in reload",
                task_info->task_id.c_str()); 
        task_info->status.set_state(kTaskError);
        return 0;
    }

    SetupDeployProcessKey(task_info);
    SetupRunProcessKey(task_info);
    SetupTerminateProcessKey(task_info);

    if (task_info->initd_stub == NULL 
            && !rpc_client_->GetStub(task_info->initd_endpoint,
                                     &(task_info->initd_stub))) {
        LOG(WARNING, "get stub failed");     
        task_info->status.set_state(kTaskError);
        return 0;
    }
    // check if in deploy stage
    do {
        task_info->stage = kTaskStageDEPLOYING; 
        int ret = DeployProcessCheck(task_info);
        if (ret == -1) {
            LOG(WARNING, "task %s check deploy failed",
                    task_info->task_id.c_str());
            break;
        } else if (ret == 0) {
            task_info->status.set_state(kTaskDeploy);
            break;
        }
        task_info->stage = kTaskStageRUNNING; 
        ret = RunProcessCheck(task_info);
        if (ret == -1) {
            LOG(WARNING, "task %s check run failed",
                    task_info->task_id.c_str()); 
            break;
        } else if (ret == 0) {
            task_info->status.set_state(kTaskRunning);
            break;
        } 
        ret = TerminateProcessCheck(task_info);
        if (ret == -1) {
            LOG(WARNING, "task %s check stop failed",
                    task_info->task_id.c_str()); 
            break;
        } else if (ret == 0) {
            task_info->status.set_state(kTaskTerminate);
            break;
        }
        task_info->status.set_state(kTaskFinish);
    } while (0);

    LOG(INFO, "task %s is reload", task_info->task_id.c_str());
    background_thread_.DelayTask(
                    50, 
                    boost::bind(
                        &TaskManager::DelayCheckTaskStageChange,
                        this, task_info->task_id));
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
    task_info->stage = kTaskStagePENDING;
    task_info->status.set_state(kTaskPending);
    tasks_[task_info->task_id] = task_info;
    // 1. prepare workspace
    if (PrepareWorkspace(task_info) != 0) {
        LOG(WARNING, "task %s prepare workspace failed",
                task_info->task_id.c_str());
        task_info->status.set_state(kTaskError);
        return -1;
    }
    if (PrepareCgroupEnv(task_info) != 0) {
        LOG(WARNING, "task %s prepare cgroup failed",
                task_info->task_id.c_str()); 
        task_info->status.set_state(kTaskError);
        return -1;
    }

    if (PrepareResourceCollector(task_info) != 0) {
        LOG(WARNING, "task %s prepare resource collector failed",
                task_info->task_id.c_str());
        task_info->status.set_state(kTaskError);
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
    tasks_mutex_.AssertHeld();
    if (task_info == NULL) {
        return -1; 
    }

    uid_t cur_uid = ::getuid();
    if (cur_uid == 0) {
        // only do in root 
        uid_t user_uid;
        gid_t user_gid;
        if (!user::GetUidAndGid(FLAGS_agent_default_user, 
                    &user_uid, &user_gid)) {
            LOG(WARNING, "task %s user %s not exists", 
                    task_info->task_id.c_str(),
                    FLAGS_agent_default_user.c_str()); 
            return -1;
        }
        if (user_uid != cur_uid 
                && !file::Chown(task_info->task_workspace, 
                    user_uid, user_gid)) {
            LOG(WARNING, "task %s chown %s to user %s failed",
                    task_info->task_id.c_str(),
                    task_info->task_workspace.c_str(),
                    FLAGS_agent_default_user.c_str());    
            return -1;
        }
    }
    
    task_info->stage = kTaskStageRUNNING;
    task_info->status.set_state(kTaskRunning);
    SetupRunProcessKey(task_info);
    // send rpc to initd to execute main process
    ExecuteRequest initd_request; 
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->main_process.key());
    initd_request.set_commands(task_info->desc.start_command());
    if (FLAGS_agent_namespace_isolation_switch) {
        initd_request.set_chroot_path(task_info->task_chroot_path); 
        std::string* chroot_path = initd_request.add_envs();
        chroot_path->append("CHROOT_PATH=");
        chroot_path->append(task_info->task_chroot_path);
    }
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->cgroup_path);
    std::map<std::string, std::string>::iterator it = task_info->cgroups.begin();
    for (;it != task_info->cgroups.end(); ++it) {
        CgroupPath* cpath = initd_request.add_cgroups();
        cpath->set_subsystem(it->first);
        cpath->set_cgroup_path(it->second);
    }
    initd_request.set_user(FLAGS_agent_default_user);
    std::string* pod_id = initd_request.add_envs();
    pod_id->append("POD_ID=");
    pod_id->append(task_info->pod_id);
    std::string* task_id = initd_request.add_envs();
    task_id->append("TASK_ID=");
    task_id->append(task_info->task_id);
    
    if (task_info->initd_stub == NULL 
            && !rpc_client_->GetStub(task_info->initd_endpoint, 
                                     &(task_info->initd_stub))) {
        LOG(WARNING, "get stub failed"); 
        return -1;
    }

    bool ret = rpc_client_->SendRequest(task_info->initd_stub,
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
    LOG(INFO, "start command [%s] execute success for %s in dir %s",
            task_info->desc.start_command().c_str(),
            task_info->task_id.c_str(),
            task_info->task_workspace.c_str());
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
    tasks_mutex_.AssertHeld();
    if (task_info == NULL) {
        return -1; 
    }
    std::string stop_command = task_info->desc.stop_command();
    task_info->stage = kTaskStageSTOPPING;
    SetupTerminateProcessKey(task_info);
    // send rpc to initd to execute stop process
    ExecuteRequest initd_request; 
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->stop_process.key());
    initd_request.set_commands(stop_command);
    if (FLAGS_agent_namespace_isolation_switch) {
        initd_request.set_chroot_path(task_info->task_chroot_path); 
        std::string* chroot_path = initd_request.add_envs();
        chroot_path->append("CHROOT_PATH=");
        chroot_path->append(task_info->task_chroot_path);
    }
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->cgroup_path);
    initd_request.set_user(FLAGS_agent_default_user);

    if (task_info->initd_stub == NULL 
            && !rpc_client_->GetStub(task_info->initd_endpoint, 
                                     &(task_info->initd_stub))) {
        LOG(WARNING, "get stub failed"); 
        return -1;
    }

    bool ret = rpc_client_->SendRequest(task_info->initd_stub,
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

int TaskManager::CleanProcess(TaskInfo* task_info) {
    tasks_mutex_.AssertHeld();
    if (task_info->deploy_process.has_status() 
            && task_info->deploy_process.status() 
                                == kProcessRunning) {
        // TODO query first?
        ::killpg(task_info->deploy_process.pid(), SIGKILL); 
    }  
    if (task_info->main_process.has_status() 
            && task_info->main_process.status()
                                == kProcessRunning) {
        ::killpg(task_info->main_process.pid(), SIGKILL); 
    }
    if (task_info->stop_process.has_status() 
            && task_info->stop_process.status()
                                == kProcessRunning) {
        ::killpg(task_info->stop_process.pid(), SIGKILL); 
    }
    return 0;
}

int TaskManager::CleanTask(TaskInfo* task_info) {
    tasks_mutex_.AssertHeld();
    if (CleanProcess(task_info) != 0) {
        LOG(WARNING, "task %s clean process failed",
                task_info->task_id.c_str()); 
        task_info->status.set_state(kTaskError);
        return -1;
    }
    if (CleanWorkspace(task_info) != 0) {
        LOG(WARNING, "task %s clean workspace failed",
                task_info->task_id.c_str());
        task_info->status.set_state(kTaskError);
        return -1;
    }
    if (CleanResourceCollector(task_info) != 0) {
        LOG(WARNING, "task  %s clean resource collector failed",
                task_info->task_id.c_str()); 
        task_info->status.set_state(kTaskError);
        return -1;
    }
    if (CleanCgroupEnv(task_info) != 0) {
        LOG(WARNING, "task %s clean cgroup failed",
                task_info->task_id.c_str()); 
        task_info->status.set_state(kTaskError);
        return -1;
    }
    if (!task_info->desc.has_cpu_isolation_type()
        ||  task_info->desc.cpu_isolation_type() == kCpuIsolationHard) {
        int32_t old_hardlimit_cores = hardlimit_cores_;
        hardlimit_cores_ -= task_info->desc.requirement().millicores();
        bool ok = HandleHardlimitChange(hardlimit_cores_);
        if (!ok) {
            LOG(WARNING, "fail move cpu quota from hard limit to sofelimit , the hardlimit is %d", hardlimit_cores_); 
            hardlimit_cores_ = old_hardlimit_cores;
        }
    }
     
    LOG(INFO, "task %s clean success", task_info->task_id.c_str());
    return 0;
}

int TaskManager::DeployTask(TaskInfo* task_info) {
    tasks_mutex_.AssertHeld();
    if (task_info == NULL) {
        return -1; 
    }
    std::string deploy_command;
    task_info->stage = kTaskStageDEPLOYING;
    task_info->status.set_state(kTaskDeploy);
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
    task_info->stage = kTaskStageDEPLOYING;
    SetupDeployProcessKey(task_info);
    task_info->status.set_state(kTaskDeploy);
    // send rpc to initd to execute deploy process; 
    ExecuteRequest initd_request;      
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->deploy_process.key());
    initd_request.set_commands(deploy_command);
    if (FLAGS_agent_namespace_isolation_switch) {
        initd_request.set_chroot_path(task_info->task_chroot_path); 
        std::string* chroot_path = initd_request.add_envs();
        chroot_path->append("CHROOT_PATH=");
        chroot_path->append(task_info->task_chroot_path);
    }
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->cgroup_path);

    if (task_info->initd_stub == NULL 
            && !rpc_client_->GetStub(task_info->initd_endpoint, 
                                     &(task_info->initd_stub))) {
        LOG(WARNING, "get stub failed"); 
        return -1;
    }
    bool ret = rpc_client_->SendRequest(
                    task_info->initd_stub, 
                    &Initd_Stub::Execute,
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
    LOG(INFO, "deploy command [%s] execute success for %s in dir %s",
            deploy_command.c_str(),
            task_info->task_id.c_str(),
            task_info->task_workspace.c_str());
    return 0;
}

int TaskManager::QueryProcessInfo(Initd_Stub* initd_stub, 
                                  ProcessInfo* info) {
    tasks_mutex_.AssertHeld();
    if (info == NULL) {
        return -1; 
    }

    if (!info->has_key()) {
        return -1; 
    }
    GetProcessStatusRequest initd_request; 
    GetProcessStatusResponse initd_response;
    initd_request.set_key(info->key());

    bool ret = rpc_client_->SendRequest(initd_stub,
                                        &Initd_Stub::GetProcessStatus,
                                        &initd_request,
                                        &initd_response,
                                        5, 3);
    if (!ret) {
        LOG(WARNING, "query key %s rpc failed",
                info->key().c_str()); 
        return -1;
    } else if (initd_response.has_status() 
            && initd_response.status() != kOk) {
        LOG(WARNING, "query key %s failed %s",
                info->key().c_str(),
                Status_Name(initd_response.status()).c_str()); 
        return -1;
    }

    info->CopyFrom(initd_response.process());
    return 0;
}

int TaskManager::PrepareWorkspace(TaskInfo* task) {
    tasks_mutex_.AssertHeld();
    std::string workspace_root = FLAGS_agent_work_dir;
    workspace_root.append("/");
    workspace_root.append(task->pod_id);
    if (!file::Mkdir(workspace_root)) {
        LOG(WARNING, "mkdir workspace root failed"); 
        return -1;
    }

    std::string task_workspace(workspace_root);
    task_workspace.append("/");
    task_workspace.append(task->task_id);
    if (!file::MkdirRecur(task_workspace)) {
        LOG(WARNING, "mkdir task workspace failed");
        return -1;
    }

    task->task_workspace = task_workspace;
    task->task_chroot_path = workspace_root;
    LOG(INFO, "task %s workspace %s",
            task->task_id.c_str(), task->task_workspace.c_str());
    return 0;
}

int TaskManager::CleanWorkspace(TaskInfo* /*task*/) {
    tasks_mutex_.AssertHeld();
    return 0;
}

int TaskManager::TerminateProcessCheck(TaskInfo* task_info) {
    tasks_mutex_.AssertHeld();
    // check stop process       
    if (QueryProcessInfo(task_info->initd_stub,
                &(task_info->stop_process)) != 0) {
        LOG(WARNING, "task %s query stop process failed",
                task_info->task_id.c_str()); 
        task_info->status.set_state(kTaskError);
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
    tasks_mutex_.AssertHeld();
    // 1. query deploy process status 
    if (QueryProcessInfo(task_info->initd_stub, 
                &(task_info->deploy_process)) != 0) {
        LOG(WARNING, "task %s check deploy state failed",
                task_info->task_id.c_str());
        task_info->status.set_state(kTaskError);
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
        task_info->status.set_state(kTaskError);
        return -1;
    }
    return 1;     
}

int TaskManager::InitdProcessCheck(TaskInfo* task_info) {
    tasks_mutex_.AssertHeld();
    const int MAX_INITD_CHECK_TIME = 10;
    if (task_info == NULL) {
        return -1; 
    }
    std::string initd_endpoint = task_info->initd_endpoint; 
    // only use to check rpc connected
    GetProcessStatusRequest initd_request; 
    GetProcessStatusResponse initd_response;
    if (task_info->initd_stub == NULL 
            && !rpc_client_->GetStub(task_info->initd_endpoint, 
                                     &(task_info->initd_stub))) {
        LOG(WARNING, "get stub failed"); 
        return -1;
    }

    bool ret = rpc_client_->SendRequest(task_info->initd_stub,
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
    tasks_mutex_.AssertHeld();
    // 1. query main_process status
    if (QueryProcessInfo(task_info->initd_stub, 
                &(task_info->main_process)) != 0) {
        LOG(WARNING, "task %s check run state failed",
                task_info->task_id.c_str());
        task_info->status.set_state(kTaskError);
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
        task_info->status.set_state(kTaskError);
        return -1;
    }
    LOG(INFO, "task %s main process exit 0",
            task_info->task_id.c_str());
    task_info->status.set_state(kTaskTerminate); 
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
    SetResourceUsage(task_info);
    // switch task stage
    if (task_info->stage == kTaskStagePENDING 
            && task_info->status.state() != kTaskError) {
        int chk_res = InitdProcessCheck(task_info);
        if (chk_res == 1) {
            if (task_info->desc.has_binary() 
                    && !task_info->desc.binary().empty()) {
                if (DeployTask(task_info) != 0)  {
                    LOG(WARNING, "task %s deploy failed",
                        task_info->task_id.c_str());
                    task_info->status.set_state(kTaskError); 
                }
            // if no binary, run directly
            } else if (RunTask(task_info) != 0) {
                LOG(WARNING, "task %s run failed",
                        task_info->task_id.c_str());  
                task_info->status.set_state(kTaskError); 
            }
        } else if (chk_res == -1) {
            LOG(WARNING, "task %s check initd failed",
                    task_info->task_id.c_str()); 
            task_info->status.set_state(kTaskError);
        }
    } else if (task_info->stage == kTaskStageDEPLOYING
            && task_info->status.state() != kTaskError) {
        int chk_res = DeployProcessCheck(task_info);
        // deploy success and run
        if (chk_res == 1 && RunTask(task_info) != 0) {
            LOG(WARNING, "task %s run failed",
                    task_info->task_id.c_str()); 
            task_info->status.set_state(kTaskError);
        } else if (chk_res == -1) {
            LOG(WARNING, "task %s deploy failed",
                    task_info->task_id.c_str()); 
            task_info->status.set_state(kTaskError);
        }
    } else if (task_info->stage == kTaskStageRUNNING
            && task_info->status.state() != kTaskError) {
        int chk_res = RunProcessCheck(task_info);
        if (chk_res == -1 && 
                task_info->fail_retry_times 
                    < task_info->max_retry_times) {
            task_info->fail_retry_times++;
            RunTask(task_info);         
        }
    } else if (task_info->stage == kTaskStageSTOPPING) {
        int chk_res = TerminateProcessCheck(task_info);
        if (chk_res != 0) {
            if (0 == CleanTask(task_info)) {
                if (task_info->initd_stub != NULL) {
                    delete task_info->initd_stub; 
                }
                delete task_info;
                task_info = NULL;
                tasks_.erase(it);     
            } else {
                LOG(WARNING, "clean task %s failed",
                        task_info->task_id.c_str());    
                task_info->status.set_state(kTaskError);
            }
        }
    }
    if (task_info != NULL) {
        LOG(DEBUG, "task %s pod %s state %s", 
                task_info->task_id.c_str(),
                task_info->pod_id.c_str(),
                TaskState_Name(task_info->status.state()).c_str());
    }
    background_thread_.DelayTask(
                    FLAGS_agent_detect_interval, 
                    boost::bind(
                        &TaskManager::DelayCheckTaskStageChange,
                        this, task_id));
    return; 
}

bool TaskManager::HandleInitTaskComCgroup(std::string& subsystem, TaskInfo* task) {
    tasks_mutex_.AssertHeld();
    if (task == NULL) {
        return false;
    }
    if (hierarchies_.find(subsystem) == hierarchies_.end()) {
        LOG(WARNING, "%s subsystem is disabled", subsystem.c_str());
        return true;
    }
    std::string path = hierarchies_[subsystem] + "/" + FLAGS_agent_global_cgroup_path + "/" 
        + task->task_id;
    if(!file::Mkdir(path)) {
        LOG(WARNING, "create dir %s failed for %s", path.c_str(), task->task_id.c_str());
        return false;
    }
    LOG(INFO, "create cgroup %s , %s for task %s", subsystem.c_str(), path.c_str(),
            task->task_id.c_str());
    task->cgroups[subsystem] = path;
    return true;
}
bool TaskManager::HandleInitTaskTcpCgroup(std::string& subsystem, TaskInfo* task) {
    tasks_mutex_.AssertHeld();
    if (task == NULL) {
        return false;
    }
    if (hierarchies_.find("tcp_throt") == hierarchies_.end()) {
        LOG(WARNING, "tcp_throt subsystem is disabled");
        return true;
    }
    std::string tcp_path = hierarchies_["tcp_throt"] + "/" + FLAGS_agent_global_cgroup_path;
    LOG(INFO, "create cgroup %s, %s for task %s", subsystem.c_str(), tcp_path.c_str(),
            task->task_id.c_str());
    task->cgroups[subsystem] = tcp_path;
    return true;
}
bool TaskManager::HandleInitTaskMemCgroup(std::string& subsystem , TaskInfo* task) {
    tasks_mutex_.AssertHeld();
    if (task == NULL) {
        return false; 
    }
    LOG(INFO, "create cgroup %s for task %s",subsystem.c_str(), task->task_id.c_str());
    if (hierarchies_.find("memory") == hierarchies_.end()) {
        LOG(WARNING, "memory subsystem is disabled");
        return true;
    }
    std::string mem_path = hierarchies_["memory"] + "/" + FLAGS_agent_global_cgroup_path + "/" 
        + task->task_id;
    if (!file::Mkdir(mem_path)) {
        LOG(WARNING, "create dir %s failed for %s", mem_path.c_str(), task->task_id.c_str());
        return false;
    }
    task->cgroups["memory"] = mem_path;
    int64_t memory_limit = task->desc.requirement().memory();
    if (task->desc.has_mem_isolation_type() && 
            task->desc.mem_isolation_type() == 
                    kMemIsolationLimit) {
        int ret = cgroups::Write(mem_path,
                                 "memory.soft_limit_in_bytes",
                                 boost::lexical_cast<std::string>(memory_limit));    
        if (ret != 0) {
            LOG(WARNING, "set memory soft limit %ld failed for %s",
                    memory_limit, mem_path.c_str()); 
            return false;
        }
    } else {
        if (cgroups::Write(mem_path,
                    "memory.limit_in_bytes",
                    boost::lexical_cast<std::string>(memory_limit)
                    ) != 0) {
            LOG(WARNING, "set memory limit %ld failed for %s",
                    memory_limit, mem_path.c_str()); 
            return false;
        }
    }
    const int GROUP_KILL_MODE = 1;
    if (file::IsExists(mem_path + "/memory.kill_mode") 
            && cgroups::Write(mem_path,
                "memory.kill_mode", 
                boost::lexical_cast<std::string>(GROUP_KILL_MODE)
                ) != 0) {
        LOG(WARNING, "set memory kill mode failed for %s", 
                mem_path.c_str()); 
        return false;
    }
    return true; 
}

bool TaskManager::HandleInitTaskCpuCgroup(std::string& subsystem, TaskInfo* task) {
    tasks_mutex_.AssertHeld();
    if (task == NULL) {
        return false;
    }
    LOG(INFO, "create cgroup %s for task %s",subsystem.c_str(), task->task_id.c_str());
    if (hierarchies_.find("cpu") == hierarchies_.end()) {
        LOG(WARNING, "cpu subsystem is disabled");
        return true;
    }
    if (task->desc.has_cpu_isolation_type()
        && task->desc.cpu_isolation_type() == kCpuIsolationSoft) {
        LOG(INFO, "create soft limit task %s", task->task_id.c_str());
        std::string cpu_path = hierarchies_["cpu"] + "/" + FLAGS_agent_global_softlimit_path 
            + "/" + task->task_id;
        if (!file::Mkdir(cpu_path)) {
            LOG(WARNING, "create dir %s failed for %s", cpu_path.c_str(), task->task_id.c_str());
            return false;
        } 
        task->cgroups[subsystem] = cpu_path;
        int32_t cpu_limit = task->desc.requirement().millicores();
        if (cgroups::Write(cpu_path,
                           "cpu.shares",
                           boost::lexical_cast<std::string>(cpu_limit)) != 0) {
            LOG(WARNING, "set cpu shares %d failed for %s",
                    cpu_limit, cpu_path.c_str()); 
            return false;
        }
        if (cgroups::Write(cpu_path,
                           "cpu.cfs_quota_us",
                           boost::lexical_cast<std::string>(-1)
                           ) != 0) {
            LOG(WARNING, "disable cpu limit failed for %s", cpu_path.c_str()); 
            return false;
        } 
    }else {
        LOG(INFO, "create hard limit task %s", task->task_id.c_str());
        std::string cpu_path = hierarchies_["cpu"] + "/" + FLAGS_agent_global_hardlimit_path + "/" + task->task_id;
        if (!file::Mkdir(cpu_path)) {
            LOG(WARNING, "create dir %s failed for %s", cpu_path.c_str(), task->task_id.c_str());
            return false;
        }
        task->cgroups[subsystem] = cpu_path;
        int32_t old_hardlimit_cores = hardlimit_cores_;
        hardlimit_cores_ += task->desc.requirement().millicores();
        int32_t limit_cores = task->desc.requirement().millicores() * (CPU_CFS_PERIOD / 1000);
        if (cgroups::Write(cpu_path,
                           "cpu.cfs_quota_us",
                           boost::lexical_cast<std::string>(limit_cores)
                           ) != 0) {
            LOG(WARNING, "set cpu limit failed for %s", cpu_path.c_str()); 
            return false;
        }
        bool ok = HandleHardlimitChange(hardlimit_cores_);
        if (!ok) {
            LOG(WARNING, "fail to adjust hardlimit to %d", hardlimit_cores_);
            hardlimit_cores_ = old_hardlimit_cores;
            return false;
        }
    }
    return true;
}

int TaskManager::PrepareCgroupEnv(TaskInfo* task) {
    if (task == NULL) {
        return -1; 
    }

    if (cgroup_funcs_.size() == 0) {
        return 0; 
    } 

    std::map<std::string, CgroupFunc>::iterator func_it = cgroup_funcs_.begin();
    for (; func_it != cgroup_funcs_.end(); ++func_it) { 
        bool ok = func_it->second(task);
        if (!ok) {
            return -1;
        }
        LOG(INFO, "create cgroup %s success",  task->task_id.c_str());
    } 
    return 0;    
}

int TaskManager::CleanCgroupEnv(TaskInfo* task) {
    if (task == NULL) {
        return -1; 
    }
    std::string freezer_path;
    if (task->cgroups.find("freezer") != task->cgroups.end()) {
        freezer_path = task->cgroups["freezer"];
    }
    std::map<std::string, std::string>::iterator it = task->cgroups.begin();

    for (;it != task->cgroups.end(); ++it) {
        if (it->first == "freezer") {
            continue; 
        }
        std::string cgroup_dir = it->second; 
        if (!file::IsExists(cgroup_dir)) {
            LOG(INFO, "%s not exists", cgroup_dir.c_str()); 
            continue;
        }

        if (!cgroups::FreezerSwitch(freezer_path, "FROZEN")) {
            LOG(WARNING, "%s frozen failed", freezer_path.c_str()); 
            return -1;
        }
        std::vector<int> pids;
        if (!cgroups::GetPidsFromCgroup(cgroup_dir, &pids)) {
            LOG(WARNING, "get pids from %s failed",
                    cgroup_dir.c_str());  
            return -1;
        }

        std::vector<int>::iterator pid_it = pids.begin();
        for (; pid_it != pids.end(); ++pid_it) {
            int pid = *pid_it;
            if (pid != 0) {
                ::kill(pid, SIGKILL); 
            }
        }
        if (!cgroups::FreezerSwitch(freezer_path, "THAWED")) {
            LOG(WARNING, "%s thawed failed", freezer_path.c_str()); 
            return -1;
        }

        if (::rmdir(cgroup_dir.c_str()) != 0
                && errno != ENOENT) {
            LOG(WARNING, "rmdir %s failed err[%d: %s]",
                    cgroup_dir.c_str(), errno, strerror(errno));
            return -1;
        }
    }
    if (::rmdir(freezer_path.c_str()) != 0
            && errno != ENOENT) {
        LOG(WARNING, "rmdir %sfailed err[%d: %s]",
                freezer_path.c_str(), 
                errno, strerror(errno)); 
        return -1;
    }
    return 0;
}


int TaskManager::PrepareResourceCollector(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }

    if (hierarchies_.size() == 0) {
        return 0; 
    } 

    if (task_info->resource_collector != NULL) {
        delete task_info->resource_collector; 
        task_info->resource_collector = NULL;
    }
    std::string cgroup_path = FLAGS_agent_global_cgroup_path + "/" 
                              + task_info->task_id;
    task_info->resource_collector = 
                    new CGroupResourceCollector(task_info->cgroups["memory"],
                                                task_info->cgroups["cpu"],
                                                task_info->cgroups["cpuacct"]);
    return 0; 
}

int TaskManager::CleanResourceCollector(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }
    if (task_info->resource_collector != NULL) {
        delete task_info->resource_collector; 
        task_info->resource_collector = NULL;
    }
    return 0;
}

void TaskManager::SetResourceUsage(TaskInfo* task_info) {
    if (task_info == NULL) {
        return ; 
    }
    
    if (task_info->resource_collector == NULL) {
        return ; 
    }

    task_info->resource_collector->CollectStatistics();
    double cpu_cores_used = 
        task_info->resource_collector->GetCpuCoresUsage();
    long memory_used = 
        task_info->resource_collector->GetMemoryUsage();
    task_info->status.mutable_resource_used()->set_millicores(
            static_cast<int32_t>(cpu_cores_used * 1000));
    task_info->status.mutable_resource_used()->set_memory(
            memory_used);
    return ; 
}

int TaskManager::InitTcpthrotEnv() {
    std::string hierarchy = FLAGS_gce_cgroup_root + "/tcp_throt";
    if (cgroups::Write(hierarchy,
                       FLAGS_agent_global_cgroup_path,
                       "tcp_throt.send_bps_limit",
                       boost::lexical_cast<std::string>(0)) != 0) {
        LOG(WARNING, "set send bps limit %d failed for %s",
                0, FLAGS_agent_global_cgroup_path.c_str());
        return -1;
    }
    if (cgroups::Write(hierarchy,
                       FLAGS_agent_global_cgroup_path,
                       "tcp_throt.recv_bps_limit",
                       boost::lexical_cast<std::string>(0)) != 0) {
        LOG(WARNING, "set recv bps limit %d failed for %s",
                0, FLAGS_agent_global_cgroup_path.c_str());
        return -1;
    }
    if (cgroups::Write(hierarchy,
                       FLAGS_agent_global_cgroup_path,
                       "tcp_throt.send_bulk_byte_limit",
                       boost::lexical_cast<std::string>(0)) != 0) {
        LOG(WARNING, "set recv bps bulk %d failed for %s",
                0, FLAGS_agent_global_cgroup_path.c_str());
        return -1;
    }
    if (cgroups::Write(hierarchy,
                       FLAGS_agent_global_cgroup_path,
                       "tcp_throt.recv_bulk_byte_limit",
                       boost::lexical_cast<std::string>(0)) != 0) {
        LOG(WARNING, "set recv bps bulk %d failed for %s",
                0, FLAGS_agent_global_cgroup_path.c_str());
        return -1;
    }
    if (cgroups::Write(hierarchy,
                       FLAGS_agent_global_cgroup_path,
                       "tcp_throt.send_bps_quota",
                       boost::lexical_cast<std::string>(FLAGS_send_bps_quota)) != 0) {
        LOG(WARNING, "set send bps quota %d failed for %s",
                FLAGS_send_bps_quota,  FLAGS_agent_global_cgroup_path.c_str());
        return -1;
    }
    if (cgroups::Write(hierarchy,
                       FLAGS_agent_global_cgroup_path,
                       "tcp_throt.recv_bps_quota",
                       boost::lexical_cast<std::string>(FLAGS_recv_bps_quota)) != 0) {
        LOG(WARNING, "set recv bps quota %d failed for %s",
                FLAGS_recv_bps_quota, FLAGS_agent_global_cgroup_path.c_str());
        return -1;
    }
    if (cgroups::Write(hierarchy,
                       FLAGS_agent_global_cgroup_path,
                       "tcp_throt.send_bulk_byte_quota",
                       boost::lexical_cast<std::string>(FLAGS_recv_bps_quota)) != 0) {
        LOG(WARNING, "set send bps bulk %d failed for %s",
                FLAGS_send_bps_quota, FLAGS_agent_global_cgroup_path.c_str());
        return -1;
    }
    if (cgroups::Write(hierarchy,
                       FLAGS_agent_global_cgroup_path,
                       "tcp_throt.recv_bulk_byte_quota",
                       boost::lexical_cast<std::string>(FLAGS_send_bps_quota)) != 0) {
        LOG(WARNING, "set recv bps bulk %d failed for %s",
                FLAGS_recv_bps_quota, FLAGS_agent_global_cgroup_path.c_str());
        return -1;
    }
    return 0;
}
}   // ending namespace galaxy
}   // ending namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
