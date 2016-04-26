// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "process.h"

#include <boost/filesystem/operations.hpp>
#include <boost/system/error_code.hpp>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <sched.h>

#include <iostream>
#include <sstream>

namespace baidu {
namespace galaxy {
namespace container {

Process::Process() :
    pid_(-1) {
}

Process::~Process() {
}

pid_t Process::SelfPid() {
    return getpid();
}

void Process::AddEnv(const std::string& key, const std::string& value) {
    env_[key] = value;
}

void Process::AddEnv(const std::map<std::string, std::string>& env) {
    if (env.empty()) {
        return;
    }
    
    std::map<std::string, std::string>::const_iterator iter = env.begin();
    while(iter != env.end()) {
        env_[iter->first] = iter->second;
        iter++;
    }
}


// Fix me: check user exist
int Process::SetRunUser(const std::string& user) {
    user_ = user;
    return 0;
}

int Process::RedirectStderr(const std::string& path) {
    stderr_path_ = path;
    return 0;
}

int Process::RedirectStdout(const std::string& path) {
    stdout_path_ = path;
    return 0;
}

int Process::Clone(boost::function<int (void*) > routine, void* param, int32_t flag) {
    assert(!stderr_path_.empty());
    assert(!stdout_path_.empty());
    Context* context = new Context();
    std::vector<int> fds;

    if (0 != ListFds(SelfPid(), fds)) {
        return -1;
    }

    context->fds.swap(fds);
    const int STD_FILE_OPEN_FLAG = O_CREAT | O_APPEND | O_WRONLY;
    const int STD_FILE_OPEN_MODE = S_IRWXU | S_IRWXG | S_IROTH;
    int stdout_fd = ::open(stdout_path_.c_str(), STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);

    if (-1 == stdout_fd) {
        return -1;
    }

    int stderr_fd = ::open(stderr_path_.c_str(), STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);

    if (-1 == stderr_fd) {
        ::close(stderr_fd);
        return -1;
    }

    context->stderr_fd = stderr_fd;
    context->stdout_fd = stdout_fd;
    context->self = this;
    context->routine = routine;
    context->parameter = param;
    const static int CLONE_FLAG = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS;
    const static int CLONE_STACK_SIZE = 1024 * 1024;
    static char CLONE_STACK[CLONE_STACK_SIZE];
    pid_ = ::clone(&Process::CloneRoutine,
            CLONE_STACK + CLONE_STACK_SIZE,
            CLONE_FLAG | SIGCHLD,
            context);
    ::close(context->stdout_fd);
    ::close(context->stderr_fd);

    if (-1 == pid_) {
        perror("clone failed:");
    }

    return 0;
}

int Process::CloneRoutine(void* param) {
    assert(NULL != param);
    Context* context = (Context*) param;
    assert(context->stdout_fd > 0);
    assert(context->stderr_fd > 0);
    assert(NULL != context->self);
    assert(NULL != context->routine);

    while (::dup2(context->stdout_fd, STDOUT_FILENO) == -1
            && errno == EINTR) {
    }

    while (::dup2(context->stderr_fd, STDERR_FILENO) == -1
            && errno == EINTR) {
    }

    for (size_t i = 0; i < context->fds.size(); i++) {
        if (STDOUT_FILENO == context->fds[i]
                || STDERR_FILENO == context->fds[i]
                || STDIN_FILENO == context->fds[i]) {
            // not close std fds
            continue;
        }

        ::close(context->fds[i]);
    }

    pid_t pid = SelfPid();

    if (0 != ::setpgid(pid, pid)) {
        std::cerr << "set pgid failed" << std::endl;
    }

    // export env
    std::map<std::string, std::string>::const_iterator iter = context->envs.begin();
    while(iter != context->envs.end()) {
        if (0 != ::setenv(iter->first.c_str(), iter->second.c_str(), 1)) {
            // LOG
            return -1;
        }
        iter++;
    }

    return context->routine(context->parameter);
}



int Process::Fork(boost::function<int (void*) > routine, void* param) {
    assert(0);
    return -1;
}

pid_t Process::Pid() {
    return pid_;
}

int Process::Wait(int& status) {
    if (pid_ <=0) {
        return -1;
    }
    
    if (pid_ != ::waitpid(pid_, &status, 0)) {
        return -1;
    }
    return 0;
}

int Process::ListFds(pid_t pid, std::vector<int>& fd) {
    std::stringstream ss;
    ss << "/proc/" << (int)pid << "/fd";
    
    boost::filesystem::path path(ss.str());
    boost::system::error_code ec;
    if (!boost::filesystem::exists(path, ec)) {
        return -1;
    }
    boost::filesystem::directory_iterator begin(path);
    boost::filesystem::directory_iterator end;

    // exclude .. and .
    for (boost::filesystem::directory_iterator iter = begin; iter != end; iter++) {
        //std::string file_name = iter->filename();
        std::string file_name = iter->path().filename().string();
        if (file_name != "." && file_name != "..") {
            fd.push_back(atoi(file_name.c_str()));
        }
    }

    return 0;
}

} //namespace container
} //namespace galaxy
} //namespace baidu

