// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include "proto/agent.pb.h"
#include "rpc/rpc_client.h"

int main (int argc, char* argv[]) {
    baidu::galaxy::Agent_Stub* agent;
    baidu::galaxy::RpcClient* rpc_client =
        new baidu::galaxy::RpcClient();
    rpc_client->GetStub("127.0.0.1:8080", &agent);
    baidu::galaxy::QueryRequest request;
    baidu::galaxy::QueryResponse response;
    bool ret = rpc_client->SendRequest(agent,
                                       &baidu::galaxy::Agent_Stub::Query,
                                       &request,
                                       &response, 5, 1);
    if (!ret) {
        fprintf(stderr, "Query agent failed\n"); 
        return -1;
    }
    fprintf(stdout, "Query agent %d", response.agent().total().millicores());

    baidu::galaxy::PodDescriptor pod;
    baidu::galaxy::TaskDescriptor task;

    task.set_start_command("touch testgced");

    baidu::galaxy::TaskDescriptor* temp_task = pod.add_tasks();
    temp_task->CopyFrom(task);
    ::baidu::galaxy::RunPodRequest run_req;
    ::baidu::galaxy::RunPodResponse run_resp;
    run_req.mutable_pod()->CopyFrom(pod);
    run_req.set_podid("hehe");
    ret = rpc_client->SendRequest(agent,
                                  &baidu::galaxy::Agent_Stub::RunPod,
                                  &run_req,
                                  &run_resp,
                                  5, 1);
    if (!ret) {
        fprintf(stderr, "run pod failed\n");
        return -1;
    }

    return 0;
}
