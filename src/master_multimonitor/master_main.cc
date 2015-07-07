// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "master_impl.h"

#include <sofa/pbrpc/pbrpc.h>

#include <stdio.h>
#include <signal.h>

extern std::string FLAGS_master_port;
extern int FLAGS_task_deploy_timeout;
extern std::string FLAGS_master_checkpoint_path;
extern int64_t FLAGS_master_safe_mode_last;

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/)
{
    s_quit = true;
}

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        char s[1024];
        if (sscanf(argv[i], "--port=%s", s) == 1) {
            FLAGS_master_port = s;
        } else if (sscanf(argv[i], "--task_deploy_timeout=%s", s) == 1)  {
            FLAGS_task_deploy_timeout = atol(s); 
        } else if (sscanf(argv[i], "--safe_mode_last=%s", s) == 1) {
            FLAGS_master_safe_mode_last = atol(s);
        } else if (sscanf(argv[i], "--checkpoint_dir=%s", s) == 1) {
            FLAGS_master_checkpoint_path = s;
        } else {
            fprintf(stderr, "Invalid flag '%s'\n", argv[i]);
            exit(1);
        }
    }

    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);

    galaxy::MasterImpl* master_service = new galaxy::MasterImpl();

    if (!master_service->Recover()) {
        fprintf(stderr, "master recover from checkpoint failed\n"); 
        return EXIT_FAILURE;
    }

    if (!rpc_server.RegisterService(master_service)) {
            return EXIT_FAILURE;
    }

    std::string server_host = std::string("0.0.0.0:") + FLAGS_master_port;
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
