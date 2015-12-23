// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <gflags/gflags.h>
#include <master/lumia_master_impl.h>
#include <sofa/pbrpc/pbrpc.h>
#include <logging.h>
#include <signal.h>

using baidu::common::Log;
using baidu::common::FATAL;
using baidu::common::INFO;
using baidu::common::WARNING;


DECLARE_string(lumia_ctrl_port);

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

int main(int argc, char* args[]) {
    ::google::ParseCommandLineFlags(&argc, &args, true);
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    ::baidu::lumia::LumiaMasterImpl* ctrl = new ::baidu::lumia::LumiaMasterImpl();
    ctrl->Init();
    if (!rpc_server.RegisterService(ctrl)) {
        LOG(FATAL, "failed to register lumia master");
        exit(-1);
    }   
    std::string server_addr = "0.0.0.0:" + FLAGS_lumia_ctrl_port;
    if (!rpc_server.Start(server_addr)) {
        LOG(FATAL, "failed to start lumia master on %s", server_addr.c_str());
        exit(-2);
    }else {
        LOG(INFO, "start lumia master with port %s", FLAGS_lumia_ctrl_port.c_str());
    }  
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    while (!s_quit) {
        sleep(1);
    }
    return 0;
}
