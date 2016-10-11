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
#include "setting_utils.h"
#include "util.h"
#include "appmaster_impl.h"

DECLARE_string(appmaster_port);

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    baidu::galaxy::SetupLog("appmaster");
    baidu::galaxy::AppMasterImpl * appmaster = new baidu::galaxy::AppMasterImpl();
    appmaster->Init();
    std::string appmaster_endpoint = ::baidu::common::util::GetLocalHostName() + ":" +FLAGS_appmaster_port;
    bool nexus_ok = appmaster->RegisterOnNexus(appmaster_endpoint);
    if (!nexus_ok) {
        LOG(WARNING) << "fail to register AppMaster on nexus";
        exit(-1);
    }
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    if (!rpc_server.RegisterService(static_cast<baidu::galaxy::proto::AppMaster*>(appmaster))) {
        LOG(WARNING) << "failed to register appmaster service";
        exit(-1);
    }

    std::string endpoint = "0.0.0.0:" + FLAGS_appmaster_port;
    if (!rpc_server.Start(endpoint)) {
        LOG(WARNING)  << "failed to start server on %s", endpoint.c_str();
        exit(-2);
    }
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);

    LOG(INFO) << "appmaster started.";
    while (!s_quit) {
        sleep(1);
    }
    return 0;
}
