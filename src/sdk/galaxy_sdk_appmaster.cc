// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "galaxy_sdk_util.h"
#include "rpc/rpc_client.h"
#include "ins_sdk.h"
#include <gflags/gflags.h>
#include "protocol/galaxy.pb.h"
#include "protocol/appmaster.pb.h"
#include "galaxy_sdk_appmaster.h"

namespace baidu {
namespace galaxy {
namespace sdk {

AppMaster::~AppMaster() {
}

class AppMasterImpl : public AppMaster {
public:
    AppMasterImpl(const std::string& nexus_addr, const std::string& path) : rpc_client_(NULL),
                      appmaster_stub_(NULL) {
        full_key_ = path;
        rpc_client_ = new RpcClient();
        nexus_ = new ::galaxy::ins::sdk::InsSDK(nexus_addr);
    }

    virtual ~AppMasterImpl() {
        delete rpc_client_;
        if (NULL != appmaster_stub_) {
            delete appmaster_stub_;
        }
        delete nexus_;
    }

    bool GetStub();
    bool SubmitJob(const SubmitJobRequest& request, SubmitJobResponse* response);
    bool UpdateJob(const UpdateJobRequest& request, UpdateJobResponse* response);
    bool StopJob(const StopJobRequest& request, StopJobResponse* response);
    bool RemoveJob(const RemoveJobRequest& request, RemoveJobResponse* response);
    bool ListJobs(const ListJobsRequest& request, ListJobsResponse* response);
    bool ShowJob(const ShowJobRequest& request, ShowJobResponse* response);
    bool RecoverInstance(const RecoverInstanceRequest& request, RecoverInstanceResponse* response);
    bool ExecuteCmd(const ExecuteCmdRequest& request, ExecuteCmdResponse* response);
private:
    ::galaxy::ins::sdk::InsSDK* nexus_;
    ::baidu::galaxy::RpcClient* rpc_client_;
    ::baidu::galaxy::proto::AppMaster_Stub* appmaster_stub_;
    std::string full_key_;

};

bool AppMasterImpl::GetStub() {
    std::string endpoint;
    ::galaxy::ins::sdk::SDKError err;
    bool ok = nexus_->Get(full_key_, &endpoint, &err);
    if (!ok || err != ::galaxy::ins::sdk::kOK) {
        fprintf(stderr, "get appmaster endpoint from nexus failed: %s\n", 
                ::galaxy::ins::sdk::InsSDK::StatusToString(err).c_str());
        return false;
    }
    if(!rpc_client_->GetStub(endpoint, &appmaster_stub_)) {
        fprintf(stderr, "connect appmaster fail, appmaster: %s\n", endpoint.c_str());
        return false;
    }
    return true;
}

bool AppMasterImpl::SubmitJob(const SubmitJobRequest& request, SubmitJobResponse* response) {
    ::baidu::galaxy::proto::SubmitJobRequest pb_request;
    ::baidu::galaxy::proto::SubmitJobResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string hostname = Strim(request.hostname);
    if (hostname.empty()) {
        fprintf(stderr, "hostname must not be empty\n");
        return false;
    }
    pb_request.set_hostname(hostname);

    if (!FillJobDescription(request.job, pb_request.mutable_job())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(appmaster_stub_, 
                                        &::baidu::galaxy::proto::AppMaster_Stub::SubmitJob,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "AppMaster Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    response->jobid = pb_response.jobid();
    return true;
}

bool AppMasterImpl::UpdateJob(const UpdateJobRequest& request, UpdateJobResponse* response) {
    ::baidu::galaxy::proto::UpdateJobRequest pb_request;
    ::baidu::galaxy::proto::UpdateJobResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    
    std::string hostname = Strim(request.hostname);
    if (hostname.empty()) {
        fprintf(stderr, "hostname must not be empty\n");
        return false;
    }
    pb_request.set_hostname(hostname);

    std::string jobid = Strim(request.jobid);
    if (jobid.empty()) {
        fprintf(stderr, "jobid must not be empty\n");
        return false;
    }
    pb_request.set_jobid(jobid);

    if (request.operate == kUpdateJobStart) {
        if (request.job.deploy.update_break_count < 0U 
                || request.job.deploy.update_break_count > request.job.deploy.replica) {
            fprintf(stderr, "deploy update_break_count must be greater than 0 and less than replica\n");
            return false;
        }
        if (!FillJobDescription(request.job, pb_request.mutable_job())) {
            return false;
        }
        pb_request.mutable_job()->mutable_deploy()->set_update_break_count(request.job.deploy.update_break_count);
        pb_request.set_operate(::baidu::galaxy::proto::kUpdateJobStart);
    } else if (request.operate == kUpdateJobContinue) {
        if (request.update_break_count < 0U) {
            fprintf(stderr, "update_break_count must not be less than 0\n");
            return false;
        }
        pb_request.set_operate(::baidu::galaxy::proto::kUpdateJobContinue);
        pb_request.set_update_break_count(request.update_break_count);
    } else if (request.operate == kUpdateJobRollback) {
        pb_request.set_operate(::baidu::galaxy::proto::kUpdateJobRollback);
    } else if (request.operate == kUpdateJobPause) {
        pb_request.set_operate(::baidu::galaxy::proto::kUpdateJobPause);
    } else {
        fprintf(stderr, "update operation must be kUpdateJobStart, \
                    kUpdateJobContinue, kUpdateJobRollback, kUpdateJobPause\n");
        return false;
    }

    bool ok = rpc_client_->SendRequest(appmaster_stub_,
                                        &::baidu::galaxy::proto::AppMaster_Stub::UpdateJob,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "AppMaster Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    return true;
}

bool AppMasterImpl::StopJob(const StopJobRequest& request, StopJobResponse* response) {
    ::baidu::galaxy::proto::StopJobRequest pb_request;
    ::baidu::galaxy::proto::StopJobResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    
    std::string hostname = Strim(request.hostname);
    if (hostname.empty()) {
        fprintf(stderr, "hostname must not be empty\n");
        return false;
    }
    pb_request.set_hostname(hostname);

    std::string jobid = Strim(request.jobid);
    if (jobid.empty()) {
        fprintf(stderr, "jobid must not be empty\n");
        return false;
    }
    pb_request.set_jobid(jobid);

    bool ok = rpc_client_->SendRequest(appmaster_stub_,
                                        &::baidu::galaxy::proto::AppMaster_Stub::StopJob,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "AppMaster Rpc SendRequest failed";
        return false;
    }
    response->error_code.status = (Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool AppMasterImpl::RemoveJob(const RemoveJobRequest& request, RemoveJobResponse* response) {
    ::baidu::galaxy::proto::RemoveJobRequest pb_request;
    ::baidu::galaxy::proto::RemoveJobResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    
    std::string hostname = Strim(request.hostname);
    if (hostname.empty()) {
        fprintf(stderr, "hostname must not be empty\n");
        return false;
    }
    pb_request.set_hostname(hostname);

    std::string jobid = Strim(request.jobid);
    if (jobid.empty()) {
        fprintf(stderr, "jobid must not be empty\n");
        return false;
    }
    pb_request.set_jobid(jobid);

    bool ok = rpc_client_->SendRequest(appmaster_stub_, 
                                        &::baidu::galaxy::proto::AppMaster_Stub::RemoveJob,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "AppMaster Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool AppMasterImpl::ListJobs(const ListJobsRequest& request, ListJobsResponse* response) {
    ::baidu::galaxy::proto::ListJobsRequest pb_request;
    ::baidu::galaxy::proto::ListJobsResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(appmaster_stub_,
                                        &::baidu::galaxy::proto::AppMaster_Stub::ListJobs,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "Appmaster Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    for (int i = 0; i < pb_response.jobs().size(); ++i) {
        const ::baidu::galaxy::proto::JobOverview& pb_job = pb_response.jobs(i); 
        JobOverview job;
        job.jobid = pb_job.jobid();
        job.user = pb_job.user().user();
        job.running_num = pb_job.running_num();
        job.pending_num = pb_job.pending_num();
        job.deploying_num = pb_job.deploying_num();
        job.death_num = pb_job.death_num();
        job.fail_count = pb_job.fail_count();
        job.create_time = pb_job.create_time();
        job.update_time = pb_job.update_time();
        job.status = (JobStatus)pb_job.status();
        PbJobDescription2SdkJobDescription(pb_job.desc(), &job.desc);
        response->jobs.push_back(job);
    }
    return true;
}

bool AppMasterImpl::ShowJob(const ShowJobRequest& request, ShowJobResponse* response) {
    ::baidu::galaxy::proto::ShowJobRequest pb_request;
    ::baidu::galaxy::proto::ShowJobResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string jobid = Strim(request.jobid);
    if (jobid.empty()) {
        fprintf(stderr, "jobid must not be empty\n");
        return false;
    }
    pb_request.set_jobid(jobid);

    bool ok = rpc_client_->SendRequest(appmaster_stub_,
                                        &::baidu::galaxy::proto::AppMaster_Stub::ShowJob,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "AppMaster Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    response->job.jobid = pb_response.job().jobid();
    response->job.version = pb_response.job().version();
    response->job.create_time = pb_response.job().create_time();
    response->job.update_time = pb_response.job().update_time();
    response->job.status = (JobStatus)pb_response.job().status();
    response->job.user = pb_response.job().user().user();
    PbJobDescription2SdkJobDescription(pb_response.job().desc(), &response->job.desc);
    /*if (pb_response.Has_desc()) {
        PbJobDescription2SdkJobDescription(pb_response.job().last_desc(), &response->job.last_desc);
    }*/
    for (int32_t i = 0; i < pb_response.job().pods().size(); ++i) {
        PodInfo pod;
        pod.podid = pb_response.job().pods(i).podid();
        pod.jobid = pb_response.job().pods(i).jobid();
        pod.endpoint = pb_response.job().pods(i).endpoint();
        pod.status = (PodStatus)pb_response.job().pods(i).status();
        pod.version = pb_response.job().pods(i).version();
        pod.start_time = pb_response.job().pods(i).start_time();
        pod.update_time = pb_response.job().pods(i).update_time();
        pod.fail_count = pb_response.job().pods(i).fail_count();
        for (int32_t j = 0; j < pb_response.job().pods(i).services().size(); ++j) {
            ServiceInfo service;
            service.name = pb_response.job().pods(i).services(j).name();
            service.port = pb_response.job().pods(i).services(j).port();
            service.ip = pb_response.job().pods(i).services(j).ip();
            service.status = (Status)pb_response.job().pods(i).services(j).status();
            pod.services.push_back(service);
        }
        response->job.pods.push_back(pod);
    }

    return true;
}

bool AppMasterImpl::RecoverInstance(const RecoverInstanceRequest& request, RecoverInstanceResponse* response) {
    std::string jobid = Strim(request.jobid);
    if (jobid.empty()) {
        fprintf(stderr, "jobid must not be empty\n");
        return false;
    }
    std::string podid = Strim(request.podid); 
    if (podid.empty()){
        fprintf(stderr, "podid must not be empty\n");
        return false;
    }

    ::baidu::galaxy::proto::RecoverInstanceRequest pb_request;
    ::baidu::galaxy::proto::RecoverInstanceResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    pb_request.set_jobid(jobid);
    pb_request.set_podid(podid);

    bool ok = rpc_client_->SendRequest(appmaster_stub_,
                                        &::baidu::galaxy::proto::AppMaster_Stub::RecoverInstance,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "AppMaster Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    return true;

}

bool AppMasterImpl::ExecuteCmd(const ExecuteCmdRequest& request, ExecuteCmdResponse* response) {
    ::baidu::galaxy::proto::ExecuteCmdRequest pb_request;
    ::baidu::galaxy::proto::ExecuteCmdResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string jobid = Strim(request.jobid);
    if (jobid.empty()) {
        fprintf(stderr, "jobid must not be empty\n");
        return false;
    }
    pb_request.set_jobid(jobid);

    std::string cmd = Strim(request.cmd);
    if (cmd.empty()) {
        fprintf(stderr, "cmd must not be empty\n");
        return false;
    }
    pb_request.set_cmd(cmd);

    bool ok = rpc_client_->SendRequest(appmaster_stub_,
                                        &::baidu::galaxy::proto::AppMaster_Stub::ExecuteCmd,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "AppMaster Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    return true;
}

AppMaster* AppMaster::ConnectAppMaster(const std::string& nexus_addr, const std::string& path) {
    AppMasterImpl* appmaster = new AppMasterImpl(nexus_addr, path);
    if (!appmaster->GetStub()) {
        delete appmaster;
        return NULL;
    }
    return appmaster;
}


} //end namespace sdk
} //end namespace galaxy
} // end namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
