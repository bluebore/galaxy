// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#ifndef _SRC_AGENT_GUARDER_IMPL_H
#define _SRC_AGENT_GUARDER_IMPL_H

#include <map>
#include "proto/guarder.pb.h"
#include "sofa/pbrpc/pbrpc.h"
#include "common/mutex.h"
#include "common/thread_pool.h"

namespace galaxy {

class GuarderImpl : public Guarder {
public:
    GuarderImpl();
    virtual ~GuarderImpl();

    bool Init();

    virtual void RunProcess(
            ::google::protobuf::RpcController* controller,
            const ::galaxy::RunProcessRequest* request,
            ::galaxy::RunProcessResponse* response,
            ::google::protobuf::Closure* done);

    virtual void StatusProcess(
            ::google::protobuf::RpcController* controller,
            const ::galaxy::StatusProcessRequest* request,
            ::galaxy::StatusProcessResponse* response,
            ::google::protobuf::Closure* done);

    virtual void KillProcess(
            ::google::protobuf::RpcController* controller,
            const ::galaxy::KillProcessRequest* request,
            ::galaxy::KillProcessResponse* response,
            ::google::protobuf::Closure* done);

    virtual void CheckProcess(
            ::google::protobuf::RpcController* controller,
            const ::galaxy::CheckProcessRequest* request,
            ::galaxy::CheckProcessResponse* response,
            ::google::protobuf::Closure* done);

    virtual void RemoveProcessState(
            ::google::protobuf::RpcController* controller,
            const ::galaxy::RemoveProcessStateRequest* request,
            ::galaxy::RemoveProcessStateResponse* response,
            ::google::protobuf::Closure* done);
    bool DumpPersistenceInfo(ProcessStatusPersistence* info);
    bool LoadPersistenceInfo(const ProcessStatusPersistence* info);
private:

    void ThreadWait();

    void CollectFds(std::vector<int>* fd_vector);

    bool PrepareStdFds(const std::string& pwd, int* stdout_fd, int* stderr_fd);

    bool AttachCgroup(const std::string& cgroup_path, pid_t pid);

    std::map<std::string, ProcessStatus> process_status_;
    std::vector<std::string> cgroup_subsystems_;
    std::string cgroup_root_;
    Mutex lock_;
    common::ThreadPool* background_thread_;
};

}   // ending namespace galaxy

#endif  //_SRC_AGENT_GUARDER_IMPL_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
