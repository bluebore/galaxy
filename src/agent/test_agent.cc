// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include "gflags/gflags.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include <google/protobuf/text_format.h>

#include "proto/agent.pb.h"
#include "proto/galaxy.pb.h"
#include "rpc/rpc_client.h"

DECLARE_string(agent_port);
DEFINE_string(pod_bin, "", "pod binary");
DEFINE_string(pod_start_command, "", "start command");

int main (int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);

    baidu::galaxy::Agent_Stub* agent;
    baidu::galaxy::RpcClient* rpc_client =
        new baidu::galaxy::RpcClient();
    std::string agent_endpoint = "127.0.0.1:";
    agent_endpoint.append(FLAGS_agent_port);
    rpc_client->GetStub(agent_endpoint.c_str(), &agent);

    // build desc
    baidu::galaxy::TaskDescriptor task_desc;

    if (boost::starts_with(FLAGS_pod_bin, "ftp://")) {
        task_desc.set_binary(FLAGS_pod_bin);
        task_desc.set_source_type(::baidu::galaxy::kSourceTypeFTP);
    } else {
        FILE* fp = ::fopen(FLAGS_pod_bin.c_str(), "r"); 
        if (fp == NULL) {
            fprintf(stderr, "open %s for read failed err[%d: %s]\n",
                    FLAGS_pod_bin.c_str(), errno, strerror(errno)); 
            return -1;
        }
        std::string task_raw;
        char buf[1024];
        int len = 0;
        while ((len = ::fread(buf, 1, 1024, fp)) > 0) {
            task_raw.append(buf, len); 
        }
        ::fclose(fp);
        fprintf(stdout, "task binary len %lu\n", task_raw.size()); 
        task_desc.set_binary(task_raw);
        task_desc.set_source_type(::baidu::galaxy::kSourceTypeBinary);
    }

    task_desc.set_start_command(FLAGS_pod_start_command);
    baidu::galaxy::PodDescriptor pod_desc;
    baidu::galaxy::TaskDescriptor* task = pod_desc.add_tasks();
    task->CopyFrom(task_desc);

    // run pod
    baidu::galaxy::RunPodRequest request;
    baidu::galaxy::RunPodResponse response;
    request.set_podid("hehe_test");
    request.mutable_pod()->CopyFrom(pod_desc);
    bool ret = rpc_client->SendRequest(agent,
                &baidu::galaxy::Agent_Stub::RunPod,
                &request, &response, 5, 1);
    if (!ret) {
        fprintf(stderr, "rpc failed\n");
    } else if (response.has_status() 
            && response.status() != baidu::galaxy::kOk) {
        fprintf(stderr, "agent error %s\n", 
                baidu::galaxy::Status_Name(response.status()).c_str()); 
    } else {
        fprintf(stdout, "run pod success\n"); 
    }

    sleep(10);
    // query pod 
    baidu::galaxy::QueryRequest q_request;
    baidu::galaxy::QueryResponse q_response;
    std::string pb_str;
    ret = rpc_client->SendRequest(agent,
            &baidu::galaxy::Agent_Stub::Query,
            &q_request, &q_response, 5, 1);
    if (!ret) {
        fprintf(stderr, "rpc query failed\n"); 
    } else if (q_response.has_status()
            && q_response.status() != baidu::galaxy::kOk) {
        fprintf(stderr, "agent error %s\n",
                baidu::galaxy::Status_Name(q_response.status()).c_str()); 
    } else {
        google::protobuf::TextFormat::PrintToString(q_response, &pb_str);
        fprintf(stdout, "agent resp %s\n", pb_str.c_str());
    }
    sleep(10);
    baidu::galaxy::KillPodRequest k_request;
    baidu::galaxy::KillPodResponse k_response;
    k_request.set_podid("hehe_test");
    ret = rpc_client->SendRequest(agent,
            &baidu::galaxy::Agent_Stub::KillPod,
            &k_request, &k_response, 5, 1);
    if (!ret) {
        fprintf(stderr, "rpc query failed\n"); 
    } 
    fprintf(stderr, "agent error %s\n",
                baidu::galaxy::Status_Name(k_response.status()).c_str()); 
    sleep(10);
    ret = rpc_client->SendRequest(agent,
            &baidu::galaxy::Agent_Stub::Query,
            &q_request, &q_response, 5, 1);
    if (!ret) {
        fprintf(stderr, "rpc query failed\n"); 
    } else if (q_response.has_status()
            && q_response.status() != baidu::galaxy::kOk) {
        fprintf(stderr, "agent error %s\n",
                baidu::galaxy::Status_Name(q_response.status()).c_str()); 
    } else {
        google::protobuf::TextFormat::PrintToString(q_response, &pb_str);
        fprintf(stdout, "agent resp %s\n", pb_str.c_str());
    }
    return 0;
}
