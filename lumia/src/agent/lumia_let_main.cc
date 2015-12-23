// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sofa/pbrpc/pbrpc.h>
#include "agent/lumia_let.h"
#include <gflags/gflags.h>
#include <signal.h>
#include "logging.h"

DECLARE_string(lumia_agent_port);

DECLARE_string(lumia_agent_cgroups);

using baidu::common::Log;
using baidu::common::FATAL;
using baidu::common::INFO;
using baidu::common::WARNING;

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}


int main(int argc, char* args[]){
    ::google::ParseCommandLineFlags(&argc, &args, true);
    ::baidu::lumia::LumiaLetImpl* agent = new ::baidu::lumia::LumiaLetImpl();
    bool ok = agent->Init();
    if (!ok) {
        LOG(FATAL, "fail to init agent");
        return -1;
    }
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    std::string agent_addr = std::string("0.0.0.0:") + FLAGS_lumia_agent_port;
    if (!rpc_server.RegisterService(agent)) {
        LOG(FATAL, "fail to boot rpc server");
        return -1;
    }
    if (!rpc_server.Start(agent_addr)) {
        LOG(WARNING, "Rpc Server Start failed");
        return EXIT_FAILURE;
    }
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    while (!s_quit) {
        sleep(2);
    }  
    return 0;
}
