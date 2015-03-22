// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "agent_impl.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

#include <boost/bind.hpp>

#include "common/util.h"
#include "rpc/rpc_client.h"

extern std::string FLAGS_master_addr;
extern std::string FLAGS_agent_port;

namespace galaxy {

static int ChildMain(void* args){
    execl("/bin/sh", "sh", "-c", static_cast<const char*>(args), NULL);
    /* Exit the child process if execl fails */
    assert(0);
    _exit(127);
}

AgentImpl::AgentImpl() {
    rpc_client_ = new RpcClient();
    if (!rpc_client_->GetStub(FLAGS_master_addr, &master_)) {
        assert(0);
    }
    thread_pool_.Start();
    thread_pool_.AddTask(boost::bind(&AgentImpl::Report, this));
}

AgentImpl::~AgentImpl() {
}

void AgentImpl::Report() {
    HeartBeatRequest request;
    HeartBeatResponse response;
    std::string addr = common::util::GetLocalHostName() + ":" + FLAGS_agent_port;
    request.set_agent_addr(addr);
    LOG(INFO, "Reprot to master %s", addr.c_str());
    rpc_client_->SendRequest(master_, &Master_Stub::HeartBeat,
                                &request, &response, 5, 1);
    thread_pool_.DelayTask(5000, boost::bind(&AgentImpl::Report, this));
}

void AgentImpl::OpenProcess(const std::string& task_name,
                            const std::string& task_raw,
                            const std::string& cmd_line) {
    std::string task_path = "/home/" + task_name;
    int fd = open(task_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);
    if (fd < 0) {
        LOG(WARNING, "Open %s sor write fail", task_path.c_str());
        return ;
    }
    int len = write(fd, task_raw.data(), task_raw.size());
    if (len < 0) {
        LOG(WARNING, "Write fail : %s", strerror(errno));
    }
    LOG(INFO, "Write %d bytes to %s", len, task_path.c_str());
    close(fd);
    /* FILE* fp = fopen(task_name.c_str(), "w");
    fwrite(task_raw.data(), task_raw.size(), 1, fp);
    fclose(fp);
    */
    LOG(INFO,"Fork to run %s", task_name.c_str());
    static unsigned long long stack[(8 * 1024 * 1024) / sizeof(unsigned long long)];
    int pid = clone(&ChildMain, 
                    &stack[sizeof(stack) / sizeof(stack[0]) - 1], 
                    CLONE_NEWNS, 
                    static_cast<void *>(const_cast<char *>(&task_name[0])));

    if (pid == -1) {
        LOG(WARNING, "start subprocess failed %s", strerror(errno));
        return;
    }

    LOG(INFO, "child process pid %d", pid);
}
void AgentImpl::RunTask(::google::protobuf::RpcController* controller,
                        const ::galaxy::RunTaskRequest* request,
                        ::galaxy::RunTaskResponse* response,
                        ::google::protobuf::Closure* done) {
    LOG(INFO, "Run Task %s %s", request->task_name().c_str(), request->cmd_line().c_str());
    OpenProcess(request->task_name(), request->task_raw(), request->cmd_line());
    done->Run();
}

} // namespace galxay

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
