// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gce/initd_impl.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <boost/lexical_cast.hpp>
#include "gce/utils.h"
#include "logging.h"

namespace baidu {
namespace galaxy {

InitdImpl::InitdImpl() :
    process_infos_(),
    lock_(),
    background_thread_(1) {
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

void InitdImpl::Execute(::google::protobuf::RpcController* controller,
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

    // 1. collect initd fds
    std::vector<int> fd_vector;
    CollectFds(&fd_vector);

    // 2. prepare std fds for child 
    int stdout_fd = 0;
    int stderr_fd = 0;
    if (!PrepareStdFds(request->path(), &stdout_fd, &stderr_fd)) {
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

    // 3. Fork     
    pid_t child_pid = ::fork();
    if (child_pid == -1) {
        LOG(WARNING, "fork %s failed err[%d: %s]",
                request->key().c_str(), errno, strerror(errno)); 
        response->set_status(kUnknown);
        done->Run();
        return;
    } else if (child_pid == 0) {
        // setpgid  
        pid_t my_pid = ::getpid();
        int ret = ::setpgid(my_pid, my_pid);
        if (ret != 0) {
            assert(0); 
        }
        
        // attach cgroup 
        if (request->has_cgroup_path() 
            && !AttachCgroup(request->cgroup_path(), my_pid)) {
            assert(0); 
        }

        // cd pwd
        ::chdir(request->path().c_str());

        // deal with std fd
        while (::dup2(stdout_fd, STDOUT_FILENO) == -1 
                && errno == EINTR) {}
        while (::dup2(stderr_fd, STDERR_FILENO) == -1
                && errno == EINTR) {}
        for (size_t i = 0; i < fd_vector.size(); i++) {
            if (fd_vector[i] == STDOUT_FILENO
                || fd_vector[i] == STDERR_FILENO
                || fd_vector[i] == STDIN_FILENO) {
                // not close std fds
                continue; 
            } 
            ::close(fd_vector[i]);
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
        assert(0);
    }

    // close child's std fds
    ::close(stdout_fd); 
    ::close(stderr_fd);

    ProcessInfo info;      
    info.set_pid(child_pid);
    info.set_status(kProcessRunning);
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

bool InitdImpl::PrepareStdFds(const std::string& pwd, 
                              int* stdout_fd, 
                              int* stderr_fd) {
    if (stdout_fd == NULL || stderr_fd == NULL) {
        LOG(WARNING, "prepare stdout_fd or stderr_fd NULL"); 
        return false;
    }
    std::string now_str_time;
    GetStrFTime(&now_str_time);
    std::string stdout_file = pwd + "/stdout_" + now_str_time;
    std::string stderr_file = pwd + "/stderr_" + now_str_time;

    const int STD_FILE_OPEN_FLAG = O_CREAT | O_APPEND | O_WRONLY;
    const int STD_FILE_OPEN_MODE = S_IRWXU | S_IRWXG | S_IROTH;
    *stdout_fd = ::open(stdout_file.c_str(), 
            STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    *stderr_fd = ::open(stderr_file.c_str(),
            STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    if (*stdout_fd == -1 || *stderr_fd == -1) {
        LOG(WARNING, "open file failed err[%d: %s]",
                errno, strerror(errno));
        return false; 
    }
    return true;
}

void InitdImpl::CollectFds(std::vector<int>* fd_vector) {
    if (fd_vector == NULL) {
        return; 
    }

    pid_t current_pid = ::getpid();
    std::string pid_path = "/proc/";
    pid_path.append(boost::lexical_cast<std::string>(current_pid));
    pid_path.append("/fd/");

    std::vector<std::string> fd_files;
    if (!file::ListFiles(pid_path, &fd_files)) {
        LOG(WARNING, "list %s failed", pid_path.c_str());    
        return;
    }

    for (size_t i = 0; i < fd_files.size(); i++) {
        if (fd_files[i] == "." || fd_files[i] == "..") {
            continue; 
        }
        fd_vector->push_back(::atoi(fd_files[i].c_str()));    
    }
    return;
}


}   // ending namespace galaxy
}   // ending namespace baidu
