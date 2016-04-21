// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "process.h"

#include <boost/filesystem/operations.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sched.h>

#include <iostream>
#include <sstream>

namespace baidu {
namespace galaxy {
namespace container {

Process::Process() :
    _pid(-1) {
}

Process::~Process() {
}

pid_t Process::SelfPid() {
    return getpid();
}

void Process::AddEnv(const std::string& key, const std::string& value) {
    _m_env[key] = value;
}

// Fix me: check user exist
int Process::SetRunUser(const std::string& user) {
    _user = user;
    return 0;
}

int Process::RedirectStderr(const std::string& path) {
    _stderr_path = path;
    return 0;
}

int Process::RedirectStdout(const std::string& path) {
    _stdout_path = path;
    return 0;
}

int Process::Clone(boost::function<int (void*) >* _routine, int32_t flag) {
    assert(!_stderr_path.empty());
    assert(!_stdout_path.empty());
    Context* context = new Context();
    std::vector<int> fds;

    if (0 != ListFds(SelfPid(), fds)) {
        return -1;
    }

    context->fds.swap(fds);
    const int STD_FILE_OPEN_FLAG = O_CREAT | O_APPEND | O_WRONLY;
    const int STD_FILE_OPEN_MODE = S_IRWXU | S_IRWXG | S_IROTH;
    int stdout_fd = ::open(_stdout_path.c_str(), STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);

    if (-1 == stdout_fd) {
        return -1;
    }

    int stderr_fd = ::open(_stderr_path.c_str(), STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);

    if (-1 == stderr_fd) {
        ::close(stderr_fd);
        return -1;
    }

    context->stderr_fd = stderr_fd;
    context->stdout_fd = stdout_fd;
    context->self = this;
    const static int CLONE_FLAG = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS;
    const static int CLONE_STACK_SIZE = 1024 * 1024;
    static char CLONE_STACK[CLONE_STACK_SIZE];
    _pid = ::clone(&Process::CloneRoutine,
            CLONE_STACK + CLONE_STACK_SIZE,
            CLONE_FLAG | SIGCHLD,
            &context);
    ::close(context->stdout_fd);
    ::close(context->stderr_fd);

    if (-1 == _pid) {
        // clone failed
    }

    return -1;
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

    return context->routine(context);
}

int Process::Fork(boost::function<int (void*) >* _routine) {
    assert(0);
    return -1;
}

pid_t Process::Pid() {
    return _pid;
}

int Process::ListFds(pid_t pid, std::vector<int>& fd) {
    std::stringstream ss;
    //ss >> "/proc/" >> (int)pid >> "/fd";
    boost::filesystem::path path(ss.str());
    boost::filesystem::directory_iterator begin(path);
    boost::filesystem::directory_iterator end;

    // 不包含.和..
    for (boost::filesystem::directory_iterator iter = begin; iter != end; iter++) {
    }

    return 0;
}

} //namespace container
} //namespace galaxy
} //namespace baidu

