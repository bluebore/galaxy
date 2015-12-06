// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "master_impl.h"
#include <gflags/gflags.h>
#include "master_util.h"
#include <logging.h>
#include <sofa/pbrpc/pbrpc.h>

DECLARE_string(nexus_servers);
DECLARE_string(nexus_root_path);
DECLARE_string(master_lock_path);
DECLARE_string(master_path);
DECLARE_string(jobs_store_path);
DECLARE_string(labels_store_path);
DECLARE_int32(max_scale_down_size);
DECLARE_int32(max_scale_up_size);
DECLARE_int32(max_need_update_job_size);

namespace baidu {
namespace galaxy {

const std::string LABEL_PREFIX = "LABEL_";

MasterImpl::MasterImpl() : nexus_(NULL){
    nexus_ = new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_servers);
}

MasterImpl::~MasterImpl() {
    delete nexus_;
}

static void OnMasterLockChange(const ::galaxy::ins::sdk::WatchParam& param, 
                               ::galaxy::ins::sdk::SDKError /*error*/) {
    MasterImpl* master = static_cast<MasterImpl*>(param.context);
    master->OnLockChange(param.value);
}

static void OnMasterSessionTimeout(void* ctx) {
    MasterImpl* master = static_cast<MasterImpl*>(ctx);
    master->OnSessionTimeout();
}

void MasterImpl::OnLockChange(std::string lock_session_id) {
    std::string self_session_id = nexus_->GetSessionID();
    if (self_session_id != lock_session_id) {
        LOG(FATAL, "master lost lock , die.");
        abort();
    }
}

void MasterImpl::OnSessionTimeout() {
    LOG(FATAL, "master lost session with nexus, die.");
    abort();
}

void MasterImpl::Init() {
    AcquireMasterLock();
    LOG(INFO, "begin to reload job descriptor from nexus");
    ReloadJobInfo();
    ReloadLabelInfo();
}

void MasterImpl::ReloadLabelInfo() {
    std::string start_key = FLAGS_nexus_root_path + FLAGS_labels_store_path + "/";
    std::string end_key = start_key + "~";
    ::galaxy::ins::sdk::ScanResult* result = nexus_->Scan(start_key, end_key);
    int label_amount = 0;
    while (!result->Done()) {
        assert(result->Error() == ::galaxy::ins::sdk::kOK);
        std::string key = result->Key();
        std::string label_raw_data = result->Value();
        LabelCell label;
        bool ok = label.ParseFromString(label_raw_data);
        if (ok) {
            LOG(INFO, "reload label: %s", label.label().c_str()); 
            job_manager_.LabelAgents(label);
        } else {
            LOG(WARNING, "faild to parse label: %s", key.c_str());
        }
        result->Next();
        label_amount ++;
    }
    LOG(INFO, "reload label info %d", label_amount);
    return;
}

void MasterImpl::ReloadJobInfo() {
    std::string start_key = FLAGS_nexus_root_path + FLAGS_jobs_store_path + "/";
    std::string end_key = start_key + "~";
    ::galaxy::ins::sdk::ScanResult* result = nexus_->Scan(start_key, end_key);
    int job_amount = 0;
    while (!result->Done()) {
        assert(result->Error() == ::galaxy::ins::sdk::kOK);
        std::string key = result->Key();
        std::string job_raw_data = result->Value(); 
        JobInfo job_info;
        bool ok = job_info.ParseFromString(job_raw_data);
        if (ok) {
            LOG(INFO, "reload job: %s", job_info.jobid().c_str());
            job_manager_.ReloadJobInfo(job_info);
        } else {
            LOG(WARNING, "faild to parse job_info: %s", key.c_str());
        }
        result->Next();
        job_amount ++;
    }
    LOG(INFO, "reload all job desc finish, total#: %d", job_amount);
}

void MasterImpl::AcquireMasterLock() {
    std::string master_lock = FLAGS_nexus_root_path + FLAGS_master_lock_path;
    ::galaxy::ins::sdk::SDKError err;
    nexus_->RegisterSessionTimeout(&OnMasterSessionTimeout, this);
    bool ret = nexus_->Lock(master_lock, &err); //whould block until accquired
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    std::string master_endpoint = MasterUtil::SelfEndpoint();
    std::string master_path_key = FLAGS_nexus_root_path + FLAGS_master_path;
    ret = nexus_->Put(master_path_key, master_endpoint, &err);
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    ret = nexus_->Watch(master_lock, &OnMasterLockChange, this, &err);
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    LOG(INFO, "master lock [ok].  %s -> %s", 
        master_path_key.c_str(), master_endpoint.c_str());
}

void MasterImpl::SubmitJob(::google::protobuf::RpcController* /*controller*/,
                           const ::baidu::galaxy::SubmitJobRequest* request,
                           ::baidu::galaxy::SubmitJobResponse* response,
                           ::google::protobuf::Closure* done) {
    const JobDescriptor& job_desc = request->job();
    MasterUtil::TraceJobDesc(job_desc);
    JobId job_id = MasterUtil::UUID();
    Status status = job_manager_.Add(job_id, job_desc);
    response->set_status(status);
    if (status == kOk) {
        response->set_jobid(job_id);
    }
    done->Run();
}

void MasterImpl::UpdateJob(::google::protobuf::RpcController* /*controller*/,
                           const ::baidu::galaxy::UpdateJobRequest* request,
                           ::baidu::galaxy::UpdateJobResponse* response,
                           ::google::protobuf::Closure* done) {
    JobId job_id = request->jobid();
    LOG(INFO, "update job %s replica %d", job_id.c_str(), request->job().replica());
    Status status = job_manager_.Update(job_id, request->job());
    response->set_status(status);
    done->Run();
}

void MasterImpl::SuspendJob(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::SuspendJobRequest* /*request*/,
                            ::baidu::galaxy::SuspendJobResponse* /*response*/,
                            ::google::protobuf::Closure* done) {
    controller->SetFailed("Method TerminateJob() not implemented.");
    done->Run();
}

void MasterImpl::ResumeJob(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::ResumeJobRequest* /*request*/,
                           ::baidu::galaxy::ResumeJobResponse* /*response*/,
                           ::google::protobuf::Closure* done) { 
    controller->SetFailed("Method TerminateJob() not implemented.");
    done->Run();
}

void MasterImpl::TerminateJob(::google::protobuf::RpcController* ,
                              const ::baidu::galaxy::TerminateJobRequest* request,
                              ::baidu::galaxy::TerminateJobResponse* response,
                              ::google::protobuf::Closure* done) {
    JobId job_id = request->jobid();
    LOG(INFO, "terminate job %s", job_id.c_str());
    Status status= job_manager_.Terminte(job_id);
    response->set_status(status);
    done->Run();
}

void MasterImpl::ShowJob(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::ShowJobRequest* request,
                         ::baidu::galaxy::ShowJobResponse* response,
                         ::google::protobuf::Closure* done) {
    for (int32_t i = 0; i < request->jobsid_size(); i++) {
        const JobId& jobid = request->jobsid(i);
        if (kOk != job_manager_.GetJobInfo(jobid, response->mutable_jobs()->Add())) {
            response->mutable_jobs()->RemoveLast();
        }
    }
    response->set_status(kOk);
    done->Run();
}

void MasterImpl::ListJobs(::google::protobuf::RpcController* /*controller*/,
                          const ::baidu::galaxy::ListJobsRequest* /*request*/,
                          ::baidu::galaxy::ListJobsResponse* response,
                          ::google::protobuf::Closure* done) {
    job_manager_.GetJobsOverview(response->mutable_jobs());
    response->set_status(kOk);
    done->Run();
}

void MasterImpl::HeartBeat(::google::protobuf::RpcController* /*controller*/,
                          const ::baidu::galaxy::HeartBeatRequest* request,
                          ::baidu::galaxy::HeartBeatResponse*,
                          ::google::protobuf::Closure* done) {
    std::string agent_addr = request->endpoint();
    job_manager_.KeepAlive(agent_addr);
    done->Run();
}

void MasterImpl::SwitchSafeMode(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::SwitchSafeModeRequest* request,
                           ::baidu::galaxy::SwitchSafeModeResponse* response,
                           ::google::protobuf::Closure* done) {
    Status ok = job_manager_.SetSafeMode(request->enter_or_leave());
    response->set_status(ok);
    done->Run();
}

void MasterImpl::GetPendingJobs(::google::protobuf::RpcController* controller,
                                const ::baidu::galaxy::GetPendingJobsRequest* request,
                                ::baidu::galaxy::GetPendingJobsResponse* response,
                                ::google::protobuf::Closure* done) {
    int32_t max_scale_up_size = FLAGS_max_scale_up_size;
    int32_t max_scale_down_size = FLAGS_max_scale_down_size;
    int32_t max_need_update_job_size = FLAGS_max_need_update_job_size;
    if (request->has_max_scale_up_size() 
        && request->max_scale_up_size() > 0) {
        max_scale_up_size = request->max_scale_up_size();
    }
    if (request->has_max_scale_down_size()
        && request->max_scale_down_size() > 0) {
        max_scale_down_size = request->max_scale_down_size();
    }
    if (request->has_max_need_update_job_size()
        && request->max_need_update_job_size() > 0) {
        max_need_update_job_size = request->max_need_update_job_size();
    }

    sofa::pbrpc::RpcController* sf_ctrl = (sofa::pbrpc::RpcController*) controller;
    response->set_status(kOk);
    LOG(INFO, "sched request from %s", sf_ctrl->RemoteAddress().c_str());
    job_manager_.GetPendingPods(response->mutable_scale_up_jobs(),
                                max_scale_up_size,
                                response->mutable_scale_down_jobs(),
                                max_scale_down_size,
                                response->mutable_need_update_jobs(),
                                max_need_update_job_size,
                                done);
}

void MasterImpl::GetResourceSnapshot(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::GetResourceSnapshotRequest* request,
                         ::baidu::galaxy::GetResourceSnapshotResponse* response,
                         ::google::protobuf::Closure* done) {
    response->set_status(kOk);
    job_manager_.GetAliveAgentsByDiff(request->versions(),
                                      response->mutable_agents(), 
                                      response->mutable_deleted_agents(),
                                      done);
}

void MasterImpl::Propose(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::ProposeRequest* request,
                         ::baidu::galaxy::ProposeResponse* response,
                         ::google::protobuf::Closure* done) {
    for (int i = 0; i < request->schedule_size(); i++) {
        const ScheduleInfo& sche_info = request->schedule(i);
        response->set_status(job_manager_.Propose(sche_info));
    }
    done->Run();
}


void MasterImpl::ListAgents(::google::protobuf::RpcController* /*controller*/,
                            const ::baidu::galaxy::ListAgentsRequest* /*request*/,
                            ::baidu::galaxy::ListAgentsResponse* response,
                            ::google::protobuf::Closure* done) {
    job_manager_.GetAgentsInfo(response->mutable_agents());
    response->set_status(kOk);
    done->Run();
}


void MasterImpl::LabelAgents(::google::protobuf::RpcController* ,
                             const ::baidu::galaxy::LabelAgentRequest* request,
                             ::baidu::galaxy::LabelAgentResponse* response,
                             ::google::protobuf::Closure* done) { 
    Status status = job_manager_.LabelAgents(request->labels());
    response->set_status(status);
    done->Run();
    return;
}

void MasterImpl::ShowPod(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::ShowPodRequest* request,
                         ::baidu::galaxy::ShowPodResponse* response,
                         ::google::protobuf::Closure* done) {
    Status ok = job_manager_.GetPods(request->jobid(), 
                                     response->mutable_pods());
    response->set_status(ok);
    done->Run();
}

void MasterImpl::GetStatus(::google::protobuf::RpcController*,
                           const ::baidu::galaxy::GetMasterStatusRequest* ,
                           ::baidu::galaxy::GetMasterStatusResponse* response,
                           ::google::protobuf::Closure* done) {
    Status ok = job_manager_.GetStatus(response);
    response->set_status(ok);
    done->Run();
}

}
}
