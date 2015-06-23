// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: sunjunyi01@baidu.com
#include <sofa/pbrpc/pbrpc.h>
#include <stdio.h>
#include <signal.h>
#include <string>
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "proto/agent.pb.h"
#include "rpc/rpc_client.h"
#include "agent/agent_impl.h"

DECLARE_string(agent_port);


TEST(Agent, SetPassword) {
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    FLAGS_agent_port = "8299";
    galaxy::AgentImpl* agent_service = new galaxy::AgentImpl();

    EXPECT_EQ(rpc_server.RegisterService(agent_service), true);

    std::string server_host = std::string("0.0.0.0:") + FLAGS_agent_port;
    EXPECT_EQ(rpc_server.Start(server_host), true);
    printf("=======Mock Agent Started============\n");
    galaxy::RpcClient* rpc_client = new galaxy::RpcClient();  
    galaxy::Agent_Stub* stub = NULL;
    rpc_client->GetStub("127.0.0.1:" + FLAGS_agent_port, &stub);
    if (stub) {
        galaxy::SetPasswordRequest request;
        galaxy::SetPasswordResponse response;
        request.set_user_name("hello");
        request.set_password("123");
        bool ok = rpc_client->SendRequest(stub, &galaxy::Agent_Stub::SetPassword,
                                          &request, &response, 5, 2);
        EXPECT_EQ(ok, true);
        EXPECT_EQ(response.status(), 0);
    }
    delete stub;
    delete rpc_client;
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

