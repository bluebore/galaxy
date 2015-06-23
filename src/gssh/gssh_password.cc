// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: sunjunyi01@baidu.com
#include <sofa/pbrpc/pbrpc.h>
#include <stdio.h>
#include <signal.h>
#include <string>
#include "proto/agent.pb.h"
#include "rpc/rpc_client.h"

void print_help() {
    printf("A tool to set galaxy's agent ssh password remotely.  Usage: \n");
    printf("   ./gssh_password [agent addr] [username] [password]\n");
    printf("example:\n");
    printf("    ./gssh_password yq01-spi-galaxy11.yq01.baidu.com:9527 sunjunyi01 123\n");
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_help();
        return 1;
    }
    std::string agent_addr = argv[1];
    std::string username = argv[2];
    std::string password = argv[3];
    galaxy::RpcClient* rpc_client = new galaxy::RpcClient();  
    galaxy::Agent_Stub* stub = NULL;
    rpc_client->GetStub(agent_addr, &stub);
    int done  = 0;
    if (stub) {
        galaxy::SetPasswordRequest request;
        galaxy::SetPasswordResponse response;
        request.set_user_name(username);
        request.set_password(password);
        bool ok = rpc_client->SendRequest(stub, &galaxy::Agent_Stub::SetPassword,
                                          &request, &response, 5, 2);
        if (!ok || response.status() != 0) {
            printf("set gssh password fail\n");
            done = -1;
        } else{
            printf("Done\n");
        }
    }
    delete stub;
    delete rpc_client;
    return done;
}
