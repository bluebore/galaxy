// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _AGENT_IMPL_H_
#define _AGENT_IMPL_H_

#include <string>
#include <map>

#include "sofa/pbrpc/pbrpc.h"
#include "proto/agent.pb.h"
#include "proto/master.pb.h"

#include "mutex.h"
#include "thread_pool.h"
#include "rpc/rpc_client.h"

#include "agent/pod_manager.h"
#include "master/master_watcher.h"

namespace baidu {
namespace galaxy {

class AgentImpl : public Agent {

public:
    AgentImpl();
    virtual ~AgentImpl(); 

    bool Init();

    virtual void Query(::google::protobuf::RpcController* cntl, 
                       const ::baidu::galaxy::QueryRequest* req,
                       ::baidu::galaxy::QueryResponse* resp,
                       ::google::protobuf::Closure* done); 
    virtual void RunPod(::google::protobuf::RpcController* cntl,
                        const ::baidu::galaxy::RunPodRequest* req,
                        ::baidu::galaxy::RunPodResponse* resp,
                        ::google::protobuf::Closure* done);
    virtual void KillPod(::google::protobuf::RpcController* cntl,
                         const ::baidu::galaxy::KillPodRequest* req,
                         ::baidu::galaxy::KillPodResponse* resp,
                         ::google::protobuf::Closure* done);
    void HandleMasterChange(const std::string& new_master_endpoint);
private:
    void KeepHeartBeat();
    
    bool RegistToMaster();
    bool CheckGcedConnection();

    bool PingMaster();

    void LoopCheckPods();
    // ret ==  0 alloc success
    //        -1 alloc failed
    int AllocResource(const Resource& requirement);
    void ReleaseResource(const Resource& requirement);

    void CreatePodInfo(const ::baidu::galaxy::RunPodRequest* req, 
                       PodInfo* pod_info);

    struct ResourceCapacity {
        int64_t millicores; 
        int64_t memory;
        ResourceCapacity() 
            : millicores(0),
              memory(0) {
        }
    };
private:
    std::string master_endpoint_;
    
    Mutex lock_;
    ThreadPool background_threads_;
    RpcClient* rpc_client_;
    std::string endpoint_;
    Master_Stub* master_;
    ResourceCapacity resource_capacity_;

    MasterWatcher* master_watcher_;
    Mutex mutex_master_endpoint_;

    PodManager pod_manager_;
    std::map<std::string, PodDescriptor> pods_descs_; 
};

}   // ending namespace galaxy
}   // ending namespace baidu


#endif
