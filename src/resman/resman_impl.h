// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>

#include "src/protocol/resman.pb.h"
#include "src/protocol/agent.pb.h"
#include "scheduler.h"
#include "ins_sdk.h"
#include "src/rpc/rpc_client.h"
#include "mutex.h"
#include "thread_pool.h"

namespace baidu {
namespace galaxy {

using ::galaxy::ins::sdk::InsSDK;

struct AgentStat {
    proto::AgentStatus status;
    proto::AgentInfo info;
    int32_t last_heartbeat_time; //timestamp in seconds
};

class ResManImpl : public baidu::galaxy::proto::ResMan {
public:
    ResManImpl();
    ~ResManImpl();
    bool Init();
    bool RegisterOnNexus(const std::string& endpoint);
    void EnterSafeMode(::google::protobuf::RpcController* controller,
                       const ::baidu::galaxy::proto::EnterSafeModeRequest* request,
                       ::baidu::galaxy::proto::EnterSafeModeResponse* response,
                       ::google::protobuf::Closure* done);
    void LeaveSafeMode(::google::protobuf::RpcController* controller,
                       const ::baidu::galaxy::proto::LeaveSafeModeRequest* request,
                       ::baidu::galaxy::proto::LeaveSafeModeResponse* response,
                       ::google::protobuf::Closure* done);
    void Status(::google::protobuf::RpcController* controller,
                const ::baidu::galaxy::proto::StatusRequest* request,
                ::baidu::galaxy::proto::StatusResponse* response,
                ::google::protobuf::Closure* done);
    void KeepAlive(::google::protobuf::RpcController* controller,
                   const ::baidu::galaxy::proto::KeepAliveRequest* request,
                   ::baidu::galaxy::proto::KeepAliveResponse* response,
                   ::google::protobuf::Closure* done);
    void CreateContainerGroup(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::CreateContainerGroupRequest* request,
                         ::baidu::galaxy::proto::CreateContainerGroupResponse* response,
                         ::google::protobuf::Closure* done);
    void RemoveContainerGroup(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::RemoveContainerGroupRequest* request,
                         ::baidu::galaxy::proto::RemoveContainerGroupResponse* response,
                         ::google::protobuf::Closure* done);
    void UpdateContainerGroup(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::UpdateContainerGroupRequest* request,
                         ::baidu::galaxy::proto::UpdateContainerGroupResponse* response,
                         ::google::protobuf::Closure* done);
    void ListContainerGroups(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::ListContainerGroupsRequest* request,
                         ::baidu::galaxy::proto::ListContainerGroupsResponse* response,
                         ::google::protobuf::Closure* done);
    void ShowContainerGroup(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::ShowContainerGroupRequest* request,
                         ::baidu::galaxy::proto::ShowContainerGroupResponse* response,
                         ::google::protobuf::Closure* done);
    void AddAgent(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::AddAgentRequest* request,
                         ::baidu::galaxy::proto::AddAgentResponse* response,
                         ::google::protobuf::Closure* done);
    void RemoveAgent(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::RemoveAgentRequest* request,
                         ::baidu::galaxy::proto::RemoveAgentResponse* response,
                         ::google::protobuf::Closure* done);
    void OnlineAgent(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::OnlineAgentRequest* request,
                         ::baidu::galaxy::proto::OnlineAgentResponse* response,
                         ::google::protobuf::Closure* done);
    void OfflineAgent(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::OfflineAgentRequest* request,
                         ::baidu::galaxy::proto::OfflineAgentResponse* response,
                         ::google::protobuf::Closure* done);
    void ListAgents(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::ListAgentsRequest* request,
                         ::baidu::galaxy::proto::ListAgentsResponse* response,
                         ::google::protobuf::Closure* done);
    void ShowAgent(::google::protobuf::RpcController* controller,
                   const ::baidu::galaxy::proto::ShowAgentRequest* request,
                   ::baidu::galaxy::proto::ShowAgentResponse* response,
                   ::google::protobuf::Closure* done);
    void CreateTag(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::CreateTagRequest* request,
                         ::baidu::galaxy::proto::CreateTagResponse* response,
                         ::google::protobuf::Closure* done);
    void ListTags(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::ListTagsRequest* request,
                         ::baidu::galaxy::proto::ListTagsResponse* response,
                         ::google::protobuf::Closure* done);
    void ListAgentsByTag(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::ListAgentsByTagRequest* request,
                         ::baidu::galaxy::proto::ListAgentsByTagResponse* response,
                         ::google::protobuf::Closure* done);
    void GetTagsByAgent(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::GetTagsByAgentRequest* request,
                         ::baidu::galaxy::proto::GetTagsByAgentResponse* response,
                         ::google::protobuf::Closure* done);
    void AddAgentToPool(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::AddAgentToPoolRequest* request,
                         ::baidu::galaxy::proto::AddAgentToPoolResponse* response,
                         ::google::protobuf::Closure* done);
    void RemoveAgentFromPool(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::RemoveAgentFromPoolRequest* request,
                         ::baidu::galaxy::proto::RemoveAgentFromPoolResponse* response,
                         ::google::protobuf::Closure* done);
    void ListAgentsByPool(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::ListAgentsByPoolRequest* request,
                         ::baidu::galaxy::proto::ListAgentsByPoolResponse* response,
                         ::google::protobuf::Closure* done);
    void GetPoolByAgent(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::GetPoolByAgentRequest* request,
                         ::baidu::galaxy::proto::GetPoolByAgentResponse* response,
                         ::google::protobuf::Closure* done);
    void AddUser(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::AddUserRequest* request,
                         ::baidu::galaxy::proto::AddUserResponse* response,
                         ::google::protobuf::Closure* done);
    void RemoveUser(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::RemoveUserRequest* request,
                         ::baidu::galaxy::proto::RemoveUserResponse* response,
                         ::google::protobuf::Closure* done);
    void ListUsers(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::ListUsersRequest* request,
                         ::baidu::galaxy::proto::ListUsersResponse* response,
                         ::google::protobuf::Closure* done);
    void ShowUser(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::ShowUserRequest* request,
                         ::baidu::galaxy::proto::ShowUserResponse* response,
                         ::google::protobuf::Closure* done);
    void GrantUser(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::GrantUserRequest* request,
                         ::baidu::galaxy::proto::GrantUserResponse* response,
                         ::google::protobuf::Closure* done);
    void AssignQuota(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::AssignQuotaRequest* request,
                         ::baidu::galaxy::proto::AssignQuotaResponse* response,
                         ::google::protobuf::Closure* done);
    void Preempt(::google::protobuf::RpcController* controller,
                 const ::baidu::galaxy::proto::PreemptRequest* request,
                 ::baidu::galaxy::proto::PreemptResponse* response,
                 ::google::protobuf::Closure* done);
private:

    void QueryAgent(const std::string& agent_endpoint, bool is_first_query);
    void QueryAgentCallback(std::string agent_endpoint,
                            bool is_first_query,
                            const proto::QueryRequest* request,
                            proto::QueryResponse* response,
                            bool fail , int err);
    void CreateContainerCallback(std::string agent_endpoint,
                                 const proto::CreateContainerRequest* request,
                                 proto::CreateContainerResponse* response,
                                 bool fail, int err);
    void RemoveContainerCallback(std::string agent_endpoint,
                                 const proto::RemoveContainerRequest* request,
                                 proto::RemoveContainerResponse* response,
                                 bool fail, int err);
    void SendCommandsToAgent(const std::string& agent_endpoint,
                             const std::vector<sched::AgentCommand>& commands);
    template <class ProtoClass> 
    bool SaveObject(const std::string& key,
                    const ProtoClass& obj);
    
    template <class ProtoClass>
    bool LoadObjects(const std::string& prefix,
                     std::map<std::string, ProtoClass>& objs);

    bool RemoveObject(const std::string& key);
    static void OnRMLockChange(const ::galaxy::ins::sdk::WatchParam& param,
                               ::galaxy::ins::sdk::SDKError err);
    void OnLockChange(std::string lock_session_id);
    
    template <class RpcRequest, class RpcResponse, class DoneClosure>
    bool CheckUserExist(const RpcRequest* request, 
                        RpcResponse* response,
                        DoneClosure* done);
    void ReloadUsersAuth();
    bool CheckUserAuth(const proto::ContainerDescription& desc,
                       const std::string& user_name,
                       const std::map<std::string, std::set<std::string> > & users_auth,
                       std::string& invalid_pool);
    bool HasQuotaToCreate(const std::string& user,
                          const proto::ContainerGroupMeta& meta,
                          std::string& fail_reason);
    bool HasQuotaToUpdate(const std::string& user,
                          const proto::ContainerGroupMeta& old_meta,
                          const proto::ContainerGroupMeta& new_meta,
                          std::string& fail_reason);

    sched::Scheduler* scheduler_;
    InsSDK* nexus_;
    std::map<std::string, proto::AgentMeta> agents_;
    std::map<std::string, AgentStat> agent_stats_;
    std::map<std::string, std::set<std::string> > agent_tags_;
    std::map<std::string, proto::UserMeta> users_;
    std::map<std::string, proto::ContainerGroupMeta> container_groups_;
    std::map<std::string, std::set<std::string> > tags_;
    std::map<std::string, std::set<std::string> > pools_;
    std::map<std::string, std::set<std::string> > users_can_create_;
    std::map<std::string, std::set<std::string> >  users_can_update_;
    std::map<std::string, std::set<std::string> > users_can_remove_;
    std::map<std::string, std::set<std::string> >  users_can_list_;
    Mutex mu_;
    bool safe_mode_;
    bool force_safe_mode_;
    ThreadPool query_pool_;
    RpcClient rpc_client_;
    int64_t start_time_;
};

}
}

