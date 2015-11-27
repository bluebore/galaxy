// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gce/initd_impl.h"
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
#include <boost/bind.hpp>
#include "gflags/gflags.h"
#include "agent/utils.h"
#include "agent/cgroups.h"
#include "logging.h"

DECLARE_string(gce_cgroup_root);
DECLARE_string(gce_support_subsystems);
DECLARE_int64(gce_initd_zombie_check_interval);

namespace baidu {
namespace galaxy {

InitdImpl::InitdImpl() :
    process_infos_(),
    lock_(),
    background_thread_(1) {
    background_thread_.DelayTask(
            FLAGS_gce_initd_zombie_check_interval,
            boost::bind(&InitdImpl::ZombieCheck, this));
}

void InitdImpl::ZombieCheck() {
    int status = 0;
    pid_t pid = ::waitpid(-1, &status, WNOHANG);    
    if (pid > 0) {
        MutexLock scope_lock(&lock_);
        std::map<std::string, ProcessInfo>::iterator it
            = process_infos_.begin();
        for (; it != process_infos_.end(); ++it) {
            if (it->second.pid() == pid) {
                if (WIFEXITED(status)) {
                    it->second.set_exit_code(WEXITSTATUS(status)); 
                } else if (WIFSIGNALED(status)) {
                    it->second.set_exit_code(128 + WTERMSIG(status));
                } 
                LOG(WARNING, "process %d exit code %d", pid, it->second.exit_code());
                it->second.set_status(kProcessTerminate);
                break;
            } 
        }
    }
    background_thread_.DelayTask(
            FLAGS_gce_initd_zombie_check_interval,
            boost::bind(&InitdImpl::ZombieCheck, this));
}

bool InitdImpl::Init() {
    return true;
}

void InitdImpl::GetProcessStatus(::google::protobuf::RpcController* /*controller*/,
                       const ::baidu::galaxy::GetProcessStatusRequest* request,
                       ::baidu::galaxy::GetProcessStatusResponse* response,
                       ::google::protobuf::Closure* done) {
    if (!request->has_key()) {
        response->set_status(kNotFound);     
        done->Run();
        return;
    }
    MutexLock scope_lock(&lock_);
    std::map<std::string, ProcessInfo>::iterator it = process_infos_.find(request->key());
    if (it == process_infos_.end()) {
        response->set_status(kNotFound);     
        done->Run(); 
        return;
    }
    response->mutable_process()->CopyFrom(it->second);
    response->set_status(kOk);
    done->Run();
    return;
}

bool InitdImpl::GetProcPath(std::string* proc_path) {
    if (proc_path == NULL) {
        return false; 
    }
    proc_path->clear();
    // TODO no need to get each time
    pid_t pid = ::getpid();
    if (pid != 1) {
        proc_path->append("/proc/");
        return true;
    }

    if (!process::GetCwd(proc_path)) {
        LOG(WARNING, "get cwd failed"); 
        return false;
    }

    proc_path->append("/proc/");
    return true;
}

void InitdImpl::Execute(::google::protobuf::RpcController* /*controller*/,
                       const ::baidu::galaxy::ExecuteRequest* request,
                       ::baidu::galaxy::ExecuteResponse* response,
                       ::google::protobuf::Closure* done) {
    if (!request->has_key() 
        || !request->has_commands() 
        || !request->has_path()) {
        LOG(WARNING, "not enough input params"); 
        response->set_status(kInputError);
        done->Run();
        return;
    }

    LOG(INFO, "run command %s at %s in cgroup %s", 
            request->commands().c_str(), request->path().c_str(),
            request->cgroup_path().c_str());

    // 1. collect initd fds
    std::vector<int> fd_vector;
    std::string proc_path;
    if (!GetProcPath(&proc_path)) {
        response->set_status(kUnknown);
        done->Run();
        return;
    }
    std::vector<std::string> files;
    if (!file::ListFiles(proc_path, &files)) {
        LOG(WARNING, "list new proc failed");
    }
    proc_path.append(boost::lexical_cast<std::string>(::getpid())); 
    proc_path.append("/fd/");
    if (!file::ListFiles(proc_path, &files)) {
        LOG(WARNING, "list new proc failed");
        response->set_status(kInputError);
        done->Run();
        return; 
    }
    for (size_t i = 0; i < files.size(); i++) {
        if (files[i] == "." || files[i] == "..") {
            continue; 
        } 
        fd_vector.push_back(::atoi(files[i].c_str()));
    }
    // check if need chroot
    bool is_chroot = request->has_chroot_path();
    if (is_chroot) {
        LOG(WARNING, "chroot %s", request->chroot_path().c_str()); 
    }

    // 2. prepare std fds for child 
    int stdout_fd = -1;
    int stderr_fd = -1;
    int stdin_fd = -1;
    if (!process::PrepareStdFds(request->path(), 
                                &stdout_fd, &stderr_fd)) {
        if (stdout_fd != -1) {
            ::close(stdout_fd); 
        }

        if (stderr_fd != -1) {
            ::close(stderr_fd); 
        }
        LOG(WARNING, "prepare for %s std file failed",
                request->path().c_str()); 
        response->set_status(kUnknown);
        done->Run();
        return;
    }

    int pty_fds = -1;
    if (request->has_pty_file()) {
        pty_fds = ::open(request->pty_file().c_str(), O_RDWR); 
        stdout_fd = pty_fds;
        stderr_fd = pty_fds;
        stdin_fd = pty_fds;
    }

    // 3. Fork     
    pid_t child_pid = ::fork();
    if (child_pid == -1) {
        LOG(WARNING, "fork %s failed err[%d: %s]",
                request->key().c_str(), errno, strerror(errno)); 
        if (pty_fds != -1) {
            ::close(pty_fds); 
        }
        if (stdout_fd != -1) {
            ::close(stdout_fd);     
        }
        if (stderr_fd != -1) {
            ::close(stderr_fd); 
        }
        response->set_status(kUnknown);
        done->Run();
        return;
    } else if (child_pid == 0) {
        // setpgid  & chdir
        pid_t my_pid = ::getpid();
        process::PrepareChildProcessEnvStep1(my_pid, 
                                             request->path().c_str());
        // attach cgroup 
        for (int i = 0; i < request->cgroups_size(); i++) {
            bool ok = AttachCgroup(request->cgroups(i).cgroup_path(), my_pid);
            if (!ok) {
                assert(0);
            }
        }
        process::PrepareChildProcessEnvStep2(stdin_fd,
                                             stdout_fd, 
                                             stderr_fd, 
                                             fd_vector);
        if (is_chroot) {
            if (::chroot(request->chroot_path().c_str()) != 0) {
                assert(0);    
            }
        }
        
        if (request->has_user() 
                && !user::Su(request->user())) {
            assert(0); 
        }
    
        // prepare argv
        char* argv[] = {
            const_cast<char*>("sh"),
            const_cast<char*>("-c"),
            const_cast<char*>(request->commands().c_str()),
            NULL};
        // prepare envs
        char* env[request->envs_size() + 1];
        for (int i = 0; i < request->envs_size(); i++) {
            env[i] = const_cast<char*>(request->envs(i).c_str());     
        }
        env[request->envs_size()] = NULL;
        // exec
        ::execve("/bin/sh", argv, env);
        fprintf(stdout, "execve %s err[%d: %s]", 
                request->commands().c_str(), errno, strerror(errno));
        assert(0);
    }

    // close child's std fds
    if (pty_fds != -1) {
        ::close(pty_fds); 
    }
    if (stdout_fd != -1) {
        ::close(stdout_fd);     
    }
    if (stderr_fd != -1) {
        ::close(stderr_fd); 
    }

    ProcessInfo info;      
    info.set_key(request->key());
    info.set_pid(child_pid);
    info.set_status(kProcessRunning);
    info.set_exit_code(0);
    {
        MutexLock scope_lock(&lock_); 
        process_infos_[request->key()] = info;
    }
    response->set_key(request->key());
    response->set_pid(child_pid);
    response->set_status(kOk);
    done->Run();
    return;
}

bool InitdImpl::AttachCgroup(const std::string& cgroup_path, 
                             pid_t child_pid) {
    if (cgroup_path.empty()) {
        return true; 
    }
    if (!cgroups::AttachPid(cgroup_path, 
                            child_pid)) {
        return false;     
    }
    return true;
}

bool InitdImpl::LoadProcessInfoCheckPoint(const ProcessInfoCheckpoint& checkpoint) {
    MutexLock scope_lock(&lock_);
    for (int i = 0; i < checkpoint.process_infos_size(); i++) {
        ProcessInfo info = checkpoint.process_infos(i); 
        process_infos_[info.key()] = info;         
    }
    return true;
}

bool InitdImpl::DumpProcessInfoCheckPoint(ProcessInfoCheckpoint* checkpoint) {
    if (checkpoint == NULL) {
        return false; 
    }

    MutexLock scope_lock(&lock_);
    std::map<std::string, ProcessInfo>::iterator it = 
        process_infos_.begin();
    for (; it != process_infos_.end(); ++it) {
        ProcessInfo* info = checkpoint->add_process_infos();
        info->CopyFrom(it->second);
    }
    return true;
}

}   // ending namespace galaxy
}   // ending namespace baidu
