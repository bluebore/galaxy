// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "gflags/gflags.h"
#include "sofa/pbrpc/pbrpc.h"
#include "utils/http_server.h"
#include "agent/agent_impl.h"
#include "utils/build_info.h"
#include "logging.h"

volatile static bool s_is_stop = false;

DECLARE_bool(v);
DECLARE_string(agent_port);
DECLARE_int32(agent_http_port);
DECLARE_string(agent_work_dir);
DECLARE_string(agent_gc_dir);
DECLARE_string(agent_coredump_dir);
void StopSigHandler(int /*sig*/) {
    s_is_stop = true;
}

void SigChldHandler(int /*sig*/) {
    int status = 0;
    while (true) {
        pid_t pid = ::waitpid(-1, &status, WNOHANG);    
        if (pid == -1 || pid == 0) {
            return; 
        }
    }    
}

int main (int argc, char* argv[]) {

    using baidu::common::Log;
    using baidu::common::FATAL;
    using baidu::common::INFO;
    using baidu::common::WARNING;

    ::google::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_v) {
        fprintf(stdout, "build version : %s\n", baidu::galaxy::GetVersion());
        fprintf(stdout, "build time : %s\n", baidu::galaxy::GetBuildTime());
        return 0;
    }
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
    std::vector<std::pair<std::string, std::string> > router;
    router.push_back(std::make_pair("/gc", FLAGS_agent_gc_dir));
    router.push_back(std::make_pair("/container", FLAGS_agent_work_dir));
    router.push_back(std::make_pair("/coredump", FLAGS_agent_coredump_dir));
    ::baidu::galaxy::HttpFileServer http_server(router, FLAGS_agent_http_port);
    http_server.Start(10);
    LOG(INFO, "start http server on %d", FLAGS_agent_http_port);
    signal(SIGINT, StopSigHandler);
    signal(SIGTERM, StopSigHandler);
    signal(SIGCHLD, SigChldHandler);
    while (!s_is_stop) {
        sleep(5); 
    }

    return EXIT_SUCCESS; 
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
