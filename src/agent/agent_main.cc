// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <sofa/pbrpc/pbrpc.h>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include "agent_impl.h"
#include "setting_utils.h"

DECLARE_string(agent_port);

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    baidu::galaxy::SetupLog("agent");

    baidu::galaxy::AgentImpl * agent = new baidu::galaxy::AgentImpl();
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    if (!rpc_server.RegisterService(static_cast<baidu::galaxy::proto::Agent*>(agent))) {
        LOG(WARNING) << "failed to register agent service";
        exit(-1);
    }

    std::string endpoint = "0.0.0.0:" + FLAGS_agent_port;
    if (!rpc_server.Start(endpoint)) {
        LOG(WARNING) << "failed to start server on " << endpoint;
        exit(-2);
    }
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);

    LOG(INFO) << "agent started.";
    while (!s_quit) {
        sleep(1);
    }
    return 0;
}


