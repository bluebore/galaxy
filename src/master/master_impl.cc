// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "master_impl.h"
#include <gflags/gflags.h>
#include "master_util.h"
#include <logging.h>

DECLARE_string(nexus_servers);
DECLARE_string(nexus_root_path);
DECLARE_string(master_lock_path);
DECLARE_string(master_path);
DECLARE_string(jobs_store_path);
DECLARE_string(labels_store_path);

namespace baidu {
namespace galaxy {

const std::string LABEL_PREFIX = "LABEL_";

MasterImpl::MasterImpl() : nexus_(NULL) {
    nexus_ = new InsSDK(FLAGS_nexus_servers);
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
    JobId job_id = MasterUtil::GenerateJobId(job_desc);
    JobState state = kJobNormal;
    bool save_ok = SaveJobInfo(job_id, &job_desc, &state);
    if (!save_ok) {
        response->set_status(kJobSubmitFail);
        done->Run();
        return;
    }
    job_manager_.Add(job_id, job_desc);
    response->set_jobid(job_id);
    done->Run();
}

void MasterImpl::UpdateJob(::google::protobuf::RpcController* /*controller*/,
                           const ::baidu::galaxy::UpdateJobRequest* request,
                           ::baidu::galaxy::UpdateJobResponse* response,
                           ::google::protobuf::Closure* done) {
    const JobDescriptor& job_desc = request->job();
    JobId job_id = request->jobid();
    LOG(INFO, "update job desc: %s", job_desc.name().c_str());
    MasterUtil::TraceJobDesc(job_desc);
    std::string job_key = FLAGS_nexus_root_path + FLAGS_jobs_store_path 
                          + "/" + job_id;
    ::galaxy::ins::sdk::SDKError err;
    std::string job_raw_data;
    bool get_ok = nexus_->Get(job_key, &job_raw_data, &err);
    if (!get_ok) {
        LOG(WARNING, "no such job: %s", job_key.c_str());
        response->set_status(kJobNotFound);
        done->Run();
        return;
    }
    JobInfo job_info;
    bool parse_ok = job_info.ParseFromString(job_raw_data);
    if (!parse_ok) {
        LOG(WARNING, "parse old jobinfo failed, %s", job_id.c_str());
        response->set_status(kJobUpdateFail);
        done->Run();
        return;
    }
    bool save_ok = SaveJobInfo(job_id, &job_desc, NULL);
    if (!save_ok) {
        response->set_status(kJobUpdateFail);
        done->Run();
        return;
    }
    Status status = job_manager_.Update(job_id, job_desc);
    response->set_status(status);
    done->Run();
}

void MasterImpl::SuspendJob(::google::protobuf::RpcController* /*controller*/,
                            const ::baidu::galaxy::SuspendJobRequest* request,
                            ::baidu::galaxy::SuspendJobResponse* response,
                            ::google::protobuf::Closure* done) {
    JobState state = kJobSuspend;
    bool save_ok = SaveJobInfo(request->jobid(), NULL, &state);
    if (!save_ok) {
        response->set_status(kJobUpdateFail);
        done->Run();
        return;
    }
    response->set_status(job_manager_.Suspend(request->jobid()));
    done->Run();
}

void MasterImpl::ResumeJob(::google::protobuf::RpcController* /*controller*/,
                           const ::baidu::galaxy::ResumeJobRequest* request,
                           ::baidu::galaxy::ResumeJobResponse* response,
                           ::google::protobuf::Closure* done) {
    JobState state = kJobNormal;
    bool save_ok = SaveJobInfo(request->jobid(), NULL, &state);
    if (!save_ok) {
        response->set_status(kJobUpdateFail);
        done->Run();
        return;
    }
    response->set_status(job_manager_.Resume(request->jobid()));
    done->Run();
}

void MasterImpl::TerminateJob(::google::protobuf::RpcController* controller,
                              const ::baidu::galaxy::TerminateJobRequest*,
                              ::baidu::galaxy::TerminateJobResponse*,
                              ::google::protobuf::Closure* done) {
    controller->SetFailed("Method TerminateJob() not implemented.");
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

void MasterImpl::GetPendingJobs(::google::protobuf::RpcController* /*controller*/,
                                const ::baidu::galaxy::GetPendingJobsRequest*,
                                ::baidu::galaxy::GetPendingJobsResponse* response,
                                ::google::protobuf::Closure* done) {
    job_manager_.GetPendingPods(response->mutable_scale_up_jobs());
    response->set_status(kOk);
    done->Run();
}

void MasterImpl::GetResourceSnapshot(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::GetResourceSnapshotRequest* request,
                         ::baidu::galaxy::GetResourceSnapshotResponse* response,
                         ::google::protobuf::Closure* done) {
    job_manager_.GetAliveAgentsByDiff(request->versions(),
                                      response->mutable_agents(), 
                                      response->mutable_deleted_agents());
    response->set_status(kOk);
    LOG(INFO, "get response snapshot whose bytes size is %d ", response->ByteSize());
    done->Run();
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
    job_manager_.DeployPod();
}

void MasterImpl::ListAgents(::google::protobuf::RpcController* /*controller*/,
                            const ::baidu::galaxy::ListAgentsRequest* /*request*/,
                            ::baidu::galaxy::ListAgentsResponse* response,
                            ::google::protobuf::Closure* done) {
    job_manager_.GetAgentsInfo(response->mutable_agents());
    response->set_status(kOk);
    done->Run();
}

bool MasterImpl::SaveJobInfo(const JobId& job_id,
                             const JobDescriptor* desc,
                             const JobState* state) {
    JobInfo job_info;
    Status status = job_manager_.GetJobInfo(job_id, &job_info);
    if (status == kJobNotFound) {
        job_info.set_state(kJobNormal);
        job_info.set_jobid(job_id);
    }
    if (desc != NULL) {
        job_info.mutable_desc()->CopyFrom(*desc);
    }
    if (state != NULL) {
        job_info.set_state(*state);
    }
    std::string job_raw_data;
    job_info.SerializeToString(&job_raw_data);
    std::string job_key = FLAGS_nexus_root_path + FLAGS_jobs_store_path 
                          + "/" + job_id;
    ::galaxy::ins::sdk::SDKError err;
    bool put_ok = nexus_->Put(job_key, job_raw_data, &err);
    if (!put_ok) {
        LOG(FATAL, "fail to save job %s for %s", job_id.c_str(),
          ::galaxy::ins::sdk::InsSDK::StatusToString(err).c_str());
    }
    return put_ok;
}

void MasterImpl::LabelAgents(::google::protobuf::RpcController* controller,
                             const ::baidu::galaxy::LabelAgentRequest* request,
                             ::baidu::galaxy::LabelAgentResponse* response,
                             ::google::protobuf::Closure* done) {
    std::string label_key = FLAGS_nexus_root_path + FLAGS_labels_store_path 
                            + "/" + request->labels().label();
    std::string label_value;
    if (!request->labels().SerializeToString(&label_value)) {
        LOG(WARNING, "label %s serialize failed", 
                request->labels().label().c_str()); 
        response->set_status(kUnknown);
        done->Run();
        return;
    }
    // TODO lock ?
    ::galaxy::ins::sdk::SDKError err;
    bool put_ok = nexus_->Put(label_key, label_value, &err);    
    if (!put_ok) {
        response->set_status(kPersistenceError); 
        LOG(WARNING, "persisten label info to nexus fail, reason: %s",
                ::galaxy::ins::sdk::InsSDK::StatusToString(err).c_str());
        done->Run();
        return;
    }

    Status status = job_manager_.LabelAgents(request->labels());
    response->set_status(status);
    done->Run();
    return;
}

}
}
