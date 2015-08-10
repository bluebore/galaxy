// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "gflags/gflags.h"
#include "sofa/pbrpc/pbrpc.h"

#include "agent/agent_impl.h"
#include "logging.h"

volatile static bool s_is_stop = false;

DECLARE_string(agent_port);

void StopSigHandler(int /*sig*/) {
    s_is_stop = true;
}

int main (int argc, char* argv[]) {

    using baidu::common::Log;
    using baidu::common::FATAL;
    using baidu::common::INFO;
    using baidu::common::WARNING;

    ::google::ParseCommandLineFlags(&argc, &argv, true);
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);

    baidu::galaxy::AgentImpl* agent_service = 
                        new baidu::galaxy::AgentImpl();
    if (!agent_service->Init()) {
        LOG(WARNING, "agent service init failed"); 
        return EXIT_FAILURE;
    }

    if (!rpc_server.RegisterService(agent_service)) {
        LOG(WARNING, "rpc server regist failed"); 
        return EXIT_FAILURE;
    }
    std::string server_host = std::string("0.0.0.0:") 
        + FLAGS_agent_port;

    if (!rpc_server.Start(server_host)) {
        LOG(WARNING, "Rpc Server Start failed");
        return EXIT_FAILURE;
    }

    signal(SIGTERM, StopSigHandler);
    signal(SIGINT, StopSigHandler);
    while (!s_is_stop) {
        sleep(5); 
    }

    return EXIT_SUCCESS; 
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
