// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once
#include "appmaster_v2/job/job_manager.h"
#include "rpc/rpc_client.h"
#include "protocol/galaxy.pb.h"
#include "protocol/appmaster.pb.h"
#include "protocol/resman.pb.h"

#include <string>
#include <map>
#include <set>

namespace proto = baidu::galaxy::proto;

namespace baidu {
namespace galaxy {
namespace am {


class AppMasterImpl : public baidu::galaxy::proto::AppMaster {
public:

    AppMasterImpl();
    virtual ~AppMasterImpl();
    //void Init();
    void Setup();
    void SubmitJob(::google::protobuf::RpcController* controller,
                  const ::baidu::galaxy::proto::SubmitJobRequest* request,
                  ::baidu::galaxy::proto::SubmitJobResponse* response,
                  ::google::protobuf::Closure* done);

    void UpdateJob(::google::protobuf::RpcController* controller,
                  const ::baidu::galaxy::proto::UpdateJobRequest* request,
                  ::baidu::galaxy::proto::UpdateJobResponse* response,
                  ::google::protobuf::Closure* done) {}

    void RemoveJob(::google::protobuf::RpcController* controller,
                  const ::baidu::galaxy::proto::RemoveJobRequest* request,
                  ::baidu::galaxy::proto::RemoveJobResponse* response,
                  ::google::protobuf::Closure* done);

    void ListJobs(::google::protobuf::RpcController* controller,
                  const ::baidu::galaxy::proto::ListJobsRequest* request,
                  ::baidu::galaxy::proto::ListJobsResponse* response,
                  ::google::protobuf::Closure* done);

    void ShowJob(::google::protobuf::RpcController* controller,
                  const ::baidu::galaxy::proto::ShowJobRequest* request,
                  ::baidu::galaxy::proto::ShowJobResponse* response,
                  ::google::protobuf::Closure* done);

    void ExecuteCmd(::google::protobuf::RpcController* controller,
                   const ::baidu::galaxy::proto::ExecuteCmdRequest* request,
                   ::baidu::galaxy::proto::ExecuteCmdResponse* response,
                   ::google::protobuf::Closure* done) {}

    void FetchTask(::google::protobuf::RpcController* controller,
                  const ::baidu::galaxy::proto::FetchTaskRequest* request,
                  ::baidu::galaxy::proto::FetchTaskResponse* response,
                  ::google::protobuf::Closure* done);

    void RecoverInstance(::google::protobuf::RpcController* controller,
                        const ::baidu::galaxy::proto::RecoverInstanceRequest* request,
                        ::baidu::galaxy::proto::RecoverInstanceResponse* response,
                        ::google::protobuf::Closure* done) {}

private:
    void BuildContainerDescription(const proto::JobDescription& job_desc,
                                  ::baidu::galaxy::proto::ContainerDescription* container_desc);
    void CreateContainerGroupCallBack(proto::JobDescription job_desc,
                                       proto::SubmitJobResponse* submit_response,
                                       ::google::protobuf::Closure* done,
                                       const proto::CreateContainerGroupRequest* request,
                                       proto::CreateContainerGroupResponse* response,
                                       bool failed, int err) ;
    void UpdateContainerGroupCallBack(proto::JobDescription job_desc, 
                                     proto::UpdateJobResponse* update_response,
                                     ::google::protobuf::Closure* done,
                                     const proto::UpdateContainerGroupRequest* request,
                                     proto::UpdateContainerGroupResponse* response,
                                     bool failed, int err);
    void RollbackContainerGroupCallBack(proto::UpdateJobResponse* rollback_response,
                                 ::google::protobuf::Closure* done,
                                 const proto::UpdateContainerGroupRequest* request,
                                 proto::UpdateContainerGroupResponse* response,
                                 bool failed, int err);
    void RemoveContainerGroupCallBack(::baidu::galaxy::proto::RemoveJobResponse* remove_response,
                                      ::google::protobuf::Closure* done,
                                      const proto::RemoveContainerGroupRequest* request,
                                      proto::RemoveContainerGroupResponse* response,
                                      bool failed, int);
    /*void HandleResmanChange(const std::string& new_endpoint);
    void OnLockChange(std::string lock_session_id);
    void ReloadAppInfo();
    static void OnMasterLockChange(const ::galaxy::ins::sdk::WatchParam& param,
                            ::galaxy::ins::sdk::SDKError err);
    void RunMaster();
    */


private:
    boost::shared_ptr<baidu::galaxy::am::JobManager> job_manager_;
    std::string resman_endpoint_;
    RpcClient rpc_client_;
    /*InsSDK *nexus_;
    ThreadPool worker_;
    Watcher* resman_watcher_;*/
    boost::mutex rpc_mutex_;
    bool running_;
    //::baidu::galaxy::proto::ResourceManager_Stub* resman_;
};

} //namespace galaxy
} //namespace baidu
}
