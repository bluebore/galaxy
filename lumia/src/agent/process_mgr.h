// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <map>
#include <unistd.h>
#include <string>
#include <stdint.h>

namespace baidu {
namespace lumia {

// the process struct for user
struct Process {
    std::string cmd_;
    std::string user_;
    std::set<std::string> envs_;
    std::string cwd_;
    std::string pty_;
    std::string rootfs_;
    bool running_;
    pid_t pid_;
    pid_t gpid_;
    int32_t ecode_;
    int64_t ctime_;
    int64_t dtime_;
    std::string id_;
    bool coredump_;
    void CopyFrom(const Process* process) {
        cmd_ = process->cmd_;
        user_ = process->user_;
        envs_ = process->envs_;
        cwd_ = process->cwd_;
        pty_ = process->pty_;
        rootfs_ = process->rootfs_;
        running_ = process->running_;
        pid_ = process->pid_;
        gpid_ = process->gpid_;
        ecode_ = process->ecode_;
        ctime_ = process->ctime_;
        dtime_ = process->dtime_;
        id_ = process->id_;
        coredump_ = process->coredump_;
    }
};

//NOTE not thread safe
class ProcessMgr {

public:
    ProcessMgr();
    ~ProcessMgr();
    bool Exec(const Process& process, std::string* id);
    bool Wait(const std::string& id, Process* process);
    bool Kill(const std::string& id, int signal);
private:
    bool GetOpenedFds(std::set<int>& fds);
    bool ResetIo(const Process& process);
    bool GetUser(const std::string& user, 
                 uid_t* uid,
                 gid_t* gid);
    std::string GetUUID();
    Process* New();
private:
    std::map<std::string, Process* > processes_;
    pid_t mypid_;
};

}
}
