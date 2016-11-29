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

#include <iostream>

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
    namespace am {

AppMasterImpl::AppMasterImpl() : 
    job_manager_(new baidu::galaxy::am::JobManager()),
    running_(false) {
}

AppMasterImpl::~AppMasterImpl() {
}


void AppMasterImpl::Setup() {
    running_ = true;
    resman_endpoint_ = "yq01-tera0.yq01.baidu.com:8645";
}

void AppMasterImpl::SubmitJob(::google::protobuf::RpcController* controller,
               const ::proto::SubmitJobRequest* request,
               ::proto::SubmitJobResponse* response,
               ::google::protobuf::Closure* done) {

    VLOG(10) << "submit job " << request->DebugString();
    LOG(INFO) << "recv submit job request from :" << request->hostname() 
        << " owner is: " << request->user().user();

    if (!running_) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("AM is starting ...");
        done->Run();
        return;
    }

    const proto::JobDescription& job_desc = request->job();
    proto::CreateContainerGroupRequest* container_request 
        = new proto::CreateContainerGroupRequest();
    container_request->mutable_user()->CopyFrom(request->user());
    container_request->set_name(job_desc.name());
    BuildContainerDescription(job_desc, container_request->mutable_desc());
    container_request->set_replica(job_desc.deploy().replica());

    VLOG(10) << "DEBUG CreateContainer: " <<  container_request->DebugString();

    proto::CreateContainerGroupResponse* container_response 
        = new proto::CreateContainerGroupResponse();
    boost::function<void (const proto::CreateContainerGroupRequest*,
                          proto::CreateContainerGroupResponse*, 
                          bool, int)> call_back;

    call_back = boost::bind(&AppMasterImpl::CreateContainerGroupCallBack, this,
                            job_desc, response, done,
                            _1, _2, _3, _4);

    boost::mutex::scoped_lock lock(rpc_mutex_);
    proto::ResMan_Stub* resman;
    rpc_client_.GetStub(resman_endpoint_, &resman);
    rpc_client_.AsyncRequest(resman,
                            &proto::ResMan_Stub::CreateContainerGroup,
                            container_request,
                            container_response,
                            call_back,
                            5, 0);
    delete resman;
    return;
}



void AppMasterImpl::CreateContainerGroupCallBack(proto::JobDescription job_desc,
                                         proto::SubmitJobResponse* submit_response,
                                         ::google::protobuf::Closure* done,
                                         const proto::CreateContainerGroupRequest* request,
                                         proto::CreateContainerGroupResponse* response,
                                         bool failed, int err) {

    boost::scoped_ptr<const proto::CreateContainerGroupRequest> request_ptr(request);
    boost::scoped_ptr<proto::CreateContainerGroupResponse> response_ptr(response);
    if (failed || response_ptr->error_code().status() != proto::kOk) {
        LOG(WARNING) << "fail to create container group with status " 
            << Status_Name(response_ptr->error_code().status());

        submit_response->mutable_error_code()->CopyFrom(response_ptr->error_code());
        done->Run();
        return;
    }

    // always return success
    baidu::galaxy::util::ErrorCode ec = job_manager_->Submit(response->id(), job_desc, request->user());
    assert(0 == ec.Code());

    submit_response->mutable_error_code()->set_status(proto::kOk);
    submit_response->mutable_error_code()->set_reason("submit job ok");
    submit_response->set_jobid(response->id());
    done->Run();
    return;
}

void AppMasterImpl::BuildContainerDescription(const ::proto::JobDescription& job_desc,
                                              ::proto::ContainerDescription* container_desc) {
    container_desc->set_priority(job_desc.priority());
    container_desc->set_run_user(job_desc.run_user());
    container_desc->set_version(job_desc.version());
    container_desc->set_max_per_host(job_desc.deploy().max_per_host());
    container_desc->set_tag(job_desc.deploy().tag());
    container_desc->set_cmd_line(FLAGS_appworker_cmdline);
    for (int i = 0; i < job_desc.deploy().pools_size(); i++) {
        container_desc->add_pool_names(job_desc.deploy().pools(i));
    }
    container_desc->set_container_type(proto::kNormalContainer);
    for (int i = 0; i < job_desc.volum_jobs_size(); i++) {
        container_desc->add_volum_jobs(job_desc.volum_jobs(i));
    }
    container_desc->mutable_workspace_volum()->CopyFrom(job_desc.pod().workspace_volum());
    container_desc->mutable_data_volums()->CopyFrom(job_desc.pod().data_volums());
    for (int i = 0; i < job_desc.pod().tasks_size(); i++) {
        proto::Cgroup* cgroup = container_desc->add_cgroups();
        cgroup->set_id(job_desc.pod().tasks(i).id());
        cgroup->mutable_cpu()->CopyFrom(job_desc.pod().tasks(i).cpu());
        cgroup->mutable_memory()->CopyFrom(job_desc.pod().tasks(i).memory());
        cgroup->mutable_tcp_throt()->CopyFrom(job_desc.pod().tasks(i).tcp_throt());
        cgroup->mutable_blkio()->CopyFrom(job_desc.pod().tasks(i).blkio());
        for (int j = 0; j < job_desc.pod().tasks(i).ports_size(); j++) {
            cgroup->add_ports()->CopyFrom(job_desc.pod().tasks(i).ports(j));
        }
    }
    VLOG(10) << "BuildContainerDescription: " << job_desc.name() <<  container_desc->DebugString();
    return;
}

/*void AppMasterImpl::UpdateContainerGroupCallBack(JobDescription job_desc, 
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
               const ::proto::UpdateJobRequest* request,
               ::proto::UpdateJobResponse* response,
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
void AppMasterImpl::ExecuteCmd(::google::protobuf::RpcController* controller,
                               const ::proto::ExecuteCmdRequest* request,
                               ::proto::ExecuteCmdResponse* response,
                               ::google::protobuf::Closure* done) {
}*/

void AppMasterImpl::RemoveContainerGroupCallBack(::proto::RemoveJobResponse* remove_response,
                                          ::google::protobuf::Closure* done,
                                          const proto::RemoveContainerGroupRequest* request,
                                          proto::RemoveContainerGroupResponse* response,
                                          bool failed, int) {
    boost::scoped_ptr<const proto::RemoveContainerGroupRequest> request_ptr(request);
    boost::scoped_ptr<proto::RemoveContainerGroupResponse> response_ptr(response);

    if (failed || (response_ptr->error_code().status() != proto::kOk && 
        response_ptr->error_code().status() != proto::kJobNotFound)) {
        LOG(WARNING) << "fail to remove container group";
        remove_response->mutable_error_code()->set_status(response_ptr->error_code().status());
        remove_response->mutable_error_code()->set_reason(response_ptr->error_code().reason());
        done->Run();
        return;
    }
    LOG(INFO) << "remove job " << request->id() << " by RM successfully";

    baidu::galaxy::util::ErrorCode ec = job_manager_->RemoveJob(request->id(), response);
    if (ec.Code() != 0) {
        LOG(WARNING) << "remove job " << request->id() << " failed: " << ec.Message();
    }
    done->Run();
    return;
}

void AppMasterImpl::RemoveJob(::google::protobuf::RpcController* controller,
               const ::proto::RemoveJobRequest* request,
               ::proto::RemoveJobResponse* response,
               ::google::protobuf::Closure* done) {

    LOG(INFO) << "receive remove job " << request->jobid()
        << " request from " << request->hostname();

    if (!running_) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("AM is not ready");
        done->Run();
        return;
    }

    proto::RemoveContainerGroupRequest* container_request = new proto::RemoveContainerGroupRequest();
    proto::RemoveContainerGroupResponse* container_response = new proto::RemoveContainerGroupResponse();
    container_request->mutable_user()->CopyFrom(request->user());
    container_request->set_id(request->jobid());
    boost::function<void (const proto::RemoveContainerGroupRequest*,
                proto::RemoveContainerGroupResponse*, 
                bool,
                int)> call_back = boost::bind(&AppMasterImpl::RemoveContainerGroupCallBack, 
                        this, 
                        response, 
                        done, _1, _2, _3, _4);

    proto::ResMan_Stub* resman;
    boost::mutex::scoped_lock lock(rpc_mutex_);
    rpc_client_.GetStub(resman_endpoint_, &resman);
    rpc_client_.AsyncRequest(resman,
                            &proto::ResMan_Stub::RemoveContainerGroup,
                            container_request,
                            container_response,
                            call_back,
                            5, 0);
    delete resman;
}



void AppMasterImpl::ListJobs(::google::protobuf::RpcController* controller,
            const ::proto::ListJobsRequest* request,
            ::proto::ListJobsResponse* response,
            ::google::protobuf::Closure* done) {
    if (!running_) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("AM is not ready");
        done->Run();
        return;
    }

    std::cerr << "list job ..\n";
    job_manager_->ListJobs(response);
    std::cerr << "list job ..1\n";
    done->Run();
}

void AppMasterImpl::ShowJob(::google::protobuf::RpcController* controller,
            const ::proto::ShowJobRequest* request,
            ::proto::ShowJobResponse* response,
            ::google::protobuf::Closure* done) {
    if (!running_) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("AM is not ready");
        done->Run();
        return;
    }
    job_manager_->ShowJob(request->jobid(), response);
    done->Run();
}



void AppMasterImpl::FetchTask(::google::protobuf::RpcController* controller,
                              const ::proto::FetchTaskRequest* request,
                              ::proto::FetchTaskResponse* response,
                              ::google::protobuf::Closure* done) {
    VLOG(10) << "DEBUG: FetchTask" << request->DebugString();
    baidu::galaxy::util::ErrorCode ec = job_manager_->HandleFetch(*request, *response);

    proto::Status status = (proto::Status)ec.Code();
    if (status != proto::kOk) {
        LOG(WARNING) << "FetchTask failed: " << Status_Name(status) 
            << ", " << ec.Message();
    }

    VLOG(10) << "DEBUG: Fetch response " << response->DebugString();
    done->Run();
    return;
}

/*
void AppMasterImpl::RecoverInstance(::google::protobuf::RpcController* controller,
                                    const ::proto::RecoverInstanceRequest* request,
                                    ::proto::RecoverInstanceResponse* response,
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
}*/


}
}
}


