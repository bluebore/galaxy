// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/pod_manager.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <string>
#include <vector>

#include <gflags/gflags.h>

#include "boost/lexical_cast.hpp"
#include "logging.h"
#include "agent/utils.h"

// for kernel 2.6.32 and glibc not define some clone flag
#ifndef CLONE_NEWPID        
#define CLONE_NEWPID 0x20000000
#endif

#ifndef CLONE_NEWUTS
#define CLONE_NEWUTS 0x04000000
#endif 

DECLARE_string(agent_initd_bin);
DECLARE_string(agent_work_dir);
DECLARE_string(agent_gc_dir);
DECLARE_int64(agent_gc_timeout);
DECLARE_int32(agent_initd_port_begin);
DECLARE_int32(agent_initd_port_end);
DECLARE_bool(agent_namespace_isolation_switch);
DECLARE_string(gce_cgroup_root);
DECLARE_string(gce_support_subsystems);
DECLARE_string(flagfile);

namespace baidu {
namespace galaxy {


struct LanuchInitdContext {
    int stdout_fd;
    int stderr_fd;
    std::string start_command;
    std::string path;
    std::vector<int> fds;
};

static int LanuchInitdMain(void *arg) {
    LanuchInitdContext* context = 
        reinterpret_cast<LanuchInitdContext*>(arg);
    if (context == NULL) {
        return -1; 
    }

    process::PrepareChildProcessEnvStep1(::getpid(), 
                                         context->path.c_str());  
    process::PrepareChildProcessEnvStep2(-1,
                                         context->stdout_fd, 
                                         context->stderr_fd, 
                                         context->fds);
    char* argv[] = {
        const_cast<char*>("sh"),
        const_cast<char*>("-c"),
        const_cast<char*>(context->start_command.c_str()),
        NULL};
    ::execv("/bin/sh", argv);
    assert(0);
    return 0;
}

PodManager::PodManager() : 
    pods_(), 
    task_manager_(NULL),
    initd_free_ports_(),
    garbage_collect_thread_(1) {
}

PodManager::~PodManager() {
    garbage_collect_thread_.Stop(false);
    if (task_manager_ != NULL) {
        delete task_manager_;
        task_manager_ = NULL;
    }
}

int PodManager::Init() {
    int initd_port_begin = FLAGS_agent_initd_port_begin; 
    int initd_port_end = FLAGS_agent_initd_port_end;
    for (int i = initd_port_begin; i < initd_port_end; i++) {
        initd_free_ports_.PushBack(i); 
    }
    if (!file::Remove(FLAGS_agent_gc_dir)
            || !file::Mkdir(FLAGS_agent_gc_dir)) {
        LOG(WARNING, "init gc dir failed"); 
        return -1;
    }

    task_manager_ = new TaskManager();
    return task_manager_->Init();
}

int PodManager::LanuchInitdByFork(PodInfo* info) {
    if (info == NULL) {
        return -1;
    }

    // 1. collect agent fds
    std::vector<int> fd_vector;
    process::GetProcessOpenFds(::getpid(), &fd_vector);
    // 2. prepare std fds for child
    int stdout_fd = -1;
    int stderr_fd = -1;
    std::string path = FLAGS_agent_work_dir;
    path.append("/");
    path.append(info->pod_id);
    std::string command = FLAGS_agent_initd_bin;
    command.append(" --flagfile=");
    command.append(FLAGS_flagfile);
    command.append(" --gce_initd_port=");
    command.append(boost::lexical_cast<std::string>(info->initd_port));
    command.append(" --gce_initd_dump_file=");
    command.append(path + "/initd_checkpoint_file");
    file::Mkdir(path);
    info->pod_path = path;
    if (!process::PrepareStdFds(path, &stdout_fd, &stderr_fd)) {
        LOG(WARNING, "prepare std fds for pod %s failed",
                info->pod_id.c_str()); 
        if (stdout_fd != -1) {
            ::close(stdout_fd);
        }
        if (stderr_fd != -1) {
            ::close(stderr_fd); 
        }
        return -1;
    }

    pid_t child_pid = ::fork();
    if (child_pid == -1) {
        LOG(WARNING, "fork %s failed err[%d: %s]",
                info->pod_id.c_str(), errno, strerror(errno)); 
        return -1;
    } else if (child_pid == 0) {
        pid_t my_pid = ::getpid(); 
        process::PrepareChildProcessEnvStep1(my_pid,
                path.c_str());
        process::PrepareChildProcessEnvStep2(-1,
                                             stdout_fd, 
                                             stderr_fd, 
                                             fd_vector);
        char* argv[] = {
            const_cast<char*>("sh"),
            const_cast<char*>("-c"),
            const_cast<char*>(command.c_str()),
            NULL};
        ::execv("/bin/sh", argv);
        assert(0);
    } 
    ::close(stdout_fd);
    ::close(stderr_fd);
    info->initd_pid = child_pid;
    return 0;
}

int PodManager::LanuchInitd(PodInfo* info) {
    if (info == NULL) {
        return -1; 
    }
    int CLONE_FLAG = CLONE_NEWNS | CLONE_NEWPID 
                            | CLONE_NEWUTS;
    const int CLONE_STACK_SIZE = 1024 * 1024;
    static char CLONE_STACK[CLONE_STACK_SIZE];
    
    LanuchInitdContext context;
    context.stdout_fd = -1; 
    context.stderr_fd = -1;
    context.start_command = FLAGS_agent_initd_bin;
    context.start_command.append(" --flagfile=");
    context.start_command.append(FLAGS_flagfile);
    context.start_command.append(" --gce_initd_port=");
    context.start_command.append(boost::lexical_cast<std::string>(info->initd_port));
    if (!FLAGS_gce_cgroup_root.empty()) {
        context.start_command.append(" --gce_cgroup_root=");
        context.start_command.append(FLAGS_gce_cgroup_root);
    }
    if (!FLAGS_gce_support_subsystems.empty()) {
        context.start_command.append(" --gce_support_subsystems=");
        context.start_command.append(FLAGS_gce_support_subsystems);
    }
    context.path = FLAGS_agent_work_dir + "/" + info->pod_id;
    context.start_command.append(" --gce_initd_dump_file=");
    context.start_command.append(context.path + "/initd_checkpoint_file");
    if (!file::Mkdir(context.path)) {
        LOG(WARNING, "mkdir %s failed", context.path.c_str()); 
        return -1;
    }
    info->pod_path = context.path;

    if (!process::PrepareStdFds(context.path,
                                &context.stdout_fd,
                                &context.stderr_fd)) {
        LOG(WARNING, "prepare %s std file failed", 
                context.path.c_str()); 
        if (context.stdout_fd > 0) {
            ::close(context.stdout_fd); 
        }
        if (context.stderr_fd > 0) {
            ::close(context.stderr_fd); 
        }
        return -1;
    }
    
    process::GetProcessOpenFds(::getpid(), &context.fds);
    int child_pid = ::clone(&LanuchInitdMain, 
                            CLONE_STACK + CLONE_STACK_SIZE, 
                            CLONE_FLAG | SIGCHLD, 
                            &context);
    // clear initd fds
    ::close(context.stdout_fd);
    ::close(context.stderr_fd);
    if (child_pid == -1) {
        LOG(WARNING, "clone initd for %s failed err[%d: %s]",
                    info->pod_id.c_str(), errno, strerror(errno));      
        return -1;
    }
    info->initd_pid = child_pid;
    return 0;
}

int PodManager::CleanPodEnv(const PodInfo& pod_info) {
    std::string pod_workspace = FLAGS_agent_work_dir;
    pod_workspace.append("/");
    pod_workspace.append(pod_info.pod_id);
    if (!file::IsExists(pod_workspace)) {
        return 0; 
    }

    LOG(WARNING, "pod of job %s gc path %s", pod_info.job_name.c_str(),
            pod_info.pod_status.pod_gc_path().c_str());
    std::string new_workspace_dir = pod_info.pod_status.pod_gc_path();
    if (::access(new_workspace_dir.c_str(), F_OK) == 0) {
        LOG(WARNING, "path %s is already exists", new_workspace_dir.c_str()); 
        return -1;
    }

    int ret = ::rename(pod_workspace.c_str(), new_workspace_dir.c_str());
    if (ret == -1) {
        LOG(WARNING, "rename %s to %s failed err[%d: %s]",
                pod_workspace.c_str(), new_workspace_dir.c_str(), errno, strerror(errno)); 
        return -1;
    }
    garbage_collect_thread_.DelayTask(FLAGS_agent_gc_timeout, 
            boost::bind(&PodManager::DelayGarbageCollect, this, new_workspace_dir));
    return 0;
}

void PodManager::DelayGarbageCollect(const std::string& workspace) {
    if (file::IsExists(workspace) 
            && !file::Remove(workspace)) {
        LOG(WARNING, "remove pod workspace failed %s", workspace.c_str()); 
    }
    return;
}

int PodManager::CheckPod(const std::string& pod_id) {
    std::map<std::string, PodInfo>::iterator pod_it = 
        pods_.find(pod_id);
    if (pod_it == pods_.end()) {
        return -1; 
    }

    PodInfo& pod_info = pod_it->second;
    // all task delete by taskmanager, no need check
    if (pod_info.tasks.size() == 0) {
        // TODO check initd exits
        if (pod_info.initd_pid > 0) { 
            ::kill(pod_info.initd_pid, SIGKILL);
            int status = 0;
            pid_t pid = ::waitpid(pod_info.initd_pid, &status, WNOHANG); 
            if (pid == 0) {
                LOG(WARNING, "fail to kill %s of job %s initd",
                        pod_info.pod_id.c_str(),
                        pod_info.job_name.c_str());
                return 0;
            }
        }
        ReleasePortFromInitd(pod_info.initd_port);
        if (CleanPodEnv(pod_info) != 0) {
            LOG(WARNING, "fail to clean %s env", pod_info.pod_id.c_str()); 
            return 0;
        }
        LOG(INFO, "remove pod %s of job %s", pod_info.pod_id.c_str(),
                pod_info.job_name.c_str());
        pods_.erase(pod_it);
        return -1;
    }

    int32_t millicores = 0;
    int64_t memory_used = 0;
    int64_t read_bytes_ps = 0;
    int64_t write_bytes_ps = 0;
    int64_t syscr_ps = 0;
    int64_t syscw_ps = 0;
    std::map<std::string, TaskInfo>::iterator task_it = 
        pod_info.tasks.begin();
    TaskState pod_state = kTaskRunning; 
    std::vector<std::string> to_del_task;
    pod_info.pod_status.mutable_status()->Clear();
    for (; task_it != pod_info.tasks.end(); ++task_it) {
        if (task_manager_->QueryTask(&(task_it->second)) != 0) {
            to_del_task.push_back(task_it->first);
        } else {
            millicores += task_it->second.status.resource_used().millicores();
            memory_used += task_it->second.status.resource_used().memory();
            read_bytes_ps += task_it->second.status.resource_used().read_bytes_ps();
            write_bytes_ps += task_it->second.status.resource_used().write_bytes_ps();
            syscr_ps += task_it->second.status.resource_used().syscr_ps();
            syscw_ps += task_it->second.status.resource_used().syscw_ps();
            // TODO pod state 
            if (task_it->second.status.state() <= kTaskRunning
                    && task_it->second.status.state() < pod_state) {
                pod_state = task_it->second.status.state(); 
            } else if (task_it->second.status.state() > pod_state){
                pod_state = task_it->second.status.state(); 
            }
            pod_info.pod_status.mutable_status()->Add()->CopyFrom(task_it->second.status);
        }
    }
    pod_info.pod_status.mutable_resource_used()->set_millicores(millicores);
    pod_info.pod_status.mutable_resource_used()->set_memory(memory_used);
    pod_info.pod_status.mutable_resource_used()->set_read_bytes_ps(read_bytes_ps);
    pod_info.pod_status.mutable_resource_used()->set_write_bytes_ps(write_bytes_ps);
    pod_info.pod_status.mutable_resource_used()->set_syscr_ps(syscr_ps);
    pod_info.pod_status.mutable_resource_used()->set_syscw_ps(syscw_ps);
    std::string version = pod_info.pod_desc.version();
    pod_info.pod_status.set_version(version);
    switch (pod_state) {
        case kTaskPending : 
        pod_info.pod_status.set_state(kPodPending);
        break;
        case kTaskDeploy :
        pod_info.pod_status.set_state(kPodDeploying);
        break;
        case kTaskRunning :
        pod_info.pod_status.set_state(kPodRunning);
        break;
        case kTaskTerminate :
        pod_info.pod_status.set_state(kPodTerminate);
        break;
        case kTaskSuspend :
        pod_info.pod_status.set_state(kPodSuspend);
        break;
        case kTaskError : 
        pod_info.pod_status.set_state(kPodError);
        break;
        case kTaskFinish :
        pod_info.pod_status.set_state(kPodFinish);
        break;
    }

    for (size_t i = 0; i < to_del_task.size(); i++) {
        pod_info.tasks.erase(to_del_task[i]); 
    }
    return 0;
}

int PodManager::ShowPods(std::vector<PodInfo>* pods) {
    if (pods == NULL) {
        return -1; 
    }
    std::map<std::string, PodInfo>::iterator pod_it = 
        pods_.begin();
    for (; pod_it != pods_.end(); ++pod_it) {
        pods->push_back(pod_it->second); 
    }
    return 0;
}

int PodManager::DeletePod(const std::string& pod_id) {
    // async delete, only do delete to task_manager
    // pods_ erase by show pods
    std::map<std::string, PodInfo>::iterator pods_it = 
        pods_.find(pod_id);
    if (pods_it == pods_.end()) {
        LOG(WARNING, "pod %s already delete",
                pod_id.c_str()); 
        return 0;
    }
    PodInfo& pod_info = pods_it->second;
    std::map<std::string, TaskInfo>::iterator task_it = 
        pod_info.tasks.begin();
    for (; task_it != pod_info.tasks.end(); ++task_it) {
        int ret = task_manager_->DeleteTask(
                task_it->first);
        if (ret != 0) {
            LOG(WARNING, "delete task %s for pod %s failed",
                    task_it->first.c_str(),
                    pod_info.pod_id.c_str()); 
            return -1;
        }
    }
    LOG(INFO, "pod %s of job %s to delete",
            pods_it->first.c_str(),
            pod_info.job_name.c_str());
    return 0;
}

int PodManager::UpdatePod(const std::string& /*pod_id*/, const PodInfo& /*info*/) {
    // TODO  not support yet
    return -1;
}

int PodManager::AddPod(const PodInfo& info) {
    // NOTE locked by agent
    std::map<std::string, PodInfo>::iterator pods_it = 
        pods_.find(info.pod_id);
    // NOTE pods_manager should be do same 
    // when add same pod multi times
    if (pods_it != pods_.end()) {
        LOG(WARNING, "pod %s already added", info.pod_id.c_str());
        return 0; 
    }
    pods_[info.pod_id] = info;

    std::string time_str;
    GetStrFTime(&time_str);
    PodInfo& internal_info = pods_[info.pod_id];
    std::string gc_dir = FLAGS_agent_gc_dir + "/" 
        + internal_info.pod_id + "_" + time_str;
    internal_info.pod_status.set_pod_gc_path(gc_dir);
    internal_info.pod_status.set_start_time(::baidu::common::timer::get_micros());
    LOG(WARNING, "pod of job %s gc path %s", internal_info.job_name.c_str(),
            pods_[info.pod_id].pod_status.pod_gc_path().c_str());

    if (AllocPortForInitd(internal_info.initd_port) != 0){
        LOG(WARNING, "pod %s alloc port for initd failed",
                internal_info.pod_id.c_str()); 
        return -1;
    }
    LOG(INFO, "run pod with namespace_isolation: [%d]", internal_info.pod_desc.namespace_isolation());
    if (internal_info.pod_desc.requirement().read_bytes_ps() > 0) {
        LOG(INFO, "run pod %s of job %s with read_bytes_ps: [%ld]", 
                info.pod_id.c_str(), info.job_name.c_str(),
                internal_info.pod_desc.requirement().read_bytes_ps());
    } else {
        LOG(INFO, "run pod %s of job %s without read_bytes_ps limit",
                info.pod_id.c_str(), info.job_name.c_str());
    }

    if (internal_info.pod_desc.requirement().write_bytes_ps() > 0) {
        LOG(INFO, "run pod %s of job %s with write bytes ps: %ld",
                info.pod_id.c_str(), info.job_name.c_str(),
                info.pod_desc.requirement().write_bytes_ps());
    } else {
        LOG(INFO, "run pod %s of job %s without write limit",
                info.pod_id.c_str(), info.job_name.c_str());
    }

    int lanuch_initd_ret = -1;
    if (FLAGS_agent_namespace_isolation_switch && internal_info.pod_desc.namespace_isolation()) {
        lanuch_initd_ret = LanuchInitd(&internal_info);
    } else {
        lanuch_initd_ret = LanuchInitdByFork(&internal_info); 
    }

    if (lanuch_initd_ret != 0) {
        LOG(WARNING, "lanuch initd for %s failed",
                internal_info.pod_id.c_str()); 
        return -1;
    } 
    std::map<std::string, TaskInfo>::iterator task_it = 
        internal_info.tasks.begin();
    for (; task_it != internal_info.tasks.end(); ++task_it) {
        task_it->second.initd_endpoint = "127.0.0.1:";
        task_it->second.gc_dir = gc_dir;
        task_it->second.initd_endpoint.append(
                boost::lexical_cast<std::string>(
                    internal_info.initd_port));
        int ret = task_manager_->CreateTask(task_it->second);
        if (ret != 0) {
            LOG(WARNING, "create task ind %s for pods %s failed",
                    task_it->first.c_str(), internal_info.pod_id.c_str()); 
            return -1;
        }
    }
    return 0; 
}

int PodManager::ShowPod(const std::string& pod_id, PodInfo* pod) {
    if (pod == NULL) {
        return -1; 
    }    
    std::map<std::string, PodInfo>::iterator pod_it = 
                        pods_.find(pod_id);
    if (pod_it == pods_.end()) {
        return -1; 
    }

    PodInfo& pod_info = pod_it->second;
    pod->CopyFrom(pod_info);
    return 0;
}

int PodManager::ReloadPod(const PodInfo& info) {
    std::map<std::string, PodInfo>::iterator pods_it = 
        pods_.find(info.pod_id);
    if (pods_it != pods_.end()) {
        LOG(WARNING, "pod %s of job %s already loaded", 
                info.pod_id.c_str(),
                info.job_name.c_str()); 
        return 0;
    }
    pods_[info.pod_id] = info;
    PodInfo& internal_info = pods_[info.pod_id];
    internal_info.pod_status.set_podid(internal_info.pod_id);
    internal_info.pod_status.set_jobid(internal_info.job_id);
    std::string time_str;
    GetStrFTime(&time_str);
    std::string gc_dir = FLAGS_agent_gc_dir + "/" 
        + internal_info.pod_id + "_" + time_str;
    internal_info.pod_status.set_pod_gc_path(gc_dir);
    // TODO check if not in free ports
    ReloadInitdPort(internal_info.initd_port);
    std::map<std::string, TaskInfo>::iterator task_it =
        internal_info.tasks.begin();
    for (; task_it != internal_info.tasks.end(); ++task_it) {
        task_it->second.pod_id = info.pod_id; 
        task_it->second.initd_endpoint = "127.0.0.1:";
        task_it->second.job_id = internal_info.job_id;
        task_it->second.job_name = info.job_name;
        task_it->second.gc_dir = gc_dir;
        task_it->second.initd_endpoint.append(
                boost::lexical_cast<std::string>(
                    internal_info.initd_port));
        int ret = task_manager_->ReloadTask(task_it->second);
        if (ret != 0) {
            LOG(WARNING, "reload task ind %s for pods %s failed",
                    task_it->first.c_str(), 
                    internal_info.pod_id.c_str()); 
            return -1;
        }
    }
    internal_info.pod_path = FLAGS_agent_work_dir + "/" + internal_info.pod_id;
    LOG(INFO, "reload pod %s job %s initd pid %d at %s success ", 
            internal_info.pod_id.c_str(), 
            internal_info.job_name.c_str(),
            internal_info.initd_pid,
            internal_info.pod_path.c_str());
    return 0;
}

int PodManager::AllocPortForInitd(int& port) {
    if (initd_free_ports_.Size() == 0) {
        LOG(WARNING, "no free ports for alloc");
        return -1; 
    }
    port = initd_free_ports_.Front();
    initd_free_ports_.PopFront();
    return 0;
}

void PodManager::ReleasePortFromInitd(int port) {
    if (port >= FLAGS_agent_initd_port_begin 
            && port < FLAGS_agent_initd_port_end) {
        initd_free_ports_.PushBack(port);
    }
}

void PodManager::ReloadInitdPort(int port) {
    initd_free_ports_.Erase(port);
}

}   // ending namespace galaxy
}   // ending namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
