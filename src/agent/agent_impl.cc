// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent_impl.h"
#include <string>
#include <sstream>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <sys/utsname.h>
#include <gflags/gflags.h>
#include <boost/bind.hpp>
#include <snappy.h>

namespace baidu {
namespace galaxy {

AgentImpl::AgentImpl() {
}

AgentImpl::~AgentImpl() {
}

void AgentImpl::CreateContainer(::google::protobuf::RpcController* controller,
        const ::baidu::galaxy::proto::CreateContainerRequest* request,
        ::baidu::galaxy::proto::CreateContainerResponse* response,
        ::google::protobuf::Closure* done) {
    //
}


void AgentImpl::RemoveContainer(::google::protobuf::RpcController* controller,
        const ::baidu::galaxy::proto::RemoveContainerRequest* request,
        ::baidu::galaxy::proto::RemoveContainerResponse* response,
        ::google::protobuf::Closure* done) {
}


void AgentImpl::ListContainers(::google::protobuf::RpcController* controller,
        const ::baidu::galaxy::proto::ListContainersRequest* request,
        ::baidu::galaxy::proto::ListContainersResponse* response,
        ::google::protobuf::Closure* done) {
}


void AgentImpl::Query(::google::protobuf::RpcController* controller,
        const ::baidu::galaxy::proto::QueryRequest* request,
        ::baidu::galaxy::proto::QueryResponse* response,
        ::google::protobuf::Closure* done) {
}


}
}
