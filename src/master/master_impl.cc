// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "master_impl.h"
#include "proto/agent.pb.h"
#include "rpc/rpc_client.h"

namespace galaxy {

MasterImpl::MasterImpl()
    : next_agent_id_(0),
      next_task_id_(0) {
    rpc_client_ = new RpcClient();
}
void MasterImpl::HeartBeat(::google::protobuf::RpcController* controller,
                           const ::galaxy::HeartBeatRequest* request,
                           ::galaxy::HeartBeatResponse* response,
                           ::google::protobuf::Closure* done) {
    const std::string& agent_addr = request->agent_addr();
    LOG(INFO, "HeartBeat from %s", agent_addr.c_str());
    LOG(INFO,"task status from %s %d",agent_addr.c_str(),request->task_status_size());
    MutexLock lock(&agent_lock_);
    if (agents_.find(agent_addr) == agents_.end()) {
        AgentInfo& agent = agents_[agent_addr];
        agent.addr = agent_addr;
        agent.id = next_agent_id_ ++;
        agent.task_num = request->task_status_size();
        agent.stub = NULL;
    }
    done->Run();
}

void MasterImpl::NewTask(::google::protobuf::RpcController* controller,
                         const ::galaxy::NewTaskRequest* request,
                         ::galaxy::NewTaskResponse* response,
                         ::google::protobuf::Closure* done) {
    MutexLock lock(&agent_lock_);
    std::string agent_addr;
    int low_load = 1<<30;
    ///TODO: Use priority queue
    std::map<std::string, AgentInfo>::iterator it = agents_.begin();
    for (; it != agents_.end(); ++it) {
        AgentInfo& ai = it->second;
        if (ai.task_num < low_load) {
            low_load = ai.task_num;
            agent_addr = ai.addr;
        }
    }
    if (agent_addr.empty()) {
        response->set_status(-1);
        done->Run();
        return;
    }

    AgentInfo& agent = agents_[agent_addr];
    if (agent.stub == NULL) {
        bool ret = rpc_client_->GetStub(agent_addr, &agent.stub);
        assert(ret);
    }
    RunTaskRequest rt_request;
    rt_request.set_task_id(next_task_id_++);
    rt_request.set_task_name(request->task_name());
    rt_request.set_task_raw(request->task_raw());
    rt_request.set_cmd_line(request->cmd_line());
    RunTaskResponse rt_response;
    LOG(INFO, "RunTask on %s", agent_addr.c_str());
    bool ret = rpc_client_->SendRequest(agent.stub, &Agent_Stub::RunTask,
                                        &rt_request, &rt_response, 5, 1);
    if (!ret) {
        LOG(WARNING, "RunTask faild agent= %s", agent_addr.c_str());
        response->set_status(-2);
    } else {
        response->set_status(0);
    }
    done->Run();
}
} // namespace galasy

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
