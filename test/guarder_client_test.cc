// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "gflags/gflags.h"
#include "sofa/pbrpc/pbrpc.h"
#include "proto/guarder.pb.h"
#include "rpc/rpc_client.h"

DEFINE_string(guarder_addr, "localhost:9876", "guarder rpc-server endpoint");

DEFINE_int32(kill_signal, 9, "kill signal");
DEFINE_string(run_cmd, "", "command line");
DEFINE_string(pwd, "", "command run path");
DEFINE_string(envs, "", "command run envs");
DEFINE_string(user, "", "command run user");
DEFINE_string(cgroup, "", "command attach cgroup");
DEFINE_string(process_id, "", "command process id");
DEFINE_string(method, "", "guarder method name");


void RunProcess();
void StatusProcess();
void KillProcess();
void RemoveProcessState();
void CheckProcess();

void CheckProcess() {
    galaxy::Guarder_Stub* guarder;
    galaxy::RpcClient* rpc_client = new galaxy::RpcClient();
    rpc_client->GetStub(FLAGS_guarder_addr, &guarder);
    galaxy::CheckProcessRequest request;
    galaxy::CheckProcessResponse response;

    request.set_process_id(FLAGS_process_id);
    bool ret = rpc_client->SendRequest(guarder,
                            &galaxy::Guarder_Stub::CheckProcess,
                            &request, &response, 5, 1);
    if (!ret
            || (response.has_status()
                && response.status() != 0)) {
        fprintf(stderr, "check process failed\n"); 
    } else {
        fprintf(stdout, "check process status : %d exit_code : %d\n",
                response.process_state(),
                response.process_exit_code()); 
    } 
    delete rpc_client;
}

void RemoveProcessState() {
    galaxy::Guarder_Stub* guarder;
    galaxy::RpcClient* rpc_client = new galaxy::RpcClient();
    rpc_client->GetStub(FLAGS_guarder_addr, &guarder);
    galaxy::RemoveProcessStateRequest request; 
    galaxy::RemoveProcessStateResponse response;

    request.set_process_id(FLAGS_process_id);

    bool ret = rpc_client->SendRequest(guarder,
                            &galaxy::Guarder_Stub::RemoveProcessState,
                            &request, &response, 5, 1);
    if (!ret
            || (response.has_status()
                && response.status() != 0)) {
        fprintf(stderr, "remove status failed \n"); 
    } else {
        fprintf(stdout, "remove status success\n"); 
    } 
    delete rpc_client;
}

void KillProcess() {
    galaxy::Guarder_Stub* guarder;
    galaxy::RpcClient* rpc_client = new galaxy::RpcClient();
    rpc_client->GetStub(FLAGS_guarder_addr, &guarder);
    galaxy::KillProcessRequest request;
    galaxy::KillProcessResponse response;
    
    request.set_process_id(FLAGS_process_id);
    request.set_signal(FLAGS_kill_signal);

    int ret = rpc_client->SendRequest(guarder,
                            &galaxy::Guarder_Stub::KillProcess,
                            &request, &response, 5, 1);
    if (ret != 0
            || (response.has_status() &&
                response.status() != 0)) {
        fprintf(stderr, "kill process failed\n");     
    } else {
        fprintf(stdout, "kill process success\n"); 
    }
    delete rpc_client;
}

void StatusProcess() {
    galaxy::Guarder_Stub* guarder;
    galaxy::RpcClient* rpc_client = new galaxy::RpcClient();
    rpc_client->GetStub(FLAGS_guarder_addr, &guarder);
    galaxy::StatusProcessRequest request;
    galaxy::StatusProcessResponse response;

    request.set_process_id(FLAGS_process_id);
    
    int ret = rpc_client->SendRequest(guarder,
                            &galaxy::Guarder_Stub::StatusProcess,
                            &request, &response, 5, 1);
    if (ret != 0) {
        fprintf(stderr, "run process failed\n"); 
    } else {
        for (int i = 0; i < response.status_size(); i++) {
            fprintf(stdout, "%s\t%d\t%d\t%d\t%d\n",
                    response.status(i).process_id().c_str(),
                    response.status(i).pid(),
                    response.status(i).gpid(),
                    response.status(i).state(),
                    response.status(i).ret_code()); 
        }
    }
    delete rpc_client;
}

void RunProcess() {
    galaxy::Guarder_Stub* guarder;
    galaxy::RpcClient* rpc_client = new galaxy::RpcClient();
    rpc_client->GetStub(FLAGS_guarder_addr, &guarder);
    galaxy::RunProcessRequest request; 
    galaxy::RunProcessResponse response;

    request.set_start_cmd(FLAGS_run_cmd);
    request.set_pwd(FLAGS_pwd);
    request.set_user(FLAGS_user);
    request.set_cgroup_path(FLAGS_cgroup);
    //request.set_envs(0, FLAGS_envs);
    bool ret  = rpc_client->SendRequest(guarder, 
                            &galaxy::Guarder_Stub::RunProcess, 
                            &request, &response, 5, 1);
    if (!ret 
            || (response.has_status()
                 && response.status() != 0)) {
        fprintf(stderr, "run process failed ret %d\n", ret); 
        if (response.has_status()) {
            fprintf(stderr, "run process failed by %s\n", 
                    galaxy::GuarderExecuteState_Name(
                        galaxy::GuarderExecuteState(response.status())).c_str()); 
        }
    } else {
        fprintf(stdout, "%s\t%d\t%d\t%d\n",
                response.process_id().c_str(),
                response.status(),
                response.pid(),
                response.gpid());     
    }
    delete rpc_client;
}

int main(int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);    
    if (FLAGS_method == "RunProcess") {
        RunProcess(); 
    } else if (FLAGS_method == "StatusProcess") {
        StatusProcess(); 
    } else if (FLAGS_method == "KillProcess") {
        KillProcess(); 
    } else if (FLAGS_method == "RemoveProcessState") {
        RemoveProcessState(); 
    } else if (FLAGS_method == "CheckProcess") {
        CheckProcess(); 
    } else {
        fprintf(stderr, "method %s not support\n", FLAGS_method.c_str()); 
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
