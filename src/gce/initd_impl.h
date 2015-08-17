// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _INITD_IMPL_H_
#define _INITD_IMPL_H_

#include <cstdlib>
#include <map>
#include <string>

#include "proto/initd.pb.h"
#include "mutex.h"
#include "thread_pool.h"

namespace baidu {
namespace galaxy {

class InitdImpl : public Initd {
public:
    InitdImpl();
    virtual ~InitdImpl();
    bool Init();
    void GetProcessStatus(::google::protobuf::RpcController* controller,
                          const ::baidu::galaxy::GetProcessStatusRequest* request,
                          ::baidu::galaxy::GetProcessStatusResponse* response, 
                          ::google::protobuf::Closure* done);

    void Execute(::google::protobuf::RpcController* controller,
                 const ::baidu::galaxy::ExecuteRequest* request,
                 ::baidu::galaxy::ExecuteResponse* response,
                 ::google::protobuf::Closure* done);

    bool LoadProcessInfoCheckPoint(const ProcessInfoCheckpoint& checkpoint);
    bool DumpProcessInfoCheckPoint(ProcessInfoCheckpoint* checkpoint);

private:
    void ZombieCheck(); 

    // collect fds for self
    // void CollectFds(std::vector<int>* fd_vector);

    // bool PrepareStdFds(const std::string& pwd, int* stdout_fd, int* stderr_fd);

    bool AttachCgroup(const std::string& cgroup_path, pid_t pid);

    std::map<std::string, ProcessInfo> process_infos_;
    Mutex lock_;
    ThreadPool background_thread_;
    std::vector<std::string> cgroup_subsystems_;
    std::string cgroup_root_;
};

}   // ending namespace galaxy
}   // ending namespace baidu

#endif

