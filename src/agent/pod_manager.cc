// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/pod_manager.h"
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
DECLARE_int32(agent_initd_port_begin);
DECLARE_int32(agent_initd_port_end);
DECLARE_bool(agent_namespace_isolation_switch);

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
    process::PrepareChildProcessEnvStep2(context->stdout_fd, 
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
    task_manager_(NULL) {
}

PodManager::~PodManager() {
    if (task_manager_ != NULL) {
        delete task_manager_;
        task_manager_ = NULL;
    }
}

int PodManager::Init() {
    int initd_port_begin = FLAGS_agent_initd_port_begin; 
    int initd_port_end = FLAGS_agent_initd_port_end;
    for (int i = initd_port_begin; i < initd_port_end; i++) {
        initd_free_ports_.push_back(i); 
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
    command.append(" --gce_initd_port=");
    command.append(boost::lexical_cast<std::string>(info->initd_port));
    file::Mkdir(path);
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
        process::PrepareChildProcessEnvStep2(stdout_fd, 
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
    const int CLONE_FLAG = CLONE_NEWNS | CLONE_NEWPID 
                            | CLONE_NEWUTS;
    const int CLONE_STACK_SIZE = 1024 * 1024;
    static char CLONE_STACK[CLONE_STACK_SIZE];
    
    LanuchInitdContext context;
    context.stdout_fd = -1; 
    context.stderr_fd = -1;
    context.start_command = FLAGS_agent_initd_bin;
    context.start_command.append(" --gce_initd_port=");
    context.start_command.append(boost::lexical_cast<std::string>(info->initd_port));
    context.path = FLAGS_agent_work_dir + "/" + info->pod_id;
    if (!file::Mkdir(context.path)) {
        LOG(WARNING, "mkdir %s failed", context.path.c_str()); 
        return -1;
    }

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
        ::kill(pod_info.initd_pid, SIGTERM);
        ReleasePortFromInitd(pod_info.initd_port);
        pods_.erase(pod_it);
        return -1;
    }

    std::map<std::string, TaskInfo>::iterator task_it = 
        pod_info.tasks.begin();
    PodState pod_state = kPodRunning; 
    std::vector<std::string> to_del_task;
    for (; task_it != pod_info.tasks.end(); ++task_it) {
        if (task_manager_->QueryTask(&(task_it->second)) != 0) {
            to_del_task.push_back(task_it->first);
        } else {
            // TODO pod state 
            if (task_it->second.status.state() <= kPodRunning
                    && task_it->second.status.state() < pod_state) {
                pod_state = task_it->second.status.state();
                 
            } else if (task_it->second.status.state() > pod_state){
                pod_state = task_it->second.status.state();
                 
            }
        }
    }
    pod_info.pod_status.set_state(pod_state);
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
    PodInfo& internal_info = pods_[info.pod_id];

    if (AllocPortForInitd(internal_info.initd_port) != 0){
        LOG(WARNING, "pod %s alloc port for initd failed",
                internal_info.pod_id.c_str());            
        return -1;
    }
    int lanuch_initd_ret = -1;
    if (FLAGS_agent_namespace_isolation_switch) {
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

int PodManager::AllocPortForInitd(int& port) {
    if (initd_free_ports_.size() == 0) {
        LOG(WARNING, "no free ports for alloc");
        return -1; 
    }
    port = initd_free_ports_.front();
    initd_free_ports_.pop_front();
    return 0;
}

void PodManager::ReleasePortFromInitd(int port) {
    initd_free_ports_.push_back(port);
}

}   // ending namespace galaxy
}   // ending namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
