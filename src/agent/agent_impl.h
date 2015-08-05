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
#include "agent/resource_manager.h"
#include "common/thread_pool.h"
#include "agent/workspace.h"
#include "agent/persistence_handler.h"

namespace common {
    class HttpFileServer;
}

namespace galaxy {

class RpcClient;

class AgentImpl : public Agent {
public:
    AgentImpl();
    virtual ~AgentImpl();
public:
    bool Init();
    void Report();

    /// Services
    virtual void RunTask(::google::protobuf::RpcController* controller,
                         const ::galaxy::RunTaskRequest* request,
                         ::galaxy::RunTaskResponse* response,
                         ::google::protobuf::Closure* done);
   virtual void KillTask(::google::protobuf::RpcController* controller,
                         const ::galaxy::KillTaskRequest* request,
                         ::galaxy::KillTaskResponse* response,
                         ::google::protobuf::Closure* done);
   virtual void SetPassword(::google::protobuf::RpcController* controller,
                            const ::galaxy::SetPasswordRequest* request,
                            ::galaxy::SetPasswordResponse* response,
                            ::google::protobuf::Closure* done);
protected:
   std::string GetPersistenceKey(const TaskInfo& task_info);
private:
    common::ThreadPool thread_pool_;
    RpcClient* rpc_client_;
    Master_Stub* master_;
    WorkspaceManager* ws_mgr_;
    TaskManager* task_mgr_;
    ResourceManager* resource_mgr_;
    //agent workspace 根目录
    std::string workspace_root_path_;
    int64_t version_;
    common::HttpFileServer* http_server_;
    PersistenceHandler* persistence_handler_;
};

} // namespace galaxy

#endif  // GALAXY_AGENT_IMPL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
