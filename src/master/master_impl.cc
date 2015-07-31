// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "master_impl.h"

namespace baidu {
namespace galaxy {

void MasterImpl::SubmitJob(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::SubmitJobRequest*,
                           ::baidu::galaxy::SubmitJobResponse*,
                           ::google::protobuf::Closure* done) {
    controller->SetFailed("Method SubmitJob() not implemented.");
    done->Run();
}

void MasterImpl::UpdateJob(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::UpdateJobRequest*,
                           ::baidu::galaxy::UpdateJobResponse*,
                           ::google::protobuf::Closure* done) {
    controller->SetFailed("Method UpdateJob() not implemented.");
    done->Run();
}

void MasterImpl::SuspendJob(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::SuspendJobRequest*,
                            ::baidu::galaxy::SuspendJobResponse*,
                            ::google::protobuf::Closure* done) {
    controller->SetFailed("Method SuspendJob() not implemented.");
    done->Run();
}

void MasterImpl::ResumeJob(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::ResumeJobRequest*,
                           ::baidu::galaxy::ResumeJobResponse*,
                           ::google::protobuf::Closure* done) {
    controller->SetFailed("Method ResumeJob() not implemented.");
    done->Run();
}

void MasterImpl::TerminateJob(::google::protobuf::RpcController* controller,
                              const ::baidu::galaxy::TerminateJobRequest*,
                              ::baidu::galaxy::TerminateJobResponse*,
                              ::google::protobuf::Closure* done) {
    controller->SetFailed("Method TerminateJob() not implemented.");
    done->Run();
}

void MasterImpl::ShowJob(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::ShowJobRequest*,
                         ::baidu::galaxy::ShowJobResponse*,
                         ::google::protobuf::Closure* done) {
    controller->SetFailed("Method ShowJob() not implemented.");
    done->Run();
}

void MasterImpl::ListJobs(::google::protobuf::RpcController* controller,
                          const ::baidu::galaxy::ListJobsRequest*,
                          ::baidu::galaxy::ListJobsResponse*,
                          ::google::protobuf::Closure* done) {
    controller->SetFailed("Method ListJobs() not implemented.");
    done->Run();
}

void MasterImpl::HeartBeat(::google::protobuf::RpcController* controller,
                          const ::baidu::galaxy::HeartBeatRequest*,
                          ::baidu::galaxy::HeartBeatResponse*,
                          ::google::protobuf::Closure* done) {
    controller->SetFailed("Method HeartBeat() not implemented.");
    done->Run();
}

void MasterImpl::GetPendingJobs(::google::protobuf::RpcController* controller,
                                const ::baidu::galaxy::GetPendingJobsRequest*,
                                ::baidu::galaxy::GetPendingJobsResponse*,
                                ::google::protobuf::Closure* done) {
    controller->SetFailed("Method GetPendingJobs() not implemented.");
    done->Run();
}

void MasterImpl::GetResourceSnapshot(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::GetResourceSnapshotRequest*,
                         ::baidu::galaxy::GetResourceSnapshotResponse*,
                         ::google::protobuf::Closure* done) {
    controller->SetFailed("Method GetResourceSnapshot() not implemented.");
    done->Run();
}

void MasterImpl::Propose(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::ProposeRequest*,
                         ::baidu::galaxy::ProposeResponse*,
                         ::google::protobuf::Closure* done) {
    controller->SetFailed("Method Propose() not implemented.");
    done->Run();
}

void MasterImpl::ListAgents(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::ListAgentsRequest*,
                            ::baidu::galaxy::ListAgentsResponse*,
                            ::google::protobuf::Closure* done) {
    controller->SetFailed("Method ListAgents() not implemented.");
    done->Run();
}

}
}
