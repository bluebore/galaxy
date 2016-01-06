// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <signal.h>
#include <unistd.h>
#include <sstream>
#include <string>

#include <sofa/pbrpc/pbrpc.h>
#include <gflags/gflags.h>
#include <logging.h>

#include "master_impl.h"
#include "utils/build_info.h"

using baidu::common::Log;
using baidu::common::FATAL;
using baidu::common::INFO;
using baidu::common::WARNING;

DECLARE_string(master_port);
DECLARE_bool(v);

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

int main(int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_v) {
        fprintf(stdout, "build version : %s\n", baidu::galaxy::GetVersion());
        fprintf(stdout, "build time : %s\n", baidu::galaxy::GetBuildTime());
        return 0;
    }
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    baidu::galaxy::MasterImpl* master_impl = new baidu::galaxy::MasterImpl();
    // reload related meta before provide service
    master_impl->Init();
    if (!rpc_server.RegisterService(master_impl)) {
        LOG(FATAL, "failed to register master service");
        exit(-1);
    }
    std::string server_addr = "0.0.0.0:" + FLAGS_master_port;
    // start to collect agent heart beat
    if (!rpc_server.Start(server_addr)) {
        LOG(FATAL, "failed to start galaxy master on %s", server_addr.c_str());
        exit(-2);
    }
    // start master related interval threads
    master_impl->Start();
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    while (!s_quit) {
        sleep(1);
    }
    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
