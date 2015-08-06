#include "task_manager.h"
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
#include "gce/utils.h"
#include "logging.h"
#include "gce/cgroups.h"

DECLARE_string(gce_cgroup_root);
DECLARE_string(gce_support_subsystems);
DECLARE_int64(gce_initd_zombie_check_interval);
DECLARE_string(gce_work_dir);
DECLARE_string(gce_initd_bin);

namespace baidu {
namespace galaxy {

TaskManager::TaskManager() : 
    tasks_mutex_(),
    tasks_(),
    background_thread_(1), 
    cgroup_root_(FLAGS_gce_cgroup_root),
    hierarchies_() {
    // init resource collector engine
}

TaskManager::~TaskManager() {
}

int TaskManager::Init() {
    // TODO create susystem deal
    return 0;
}

int TaskManager::CreateTasks(const std::string& podid, const PodDescriptor& pod) {
    MutexLock scope_lock(&tasks_mutex_);
    for (int i = 0; i < pod.tasks_size(); i++) {
        const TaskDescriptor& task = pod.tasks(i);  
        TaskInfo* task_info = new TaskInfo();
        task_info->task_id = GenerateTaskId(podid);
        task_info->pod_id = podid;
        task_info->desc.CopyFrom(task);
        task_info->status.set_state(kPodPending);
        task_info->millicores = task.requirement().millicores();
        tasks_[task_info->task_id] = task_info;
        // 1. prepare workspace 
        if (PrepareWorkspace(task_info) != 0) {
            LOG(WARNING, "prepare task %s workspace failed", 
                    task_info->task_id.c_str()); 
            return -1;
        }
        // 2. prepare cgroup 
        if (PrepareCgroupEnv(task_info) != 0) {
            LOG(WARNING, "prepare task %s cgroup faield", 
                    task_info->task_id.c_str()); 
            return -1;
        }
        // 3. prepare mount path
        if (PrepareVolumeEnv(task_info) != 0) {
            LOG(WARNING, "prepare task %s volume env failed",
                    task_info->task_id.c_str()); 
            return -1;
        }
        LOG(INFO, "prepare task %s success", 
                        task_info->task_id.c_str());
    }
    return 0;
}

int TaskManager::DeleteTasks() {
    MutexLock scope_lock(&tasks_mutex_);
    std::map<std::string, TaskInfo*>::iterator it = 
        tasks_.begin();
    std::string pod_id;
    for (; it != tasks_.end(); ++it) {
        pod_id = it->second->pod_id;
        // 4. Terminate Task 
        if (TerminateTask(it->second) != 0) {
            LOG(WARNING, "terminate task %s failed", 
                    it->first.c_str()); 
            return -1;
        }
        // 3. clean mount path
        if (CleanVolumeEnv(it->second) != 0) {
            LOG(WARNING, "clean task %s volume failed",
                    it->first.c_str()); 
            return -1;
        }
        // 2. clean cgroup
        if (CleanCgroupEnv(it->second) != 0) {
            LOG(WARNING, "clean task %s cgroup failed", 
                    it->first.c_str()); 
            return -1;
        }
        // 1. clean workspace
        if (CleanWorkspace(it->second) != 0) {
            LOG(WARNING, "clean task %s workspace failed",
                    it->first.c_str()); 
            return -1;
        }
        delete it->second;
        it->second = NULL;
    }
    tasks_.clear();  
    return 0;
}

int TaskManager::UpdateCpuLimit(const std::string& task_id, 
                                const uint32_t millicores) {
    std::string cpu_hierarchy = FLAGS_gce_cgroup_root + "/cpu";
    int32_t cpu_share = millicores * 512;
    MutexLock scope_lock(&tasks_mutex_);
    std::map<std::string, TaskInfo*>::iterator it = 
        tasks_.find(task_id);    
    if (it == tasks_.end()) {
        return -1; 
    }
    TaskInfo* task_info = it->second;
    std::string cgroup_name = task_info->task_id;
    if (cgroups::Write(cpu_hierarchy, 
                cgroup_name, 
                "cpu.share", 
                boost::lexical_cast<std::string>(cpu_share)) != 0) {
        LOG(WARNING, "update %s cpu limit failed",
                cgroup_name.c_str()); 
        return -1;
    }
    return 0;
}

void TaskManager::LoopCheckTaskStatus() {
    // TODO
    return;
}

int TaskManager::DeployTask(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }
    std::string deploy_command;
    if (task_info->desc.source_type() == kSourceTypeBinary) {
        // TODO write binary directly
        std::string tar_packet = task_info->task_workspace;
        tar_packet.append("/tmp.tar.gz");
        if (file::IsExists(tar_packet)) {
            file::Remove(tar_packet);     
        }
        const int WRITE_FLAG = O_CREAT | O_WRONLY;
        const int WRITE_MODE = S_IRUSR | S_IWUSR 
            | S_IRGRP | S_IRWXO;
        int fd = ::open(tar_packet.c_str(), 
                WRITE_FLAG, WRITE_MODE);
        if (fd == -1) {
            LOG(WARNING, "open download "
                    "%s file failed err[%d: %s]",
                    tar_packet.c_str(),
                    errno,
                    strerror(errno));     
            return -1;
        }
        std::string binary = task_info->desc.binary();
        int write_len = ::write(fd, 
                binary.c_str(), binary.size());
        if (write_len == -1) {
            LOG(WARNING, "write download "
                    "%s file failed err[%d: %s]",
                    tar_packet.c_str(),
                    errno,
                    strerror(errno)); 
            ::close(fd);
            return -1;
        }
        ::close(fd);
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
    // send rpc to initd to execute deploy process; 
    ExecuteRequest initd_request;      
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->task_id);
    initd_request.set_commands(deploy_command);
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->task_id);
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
    if (ret != 0) {
        LOG(WARNING, "deploy command "
                "%s execute rpc failed for %s",
                deploy_command.c_str(),
                task_info->task_id.c_str());
        return -1;
    } else if (initd_response.has_status() 
            && initd_response.status() != kOk) {
        LOG(WARNING, "deploy command "
                "%s execute failed %s for %s",
                deploy_command.c_str(),
                Status_Name(initd_response.status()).c_str(),
                task_info->task_id.c_str()); 
        return -1;
    }
    LOG(INFO, "deploy command %s execute success for %s",
            deploy_command.c_str(),
            task_info->task_id.c_str());
    return 0;
}

int TaskManager::RunTask(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }
    task_info->stage = kStageRUNNING;

    // send rpc to initd to execute main process
    ExecuteRequest initd_request; 
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->task_id);
    initd_request.set_commands(task_info->desc.start_command());
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->task_id);
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
        LOG(WARNING, "start command %s rpc failed for %s",
                task_info->desc.start_command().c_str(),
                task_info->task_id.c_str()); 
        return -1;
    } else if (initd_response.has_status()
            && initd_response.status() != kOk) {
        LOG(WARNING, "start command %s failed %s for %s",
                task_info->desc.start_command().c_str(),
                Status_Name(initd_response.status()).c_str(),
                task_info->task_id.c_str()); 
        return -1;
    }
    LOG(INFO, "start command %s execute success for %s",
            task_info->desc.start_command().c_str(),
            task_info->task_id.c_str());
    return 0;
}

int TaskManager::TerminateTask(TaskInfo* task_info) {
    if (task_info == NULL) {
        return -1; 
    }
    std::string stop_command = task_info->desc.stop_command();
    task_info->stage = kStageSTOPPING;
    // send rpc to initd to execute stop process
    ExecuteRequest initd_request; 
    ExecuteResponse initd_response;
    initd_request.set_key(task_info->task_id);
    initd_request.set_commands(stop_command);
    initd_request.set_path(task_info->task_workspace);
    initd_request.set_cgroup_path(task_info->task_id);
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
        LOG(WARNING, "stop command %s rpc failed for %s",
                stop_command.c_str(),
                task_info->task_id.c_str()); 
        return -1;
    } else if (initd_response.has_status()
                && initd_response.status() != kOk) {
        LOG(WARNING, "stop command %s failed %s for %s",
                stop_command.c_str(),
                Status_Name(initd_response.status()).c_str(),
                task_info->task_id.c_str()); 
        return -1;
    }
    LOG(INFO, "stop command %s execute success for %s",
            stop_command.c_str(),
            task_info->task_id.c_str());
    return 0;
}

int TaskManager::PrepareWorkspace(TaskInfo* task) {
    std::string workspace_root = FLAGS_gce_work_dir;
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
    return 0;
}

int TaskManager::PrepareCgroupEnv(const TaskInfo* task) {
    if (task == NULL) {
        return -1; 
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

int TaskManager::PrepareVolumeEnv(const TaskInfo* task) {
    return 0;
}

int TaskManager::CleanWorkspace(const TaskInfo* task) {
    if (file::IsExists(task->task_workspace)
            && !file::Remove(task->task_workspace)) {
        LOG(WARNING, "Remove task %s workspace failed",
                task->task_id.c_str());
        return -1;
    }
    std::string workspace_root = FLAGS_gce_work_dir;
    workspace_root.append("/");
    workspace_root.append(task->pod_id);
    if (file::IsExists(workspace_root)
            && !file::Remove(workspace_root)) {
        return -1;     
    }
    return 0;
}

int TaskManager::CleanCgroupEnv(const TaskInfo* task) {
    if (task == NULL) {
        return -1; 
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

int TaskManager::CleanVolumeEnv(const TaskInfo* task) {
    return 0;
}

int TaskManager::CleanResourceCollector(const TaskInfo* task) {
    return 0;
}

int TaskManager::PrepareResourceCollector(const TaskInfo* task) {
    // TODO 
    return 0;
}

int TaskManager::QueryProcessInfo(const std::string& key, 
                                  const std::string& initd_endpoint, 
                                  ProcessInfo* info) {
    if (info == NULL) {
        return -1; 
    }

    GetProcessStatusRequest initd_request; 
    GetProcessStatusResponse initd_response;
    initd_request.set_key(key);
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
                key.c_str(), initd_endpoint.c_str()); 
        return -1;
    } else if (initd_response.has_status() 
            && initd_response.status() != kOk) {
        LOG(WARNING, "query key %s to %s failed %s",
                key.c_str(), initd_endpoint.c_str(),
                Status_Name(initd_response.status()).c_str()); 
        return -1;
    }

    info->CopyFrom(initd_response.process());
    return 0;
}

std::string TaskManager::GenerateTaskId(const std::string& podid) {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();    
    std::stringstream sm_uuid;
    sm_uuid << uuid;
    std::string str_uuid = podid;
    str_uuid.append("_");
    str_uuid.append(sm_uuid.str());
    return str_uuid;
}

} // ending namespace galaxy
} // ending namespace baidu


