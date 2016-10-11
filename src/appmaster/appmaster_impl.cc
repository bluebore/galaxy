// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "appmaster_impl.h"
#include <string>
#include <sstream>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <sys/utsname.h>
#include <gflags/gflags.h>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <snappy.h>

DECLARE_string(nexus_root);
DECLARE_string(nexus_addr);
DECLARE_string(jobs_store_path);
DECLARE_string(appworker_cmdline);
DECLARE_int32(safe_interval);

const std::string sMASTERLock = "/appmaster_lock";
const std::string sMASTERAddr = "/appmaster";
const std::string sRESMANPath = "/resman";

namespace baidu {
namespace galaxy {

AppMasterImpl::AppMasterImpl() : running_(false) {
    nexus_ = new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_addr);
    resman_watcher_ = new baidu::galaxy::Watcher();
}

AppMasterImpl::~AppMasterImpl() {
    //delete resman_watcher_;
    delete nexus_;
    delete resman_watcher_;
}

void AppMasterImpl::RunMaster() {
    running_ = true;
    job_manager_.Run();
    return;
}
void AppMasterImpl::Init() {
    if (!resman_watcher_->Init(boost::bind(&AppMasterImpl::HandleResmanChange, this, _1),
                                        FLAGS_nexus_addr, FLAGS_nexus_root, sRESMANPath)) {
        LOG(FATAL) << "init res manager watch failed, agent will exit ...";
        exit(1);
    }
    LOG(INFO) << "init resource manager watcher successfully";
    job_manager_.Start();
    ReloadAppInfo();
    worker_.DelayTask(FLAGS_safe_interval * 1000,
                boost::bind(&AppMasterImpl::RunMaster, this));
    return;
}

void AppMasterImpl::ReloadAppInfo() {
    std::string start_key = FLAGS_nexus_root + FLAGS_jobs_store_path + "/";
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
            LOG(INFO) << "reload job: " << job_info.jobid();
            job_manager_.ReloadJobInfo(job_info);
        } else {
            LOG(WARNING) <<  "faild to parse job_info: " << key;
        }
        result->Next();
        job_amount++;
    }
    LOG(INFO) << "reload all job desc finish, total#: " << job_amount;
}

void AppMasterImpl::HandleResmanChange(const std::string& new_endpoint) {
    if (new_endpoint.empty()) {
        LOG(WARNING) << "endpoint of AM is deleted from nexus";
    }
    if (new_endpoint != resman_endpoint_) {
        MutexLock lock(&resman_mutex_);
        LOG(INFO) << "AM changes to " << new_endpoint.c_str();
        resman_endpoint_ = new_endpoint;
        job_manager_.SetResmanEndpoint(resman_endpoint_);
    }
    return;
}

void AppMasterImpl::OnMasterLockChange(const ::galaxy::ins::sdk::WatchParam& param,
                                ::galaxy::ins::sdk::SDKError err) {
    AppMasterImpl* impl = static_cast<AppMasterImpl*>(param.context);
    impl->OnLockChange(param.value);
}

void AppMasterImpl::OnLockChange(std::string lock_session_id) {
    std::string self_session_id = nexus_->GetSessionID();
    if (self_session_id != lock_session_id) {
        LOG(FATAL) << "master lost lock , die.";
    }
}

bool AppMasterImpl::RegisterOnNexus(const std::string endpoint) {
    ::galaxy::ins::sdk::SDKError err;
    bool ret = nexus_->Lock(FLAGS_nexus_root + sMASTERLock, &err);
    if (!ret) {
        LOG(WARNING) << "failed to acquire AM lock, " << err;
        return false;
    }
    ret = nexus_->Put(FLAGS_nexus_root + sMASTERAddr, endpoint, &err);
    if (!ret) {
        LOG(WARNING) << "failed to write AM endpoint to nexus, " << err;
        return false;
    }
    ret = nexus_->Watch(FLAGS_nexus_root + sMASTERLock, &OnMasterLockChange, this, &err);
    if (!ret) {
        LOG(WARNING) << "failed to watch appmaster lock, " << err;
        return false;
    }
    return true;
}

void AppMasterImpl::CreateContainerGroupCallBack(JobDescription job_desc,
                                         proto::SubmitJobResponse* submit_response,
                                         ::google::protobuf::Closure* done,
                                         const proto::CreateContainerGroupRequest* request,
                                         proto::CreateContainerGroupResponse* response,
                                         bool failed, int err) {
    boost::scoped_ptr<const baidu::galaxy::proto::CreateContainerGroupRequest> request_ptr(request);
    boost::scoped_ptr<baidu::galaxy::proto::CreateContainerGroupResponse> response_ptr(response);
    if (failed || response_ptr->error_code().status() != proto::kOk) {
        LOG(WARNING) << "fail to create container group with status " 
        << Status_Name(response_ptr->error_code().status());
        submit_response->mutable_error_code()->CopyFrom(response_ptr->error_code());
        done->Run();
        return;
    }
    Status status = job_manager_.Add(response->id(), job_desc, request->user());
    if (status != proto::kOk) {
        LOG(WARNING) << "fail to add job :" << response->id() 
        << " with status " << Status_Name(status);
        submit_response->mutable_error_code()->set_status(status);
        submit_response->mutable_error_code()->set_reason(Status_Name(status));
        done->Run();
        return;
    }
    submit_response->mutable_error_code()->set_status(status);
    submit_response->mutable_error_code()->set_reason("submit job ok");
    submit_response->set_jobid(response->id());
    done->Run();
    return;
}

void AppMasterImpl::BuildContainerDescription(const ::baidu::galaxy::proto::JobDescription& job_desc,
                                              ::baidu::galaxy::proto::ContainerDescription* container_desc) {
    container_desc->set_priority(job_desc.priority());
    container_desc->set_run_user(job_desc.run_user());
    container_desc->set_version(job_desc.version());
    container_desc->set_max_per_host(job_desc.deploy().max_per_host());
    container_desc->set_tag(job_desc.deploy().tag());
    container_desc->set_cmd_line(FLAGS_appworker_cmdline);
    for (int i = 0; i < job_desc.deploy().pools_size(); i++) {
        container_desc->add_pool_names(job_desc.deploy().pools(i));
    }
    container_desc->set_container_type(kNormalContainer);
    for (int i = 0; i < job_desc.volum_jobs_size(); i++) {
        container_desc->add_volum_jobs(job_desc.volum_jobs(i));
    }
    container_desc->mutable_workspace_volum()->CopyFrom(job_desc.pod().workspace_volum());
    container_desc->mutable_data_volums()->CopyFrom(job_desc.pod().data_volums());
    for (int i = 0; i < job_desc.pod().tasks_size(); i++) {
        Cgroup* cgroup = container_desc->add_cgroups();
        cgroup->set_id(job_desc.pod().tasks(i).id());
        cgroup->mutable_cpu()->CopyFrom(job_desc.pod().tasks(i).cpu());
        cgroup->mutable_memory()->CopyFrom(job_desc.pod().tasks(i).memory());
        cgroup->mutable_tcp_throt()->CopyFrom(job_desc.pod().tasks(i).tcp_throt());
        cgroup->mutable_blkio()->CopyFrom(job_desc.pod().tasks(i).blkio());
        for (int j = 0; j < job_desc.pod().tasks(i).ports_size(); j++) {
            cgroup->add_ports()->CopyFrom(job_desc.pod().tasks(i).ports(j));
        }
    }
    VLOG(10) << "TRACE BuildContainerDescription: " << job_desc.name();
    VLOG(10) <<  container_desc->DebugString();
    VLOG(10) << "TRACE END";
    return;
}

void AppMasterImpl::SubmitJob(::google::protobuf::RpcController* controller,
               const ::baidu::galaxy::proto::SubmitJobRequest* request,
               ::baidu::galaxy::proto::SubmitJobResponse* response,
               ::google::protobuf::Closure* done) {
    VLOG(10) << "DEBUG SubmitJob: ";
    VLOG(10) << request->DebugString();
    VLOG(10) << "DEBUG END";
    if (!running_) {
        response->mutable_error_code()->set_status(kError);
        response->mutable_error_code()->set_reason("AM not running");
        done->Run();
        return;
    }
    const JobDescription& job_desc = request->job();
    MutexLock lock(&resman_mutex_);
    proto::CreateContainerGroupRequest* container_request = new proto::CreateContainerGroupRequest();
    container_request->mutable_user()->CopyFrom(request->user());
    container_request->set_name(job_desc.name());
    BuildContainerDescription(job_desc, container_request->mutable_desc());
    container_request->set_replica(job_desc.deploy().replica());
    VLOG(10) << "DEBUG CreateContainer: ";
    VLOG(10) <<  container_request->DebugString();
    VLOG(10) << "DEBUG END";
    proto::CreateContainerGroupResponse* container_response = new proto::CreateContainerGroupResponse();
    
    boost::function<void (const proto::CreateContainerGroupRequest*,
                          proto::CreateContainerGroupResponse*, 
                          bool, int)> call_back;
    call_back = boost::bind(&AppMasterImpl::CreateContainerGroupCallBack, this,
                            job_desc, response, done,
                            _1, _2, _3, _4);
    ResMan_Stub* resman;
    rpc_client_.GetStub(resman_endpoint_, &resman);
    rpc_client_.AsyncRequest(resman,
                            &ResMan_Stub::CreateContainerGroup,
                            container_request,
                            container_response,
                            call_back,
                            5, 0);
    delete resman;
    return;
}

void AppMasterImpl::UpdateContainerGroupCallBack(JobDescription job_desc, 
                                         proto::UpdateJobResponse* update_response,
                                         ::google::protobuf::Closure* done,
                                         const proto::UpdateContainerGroupRequest* request,
                                         proto::UpdateContainerGroupResponse* response,
                                         bool failed, int err) {
    boost::scoped_ptr<const proto::UpdateContainerGroupRequest> request_ptr(request);
    boost::scoped_ptr<proto::UpdateContainerGroupResponse> response_ptr(response);
    if (failed || response_ptr->error_code().status() != proto::kOk) {
        //LOG(WARNING, "fail to update container group");
        update_response->mutable_error_code()->CopyFrom(response_ptr->error_code());
        done->Run();
        return;
    }
    bool container_change = false;
    if (response_ptr->has_resource_change() && response_ptr->resource_change()) {
        container_change = true;
    }
    Status status = job_manager_.Update(request->id(), job_desc, container_change);

    if (status != proto::kOk) {
        update_response->mutable_error_code()->set_status(status);
        update_response->mutable_error_code()->set_reason(Status_Name(status));
        done->Run();
        return;
    }
    update_response->mutable_error_code()->set_status(status);
    update_response->mutable_error_code()->set_reason("update job ok");
    done->Run();
    return;
}

void AppMasterImpl::RollbackContainerGroupCallBack(proto::UpdateJobResponse* rollback_response,
                                         ::google::protobuf::Closure* done,
                                         const proto::UpdateContainerGroupRequest* request,
                                         proto::UpdateContainerGroupResponse* response,
                                         bool failed, int err) {
    boost::scoped_ptr<const proto::UpdateContainerGroupRequest> request_ptr(request);
    boost::scoped_ptr<proto::UpdateContainerGroupResponse> response_ptr(response);
    if (failed || response_ptr->error_code().status() != proto::kOk) {
        //LOG(WARNING, "fail to update container group");
        rollback_response->mutable_error_code()->CopyFrom(response_ptr->error_code());
        done->Run();
        return;
    }
    Status status = job_manager_.Rollback(request->id());
    if (status != proto::kOk) {
        rollback_response->mutable_error_code()->set_status(status);
        rollback_response->mutable_error_code()->set_reason(Status_Name(status));
        VLOG(10) << rollback_response->DebugString();
        done->Run();
        return;
    }
    rollback_response->mutable_error_code()->set_status(status);
    rollback_response->mutable_error_code()->set_reason("rollback job ok");
    VLOG(10) << rollback_response->DebugString();
    done->Run();
    return;
}

void AppMasterImpl::UpdateJob(::google::protobuf::RpcController* controller,
               const ::baidu::galaxy::proto::UpdateJobRequest* request,
               ::baidu::galaxy::proto::UpdateJobResponse* response,
               ::google::protobuf::Closure* done) {
    VLOG(10) << "DEBUG UpdateJob: ";
    VLOG(10) << request->DebugString();
    VLOG(10) << "DEBUG END";
    if (!running_) {
        response->mutable_error_code()->set_status(kError);
        response->mutable_error_code()->set_reason("AM not running");
        done->Run();
        return;
    }
    const JobDescription& job_desc = request->job();
    if (request->has_operate() && request->operate() == kUpdateJobContinue) {
        uint32_t update_break_count = 0;
        if (request->has_update_break_count()) {
            update_break_count = request->update_break_count();
        }
        Status status = job_manager_.ContinueUpdate(request->jobid(), update_break_count);
        if (status != proto::kOk) {
            response->mutable_error_code()->set_status(status);
            response->mutable_error_code()->set_reason(Status_Name(status));
            VLOG(10) << response->DebugString();
            done->Run();
            return;
        }
        response->mutable_error_code()->set_status(status);
        response->mutable_error_code()->set_reason("continue job ok");
        VLOG(10) << response->DebugString();
        done->Run();
        return;
    } else if (request->has_operate() && request->operate() == kUpdateJobRollback) {
        MutexLock lock(&resman_mutex_);
        JobDescription last_desc = job_manager_.GetLastDesc(request->jobid());
        if (!last_desc.has_name()) {
            response->mutable_error_code()->set_status(kError);
            response->mutable_error_code()->set_reason("last description not fount");
            VLOG(10) << response->DebugString();
            done->Run();
            return;
        }
        
        proto::UpdateContainerGroupRequest* container_request = new proto::UpdateContainerGroupRequest();
        container_request->mutable_user()->CopyFrom(request->user());
        container_request->set_id(request->jobid());

        container_request->set_interval(last_desc.deploy().interval());
        container_request->set_replica(last_desc.deploy().replica());
        BuildContainerDescription(last_desc, container_request->mutable_desc());
        VLOG(10) << "DEBUG RollbackUpdateContainer: ";
        VLOG(10) <<  container_request->DebugString();
        VLOG(10) << "DEBUG END";
        proto::UpdateContainerGroupResponse* container_response = new proto::UpdateContainerGroupResponse();
        boost::function<void (const proto::UpdateContainerGroupRequest*,
                              proto::UpdateContainerGroupResponse*, 
                              bool, int)> call_back;
        call_back = boost::bind(&AppMasterImpl::RollbackContainerGroupCallBack, this,
                                response, done,
                                _1, _2, _3, _4);
        ResMan_Stub* resman;
        rpc_client_.GetStub(resman_endpoint_, &resman);
        rpc_client_.AsyncRequest(resman,
                                &ResMan_Stub::UpdateContainerGroup,
                                container_request,
                                container_response,
                                call_back,
                                5, 0);
        delete resman;
        return;
    } else if (request->has_operate() && request->operate() == kUpdateJobPause) {
        Status status = job_manager_.PauseUpdate(request->jobid());
        if (status != proto::kOk) {
            response->mutable_error_code()->set_status(status);
            response->mutable_error_code()->set_reason(Status_Name(status));
            VLOG(10) << response->DebugString();
            done->Run();
            return;
        }
        response->mutable_error_code()->set_status(status);
        response->mutable_error_code()->set_reason("pause job ok");
        VLOG(10) << response->DebugString();
        done->Run();
        return;
    }
    
    MutexLock lock(&resman_mutex_);
    proto::UpdateContainerGroupRequest* container_request = new proto::UpdateContainerGroupRequest();
    container_request->mutable_user()->CopyFrom(request->user());
    container_request->set_id(request->jobid());
    container_request->set_interval(job_desc.deploy().interval());
    BuildContainerDescription(job_desc, container_request->mutable_desc());
    container_request->set_replica(job_desc.deploy().replica());
    VLOG(10) << "DEBUG UpdateContainer: ";
    VLOG(10) <<  container_request->DebugString();
    VLOG(10) << "DEBUG END";
    proto::UpdateContainerGroupResponse* container_response = new proto::UpdateContainerGroupResponse();
    boost::function<void (const proto::UpdateContainerGroupRequest*,
                          proto::UpdateContainerGroupResponse*, 
                          bool, int)> call_back;

    call_back = boost::bind(&AppMasterImpl::UpdateContainerGroupCallBack, this,
                            job_desc, response, done,
                            _1, _2, _3, _4);
    ResMan_Stub* resman;
    rpc_client_.GetStub(resman_endpoint_, &resman);
    rpc_client_.AsyncRequest(resman,
                            &ResMan_Stub::UpdateContainerGroup,
                            container_request,
                            container_response,
                            call_back,
                            5, 0);
    delete resman;
    return;
}

void AppMasterImpl::RemoveContainerGroupCallBack(::baidu::galaxy::proto::RemoveJobResponse* remove_response,
                                          ::google::protobuf::Closure* done,
                                          const proto::RemoveContainerGroupRequest* request,
                                          proto::RemoveContainerGroupResponse* response,
                                          bool failed, int) {
    boost::scoped_ptr<const proto::RemoveContainerGroupRequest> request_ptr(request);
    boost::scoped_ptr<proto::RemoveContainerGroupResponse> response_ptr(response);
    if (failed || (response_ptr->error_code().status() != kOk && 
        response_ptr->error_code().status() != kJobNotFound)) {
        LOG(WARNING) << "fail to remove container group";
        remove_response->mutable_error_code()->set_status(response_ptr->error_code().status());
        remove_response->mutable_error_code()->set_reason(response_ptr->error_code().reason());
        done->Run();
        return;
    }
    VLOG(10) << "DEBUG RemoveJob: ";
    VLOG(10) << request->DebugString();
    VLOG(10) << "DEBUG END";
    Status status = job_manager_.Terminate(request->id(), request->user());
    VLOG(10) << "remove job : " << request->DebugString();
    remove_response->mutable_error_code()->set_status(status);
    remove_response->mutable_error_code()->set_reason("remove job ok");
    done->Run();
    return;
}

void AppMasterImpl::RemoveJob(::google::protobuf::RpcController* controller,
               const ::baidu::galaxy::proto::RemoveJobRequest* request,
               ::baidu::galaxy::proto::RemoveJobResponse* response,
               ::google::protobuf::Closure* done) {
    if (!running_) {
        response->mutable_error_code()->set_status(kError);
        response->mutable_error_code()->set_reason("AM not running");
        done->Run();
        return;
    }
    MutexLock lock(&resman_mutex_);
    proto::RemoveContainerGroupRequest* container_request = new proto::RemoveContainerGroupRequest();
    proto::RemoveContainerGroupResponse* container_response = new proto::RemoveContainerGroupResponse();
    container_request->mutable_user()->CopyFrom(request->user());
    container_request->set_id(request->jobid());
    boost::function<void (const proto::RemoveContainerGroupRequest*, proto::RemoveContainerGroupResponse*, 
            bool, int)> call_back;
    call_back = boost::bind(&AppMasterImpl::RemoveContainerGroupCallBack, this, response, done, _1, _2, _3, _4);
    ResMan_Stub* resman;
    rpc_client_.GetStub(resman_endpoint_, &resman);
    rpc_client_.AsyncRequest(resman,
                            &ResMan_Stub::RemoveContainerGroup,
                            container_request,
                            container_response,
                            call_back,
                            5, 0);
    delete resman;
    return;
}

void AppMasterImpl::ListJobs(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::proto::ListJobsRequest* request,
                            ::baidu::galaxy::proto::ListJobsResponse* response,
                            ::google::protobuf::Closure* done) {
    if (!running_) {
        response->mutable_error_code()->set_status(kError);
        response->mutable_error_code()->set_reason("AM not running");
        done->Run();
        return;
    }
    job_manager_.GetJobsOverview(response->mutable_jobs());
    response->mutable_error_code()->set_status(kOk);
    done->Run();
}

void AppMasterImpl::ShowJob(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::proto::ShowJobRequest* request,
                            ::baidu::galaxy::proto::ShowJobResponse* response,
                            ::google::protobuf::Closure* done) {
    if (!running_) {
        response->mutable_error_code()->set_status(kError);
        response->mutable_error_code()->set_reason("AM not running");
        done->Run();
        return;
    }
    Status rlt = job_manager_.GetJobInfo(request->jobid(), response->mutable_job());
    response->mutable_error_code()->set_status(rlt);
    done->Run();
}

void AppMasterImpl::ExecuteCmd(::google::protobuf::RpcController* controller,
                               const ::baidu::galaxy::proto::ExecuteCmdRequest* request,
                               ::baidu::galaxy::proto::ExecuteCmdResponse* response,
                               ::google::protobuf::Closure* done) {
}

void AppMasterImpl::FetchTask(::google::protobuf::RpcController* controller,
                              const ::baidu::galaxy::proto::FetchTaskRequest* request,
                              ::baidu::galaxy::proto::FetchTaskResponse* response,
                              ::google::protobuf::Closure* done) {
    VLOG(10) << "DEBUG: FetchTask"
    << request->DebugString() 
    <<"DEBUG END";
    Status status = job_manager_.HandleFetch(request, response);
    if (status != kOk) {
        LOG(WARNING) << "FetchTask failed, code:" << Status_Name(status) << ", method:" << __FUNCTION__;
    }
    VLOG(10) << "DEBUG: Fetch response " 
    << response->DebugString() 
    <<"DEBUG END";
    done->Run();
    return;
}

void AppMasterImpl::RecoverInstance(::google::protobuf::RpcController* controller,
                                    const ::baidu::galaxy::proto::RecoverInstanceRequest* request,
                                    ::baidu::galaxy::proto::RecoverInstanceResponse* response,
                                    ::google::protobuf::Closure* done) {
    Status status = job_manager_.RecoverPod(request->user(), request->jobid(), request->podid());
    LOG(INFO) << "DEBUG: RecoverInstance req"
        << request->DebugString()
        << "DEBUG END";
    if (status != kOk) {
        LOG(WARNING) << "RecoverInstance failed, code: " << Status_Name(status);
    }
    response->mutable_error_code()->set_status(status);
    done->Run();
    return;
}
}
}


