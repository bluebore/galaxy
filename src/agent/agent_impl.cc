// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/agent_impl.h"

#include "gflags/gflags.h"

#include "boost/bind.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"
#include "proto/master.pb.h"
#include "logging.h"
#include "agent/agent_internal_infos.h"
#include "agent/utils.h"

DECLARE_string(master_host);
DECLARE_string(master_port);
DECLARE_int32(agent_background_threads_num);
DECLARE_int32(agent_heartbeat_interval);
DECLARE_string(agent_ip);
DECLARE_string(agent_port);

DECLARE_int32(agent_millicores);
DECLARE_int32(agent_memory);

namespace baidu {
namespace galaxy {

AgentImpl::AgentImpl() : 
    master_endpoint_(),
    lock_(),
    background_threads_(FLAGS_agent_background_threads_num),
    rpc_client_(NULL),
    endpoint_(),
    master_(NULL),
    resource_capacity_(),
    master_watcher_(NULL),
    mutex_master_endpoint_() { 

    rpc_client_ = new RpcClient();    
    endpoint_ = FLAGS_agent_ip;
    endpoint_.append(":");
    endpoint_.append(FLAGS_agent_port);
    master_watcher_ = new MasterWatcher();
    background_threads_.DelayTask(
            FLAGS_agent_heartbeat_interval, boost::bind(&AgentImpl::KeepHeartBeat, this));
}

AgentImpl::~AgentImpl() {
    background_threads_.Stop(false);
    if (rpc_client_ != NULL) {
        delete rpc_client_; 
        rpc_client_ = NULL;
    }
    delete master_watcher_;
}

void AgentImpl::Query(::google::protobuf::RpcController* /*cntl*/,
                      const ::baidu::galaxy::QueryRequest* /*req*/,
                      ::baidu::galaxy::QueryResponse* resp,
                      ::google::protobuf::Closure* done) {

    MutexLock scope_lock(&lock_);
    resp->set_status(kOk);
    AgentInfo agent_info; 
    agent_info.set_endpoint(endpoint_);
    agent_info.mutable_total()->set_millicores(
            FLAGS_agent_millicores);
    agent_info.mutable_total()->set_memory(
            FLAGS_agent_memory);
    agent_info.mutable_assigned()->set_millicores(
            FLAGS_agent_millicores - resource_capacity_.millicores); 
    agent_info.mutable_assigned()->set_memory(
            FLAGS_agent_memory - resource_capacity_.memory);

    std::vector<PodInfo> pods;
    pod_manager_.ShowPods(&pods);
    std::vector<PodInfo>::iterator it = pods.begin();
    for (; it != pods.end(); ++it) {
        PodStatus* pod_status = agent_info.add_pods();         
        pod_status->CopyFrom(it->pod_status);
    }
    resp->mutable_agent()->CopyFrom(agent_info);
    done->Run();
    return;
}

void AgentImpl::CreatePodInfo(
                    const ::baidu::galaxy::RunPodRequest* req,
                    PodInfo* pod_info) {
    if (pod_info == NULL) {
        return; 
    }
    pod_info->pod_id = req->podid();
    pod_info->job_id = req->jobid();
    pod_info->pod_desc.CopyFrom(req->pod());
    pod_info->pod_status.set_podid(req->podid());
    pod_info->pod_status.set_jobid(req->jobid());
    pod_info->pod_status.set_state(kPodPending);
    pod_info->initd_port = 0;
    pod_info->initd_pid = 0;
    
    for (int i = 0; i < pod_info->pod_desc.tasks_size(); i++) {
        TaskInfo task_info;
        task_info.task_id = GenerateTaskId(pod_info->pod_id);
        task_info.pod_id = pod_info->pod_id;
        task_info.desc.CopyFrom(pod_info->pod_desc.tasks(i));
        task_info.status.set_state(kPodPending);
        task_info.initd_endpoint = "";
        task_info.stage = kStagePENDING;
        task_info.fail_retry_times = 0;
        task_info.max_retry_times = 10;
        pod_info->tasks[task_info.task_id] = task_info;
    }
}

void AgentImpl::RunPod(::google::protobuf::RpcController* /*cntl*/,
                       const ::baidu::galaxy::RunPodRequest* req,
                       ::baidu::galaxy::RunPodResponse* resp,
                       ::google::protobuf::Closure* done) {
    do {
        MutexLock scope_lock(&lock_);
        if (!req->has_podid()
                || !req->has_pod()) {
            resp->set_status(kInputError);  
            break;
        }
        std::map<std::string, PodDescriptor>::iterator it = 
            pods_descs_.find(req->podid());
        if (it != pods_descs_.end()) {
            resp->set_status(kOk); 
            break;
        }
        if (AllocResource(req->pod().requirement()) != 0) {
            LOG(WARNING, "pod %s alloc resource failed",
                    req->podid().c_str()); 
            resp->set_status(kQuota);
            break;
        } 
        // NOTE alloc should before pods_desc set
        pods_descs_[req->podid()] = req->pod();

        PodInfo info;
        CreatePodInfo(req, &info);
        // if add failed, clean by master?
        pod_manager_.AddPod(info);
        resp->set_status(kOk); 
    } while (0);
    done->Run();
    return;
}

void AgentImpl::KillPod(::google::protobuf::RpcController* /*cntl*/,
                        const ::baidu::galaxy::KillPodRequest* req,
                        ::baidu::galaxy::KillPodResponse* resp,
                        ::google::protobuf::Closure* done) {
    if (!req->has_podid()) {
        LOG(WARNING, "master kill request has no podid"); 
        resp->set_status(kInputError);
        done->Run();
        return;
    }
    MutexLock scope_lock(&lock_);
    std::map<std::string, PodDescriptor>::iterator it = 
        pods_descs_.find(req->podid());
    if (it == pods_descs_.end()) {
        resp->set_status(kOk); 
        done->Run();
        return;
    }

    pod_manager_.DeletePod(req->podid());
    resp->set_status(kOk); 
    done->Run();
    return;
}

void AgentImpl::KeepHeartBeat() {
    MutexLock lock(&mutex_master_endpoint_);
    if (!PingMaster()) {
        LOG(WARNING, "ping master %s failed", 
                     master_endpoint_.c_str());
    }
    background_threads_.DelayTask(FLAGS_agent_heartbeat_interval,
                                  boost::bind(&AgentImpl::KeepHeartBeat, this));
    return;
}

bool AgentImpl::Init() {

    resource_capacity_.millicores = FLAGS_agent_millicores;
    resource_capacity_.memory = FLAGS_agent_memory;
    
    if (pod_manager_.Init() != 0) {
        return false; 
    }
    if (!RegistToMaster()) {
        return false; 
    }

    background_threads_.DelayTask(
                500, 
                boost::bind(&AgentImpl::LoopCheckPods, this)); 

    return true;
}

bool AgentImpl::PingMaster() {
    mutex_master_endpoint_.AssertHeld();
    HeartBeatRequest request;
    HeartBeatResponse response;
    request.set_endpoint(endpoint_); 
    return rpc_client_->SendRequest(master_,
                                    &Master_Stub::HeartBeat,
                                    &request,
                                    &response,
                                    5, 1);    
}

void AgentImpl::LoopCheckPods() {
    MutexLock scope_lock(&lock_);
    std::map<std::string, PodDescriptor>::iterator it = 
        pods_descs_.begin();
    std::vector<std::string> to_del_pod;
    for (; it != pods_descs_.end(); ++it) {
        if (pod_manager_.CheckPod(it->first) != 0) {
            to_del_pod.push_back(it->first);
            ReleaseResource(it->second.requirement()); 
        }  
    }
    for (size_t i = 0; i < to_del_pod.size(); i++) {
        pods_descs_.erase(to_del_pod[i]); 
    }
    background_threads_.DelayTask(
                100, 
                boost::bind(&AgentImpl::LoopCheckPods, this)); 
}

void AgentImpl::HandleMasterChange(const std::string& new_master_endpoint) {
    if (new_master_endpoint.empty()) {
        LOG(WARNING, "the master endpoint is deleted from nexus");
    }
    if (new_master_endpoint != master_endpoint_) {
        MutexLock lock(&mutex_master_endpoint_);
        LOG(INFO, "master change to %s", new_master_endpoint.c_str());
        master_endpoint_ = new_master_endpoint; 
        if (master_) {
            delete master_;
        }
        if (!rpc_client_->GetStub(master_endpoint_, &master_)) {
            LOG(WARNING, "connect master %s failed", master_endpoint_.c_str()); 
            return;
        }  
    }
}

bool AgentImpl::RegistToMaster() {
    boost::function<void(const std::string)> handler;
    handler = boost::bind(&AgentImpl::HandleMasterChange, this, _1);
    bool ok = master_watcher_->Init(handler);
    if (!ok) {
        LOG(WARNING, "fail to watch on nexus");
        return false;
    }
    MutexLock lock(&mutex_master_endpoint_);
    
    master_endpoint_ = master_watcher_->GetMasterEndpoint();

    if (!rpc_client_->GetStub(master_endpoint_, &master_)) {
        LOG(WARNING, "connect master %s failed", master_endpoint_.c_str()); 
        return false;
    }

    if (!PingMaster()) {
        LOG(WARNING, "connect master %s failed", master_endpoint_.c_str()); 
    }
    return true;
}

int AgentImpl::AllocResource(const Resource& requirement) {
    if (resource_capacity_.millicores > requirement.millicores()
            && resource_capacity_.memory > requirement.memory()) {
        resource_capacity_.millicores -= requirement.millicores(); 
        resource_capacity_.memory -= requirement.memory();
        return 0;
    }
    return -1;
}

void AgentImpl::ReleaseResource(const Resource& requirement) {
    resource_capacity_.millicores += requirement.millicores();
    resource_capacity_.memory += requirement.memory();
}

}   // ending namespace galaxy
}   // ending namespace baidu
