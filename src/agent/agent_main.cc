// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "agent_impl.h"

#include <sofa/pbrpc/pbrpc.h>

#include <stdio.h>
#include <signal.h>
#include <gflags/gflags.h>

#include "agent/resource_collector_engine.h"
#include "agent/dynamic_resource_scheduler.h"

DECLARE_string(agent_port);
DECLARE_int32(agent_http_port);
DECLARE_string(master_addr);
DECLARE_string(agent_work_dir);
DECLARE_string(container);
DECLARE_string(cgroup_root);
DECLARE_string(task_acct);
DECLARE_double(cpu_num);
DECLARE_int64(mem_gbytes);
DECLARE_int64(mem_bytes);

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/) {
    s_quit = true;
}

int main(int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);

    FLAGS_mem_bytes = FLAGS_mem_gbytes*(1024*1024*1024);
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);

    galaxy::AgentImpl* agent_service = new galaxy::AgentImpl();

    if (!agent_service->Init()) {
        return -1;
    }
    // use for atexit regist
    galaxy::ResourceCollectorEngine* engine = 
        galaxy::GetResourceCollectorEngine();
    engine = engine;
    galaxy::DynamicResourceScheduler* dy_scheduler = 
        galaxy::GetDynamicResourceScheduler();
    dy_scheduler = dy_scheduler;

    if (!rpc_server.RegisterService(agent_service)) {
        return EXIT_FAILURE;
    }

    std::string server_host = std::string("0.0.0.0:") + FLAGS_agent_port;
    if (!rpc_server.Start(server_host)) {
        return EXIT_FAILURE;
    }

    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    while (!s_quit) {
        sleep(1);
    }

    return EXIT_SUCCESS;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
