// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <assert.h>
#include <vector>
#include <sofa/pbrpc/pbrpc.h>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <boost/scoped_ptr.hpp>

#include "setting_utils.h"
#include "appmaster_v2/appmaster_impl.h"
#include "protocol/appmaster.pb.h"
#include "nexus/nexus_proxy.h"

DECLARE_string(appmaster_port);
DECLARE_string(appmaster_ip);
DECLARE_string(appmaster_addr);

DECLARE_string(nexus_addr);
DECLARE_string(nexus_root);
DECLARE_string(appmaster_lock);

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

extern baidu::galaxy::util::ErrorCode CheckCommandLine();

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    baidu::galaxy::SetupLog("appmaster");
    LOG(INFO) << "setup log successfully";

    baidu::galaxy::util::ErrorCode ec = CheckCommandLine();
    if (ec.Code() != 0) {
        LOG(FATAL) << "check cmd line failed: " << ec.Message();
        exit(-1);
    }

    boost::scoped_ptr<baidu::galaxy::am::AppMasterImpl> appmaster(new baidu::galaxy::am::AppMasterImpl());
    appmaster->Setup();
   
    // start service
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    if (!rpc_server.RegisterService(static_cast<baidu::galaxy::proto::AppMaster*>(appmaster.get()))) {
        LOG(WARNING) << "failed to register appmaster service";
        exit(-1);
    }

    std::string endpoint = "0.0.0.0:" + FLAGS_appmaster_port;
    if (!rpc_server.Start(endpoint)) {
        LOG(WARNING)  << "failed to start server on %s", endpoint.c_str();
        exit(-2);
    }
    LOG(INFO) << "appmaster service start successfully";


    // init nexus
    // register to nexus, try to lock
    LOG(INFO) << "try to register to nexus ...";
    const std::string appmaster_endpoint = FLAGS_appmaster_ip + ":" +FLAGS_appmaster_port;
    // if register failed, log and exit
    baidu::galaxy::util::NexusProxy::Setup(FLAGS_nexus_addr);
    baidu::galaxy::util::NexusProxy::GetInstance()->Register(FLAGS_nexus_root + FLAGS_appmaster_lock,
                FLAGS_nexus_root + FLAGS_appmaster_addr,
                appmaster_endpoint);
    
    LOG(INFO) << "register to nexus successfully, nexus lock is " << FLAGS_nexus_root + FLAGS_appmaster_lock
        << ", master address is " << FLAGS_nexus_root + FLAGS_appmaster_addr;

    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);

    while (!s_quit) {
        sleep(1);
    }
    _exit(0);
    return 0;
}

