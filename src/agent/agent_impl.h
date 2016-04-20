// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#include <string>
#include <map>
#include <set>

#include "src/protocol/agent.pb.h"

namespace baidu {
namespace galaxy {

class AgentImpl : public baidu::galaxy::proto::Agent {
public:

AgentImpl();
virtual ~AgentImpl();

void CreateContainer(::google::protobuf::RpcController* controller,
                     const ::baidu::galaxy::proto::CreateContainerRequest* request,
                     ::baidu::galaxy::proto::CreateContainerResponse* response,
                     ::google::protobuf::Closure* done);

void RemoveContainer(::google::protobuf::RpcController* controller,
                     const ::baidu::galaxy::proto::RemoveContainerRequest* request,
                     ::baidu::galaxy::proto::RemoveContainerResponse* response,
                     ::google::protobuf::Closure* done);

void ListContainers(::google::protobuf::RpcController* controller,
                    const ::baidu::galaxy::proto::ListContainersRequest* request,
                    ::baidu::galaxy::proto::ListContainersResponse* response,
                    ::google::protobuf::Closure* done);

void Query(::google::protobuf::RpcController* controller,
           const ::baidu::galaxy::proto::QueryRequest* request,
           ::baidu::galaxy::proto::QueryResponse* response,
           ::google::protobuf::Closure* done);



};

}
}
