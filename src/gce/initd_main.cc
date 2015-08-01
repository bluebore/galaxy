// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include "gce/initd_impl.h"
#include "gflags/gflags.h"
#include "sofa/pbrpc/pbrpc.h"
#include "logging.h"
#include "gce/utils.h"

const int RPC_START_FAIL = -3;

DECLARE_string(gce_initd_dump_file);
DECLARE_string(gce_initd_port);

volatile static bool s_is_stop = false;
volatile static bool s_is_restart = false;

void StopSigHandler(int /*sig*/) {
    s_is_stop = true;        
}

void RestartSigHandler(int /*sig*/) {
    s_is_restart = true;
}

bool LoadInitdCheckpoint(baidu::galaxy::InitdImpl* service) {
    if (service == NULL) {
        return false; 
    }

    std::ifstream fin(FLAGS_gce_initd_dump_file.c_str(), std::ios::binary);
    if (!fin.is_open()) {
        baidu::common::LOG(baidu::WARNING, "open %s failed",
                FLAGS_gce_initd_dump_file.c_str());
        return false;
    }

    std::string pb_buffer;
    fin >> pb_buffer;
    fin.close();

    // TODO delete dump file

    baidu::galaxy::ProcessInfoCheckpoint checkpoint;
    if (!checkpoint.ParseFromString(pb_buffer)) {
        baidu::common::LOG(baidu::WARNING, "parse checkpoint failed");     
        return false;
    }
    return service->LoadProcessInfoCheckPoint(checkpoint);
}

bool DumpInitdCheckpoint(baidu::galaxy::InitdImpl* service) {

    if (service == NULL) {
        return false;
    }

    baidu::galaxy::ProcessInfoCheckpoint checkpoint;

    if (!service->DumpProcessInfoCheckPoint(&checkpoint)) {
        baidu::common::LOG(baidu::WARNING, "dump checkpoint failed");     
        return false;
    }

    std::string checkpoint_buffer;
    if (!checkpoint.SerializeToString(&checkpoint_buffer)) {
        baidu::common::LOG(baidu::WARNING, "serialize to string error");     
        return false;
    }

    std::ofstream ofs(FLAGS_gce_initd_dump_file.c_str(), std::ios::binary);

    if (!ofs.is_open()) {
        baidu::common::LOG(baidu::WARNING, "serialize to string error");     
        return false;
    }

    ofs << checkpoint_buffer;
    ofs.close();

    return false;
}

int main (int argc, char* argv[]) {
    // keep argv for restart
    char* restart_argv[argc + 1];
    int restart_argc = argc;
    for (int i = 0; i < restart_argc; i++) {
        restart_argv[i] =  new char[strlen(argv[i]) + 1]; 
        ::strncpy(restart_argv[i], argv[i], strlen(argv[i]));
        restart_argv[i][strlen(argv[i])] = '\0';
    }
    ::google::ParseCommandLineFlags(&argc, &argv, true);

    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);

    baidu::galaxy::InitdImpl* initd_service =
                                new baidu::galaxy::InitdImpl();
    if (!initd_service->Init()) {
        baidu::common::LOG(baidu::WARNING, "Initd Service Init failed"); 
        return EXIT_FAILURE;
    }

    if (baidu::galaxy::file::IsExists(FLAGS_gce_initd_dump_file)
            && !LoadInitdCheckpoint(initd_service)) {
        baidu::common::LOG(baidu::WARNING, "Load Initd Checkpoint failed");     
        return EXIT_FAILURE;
    }

    if (!rpc_server.RegisterService(initd_service)) {
        baidu::common::LOG(baidu::WARNING, "Rpc Server Regist Service failed");
        return EXIT_FAILURE;
    }

    std::string server_host = std::string("0.0.0.0:") 
        + FLAGS_gce_initd_port;
    if (!rpc_server.Start(server_host)) {
        baidu::common::LOG(baidu::WARNING, "Rpc Server Start failed");
        return RPC_START_FAIL;
    } 
    signal(SIGTERM, StopSigHandler);
    signal(SIGINT, StopSigHandler);
    signal(SIGUSR1, RestartSigHandler);

    while (!s_is_stop && !s_is_restart) {
        sleep(5); 
    }

    if (s_is_restart) {
        rpc_server.Stop(); 
        if (!DumpInitdCheckpoint(initd_service)) {
            baidu::common::LOG(baidu::WARNING, "Dump Initd Checkpoint failed"); 
            return EXIT_FAILURE;
        }
        restart_argv[restart_argc] = NULL;
        ::execvp(restart_argv[0], restart_argv);
        baidu::common::LOG(baidu::WARNING, "execvp failed err[%d: %s]",
                errno, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
