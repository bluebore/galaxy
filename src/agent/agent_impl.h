// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#ifndef  GALAXY_AGENT_IMPL_H_
#define  GALAXY_AGENT_IMPL_H_

#include "proto/agent.pb.h"
#include "proto/master.pb.h"
#include "agent/workspace_manager.h"
#include "agent/task_manager.h"
#include "common/thread_pool.h"
#include "agent/workspace.h"
namespace galaxy {

class RpcClient;

class AgentImpl : public Agent {
public:
    AgentImpl(const std::string _workspace_root_path);
    virtual ~AgentImpl();
public:
    void Report();

    /// Services
    virtual void RunTask(::google::protobuf::RpcController* controller,
                         const ::galaxy::RunTaskRequest* request,
                         ::galaxy::RunTaskResponse* response,
                         ::google::protobuf::Closure* done);
private:
    common::ThreadPool thread_pool_;
    RpcClient* rpc_client_;
    Master_Stub* master_;
    WorkspaceManager* ws_mgr_;
    TaskManager* task_mgr_;
    //agent workspace 根目录
    std::string workspace_root_path_;
};

} // namespace galaxy

#endif  // GALAXY_AGENT_IMPL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
