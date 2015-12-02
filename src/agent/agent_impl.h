// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _AGENT_IMPL_H_
#define _AGENT_IMPL_H_

#include <string>
#include <map>

#include "boost/unordered_set.hpp"

#include "sofa/pbrpc/pbrpc.h"
#include "proto/agent.pb.h"
#include "proto/master.pb.h"

#include "mutex.h"
#include "thread_pool.h"
#include "rpc/rpc_client.h"

#include "agent/pod_manager.h"
#include "master/master_watcher.h"
#include "agent/persistence_handler.h"

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
    virtual void ShowPods(::google::protobuf::RpcController* cntl,
                         const ::baidu::galaxy::ShowPodsRequest* req,
                         ::baidu::galaxy::ShowPodsResponse* resp,
                         ::google::protobuf::Closure* done);

    void HandleMasterChange(const std::string& new_master_endpoint);
private:
    typedef enum {
        kProc = 1,
        kNonProc = 2
    }ResourceType;

    void KeepHeartBeat();
    
    bool RegistToMaster();
    bool CheckGcedConnection();

    bool PingMaster();

    bool RestorePods();
    
    void LoopCheckPods();
  

    ResourceType GetResourceType(const PodDescriptor& pd);
    // ret ==  0 alloc success
    //        -1 alloc failed
    int AllocResource(const Resource& requirement, ResourceType type = kProc);
    void ReleaseResource(const Resource& requirement, ResourceType type = kNonProc);

    int32_t NonprocCpuTotal(int32_t total_shared, int32_t proc_used);

    void ConvertToPodPropertiy(const PodInfo& info, PodPropertiy* pod_propertiy);

    void CreatePodInfo(const ::baidu::galaxy::RunPodRequest* req, 
                       PodInfo* pod_info);

    void ProcessNonProc();

    struct ResourceCapacity {
        int64_t millicores; 
        int64_t memory;
        // TODO initd port not included
        boost::unordered_set<int32_t> used_port;
        int64_t nonproc_millicores;
        int64_t nonproc_millicores_assigned;
        ResourceCapacity() 
            : millicores(0),
              memory(0),
              used_port(),
              nonproc_millicores(0),
              nonproc_millicores_assigned(0) {
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
    PersistenceHandler persistence_handler_;    
};

}   // ending namespace galaxy
}   // ending namespace baidu


#endif
