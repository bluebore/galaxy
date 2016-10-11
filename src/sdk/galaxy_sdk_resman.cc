// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "galaxy_sdk_util.h"
#include "rpc/rpc_client.h"
#include "ins_sdk.h"
#include <gflags/gflags.h>
#include "protocol/resman.pb.h"
#include "protocol/galaxy.pb.h"
#include "galaxy_sdk_resman.h"

namespace baidu {
namespace galaxy {
namespace sdk {

ResourceManager::~ResourceManager() {
}

class ResourceManagerImpl : public ResourceManager {
public:
    ResourceManagerImpl(const std::string& nexus_addr, const std::string& path) : rpc_client_(NULL),
 							res_stub_(NULL) {
	    rpc_client_ = new ::baidu::galaxy::RpcClient();
        full_key_ = path;
        nexus_ = new ::galaxy::ins::sdk::InsSDK(nexus_addr);
    }

    virtual ~ResourceManagerImpl() {
        delete rpc_client_;
        if (NULL != res_stub_) {
            delete res_stub_;
        }
        delete nexus_;
    }

    bool GetStub();
    bool MasterEndpoint(const std::string& appmaster_path, 
             std::string* appmaster_endpoint, 
             std::string* resman_endpoint);

    bool EnterSafeMode(const EnterSafeModeRequest& request, EnterSafeModeResponse* response);
    bool LeaveSafeMode(const LeaveSafeModeRequest& request, LeaveSafeModeResponse* response);
    bool Status(const StatusRequest& request, StatusResponse* response);
    bool CreateContainerGroup(const CreateContainerGroupRequest& request, 
                              CreateContainerGroupResponse* response);
    bool RemoveContainerGroup(const RemoveContainerGroupRequest& request, 
                              RemoveContainerGroupResponse* response);
    bool UpdateContainerGroup(const UpdateContainerGroupRequest& request, 
                              UpdateContainerGroupResponse* response);
    bool ListContainerGroups(const ListContainerGroupsRequest& request, 
                             ListContainerGroupsResponse* response);
    bool ShowContainerGroup(const ShowContainerGroupRequest& request, 
                            ShowContainerGroupResponse* response);
    bool AddAgent(const AddAgentRequest& request, AddAgentResponse* response);
    bool RemoveAgent(const RemoveAgentRequest& request, RemoveAgentResponse* response);
    bool ShowAgent(const ShowAgentRequest& request, ShowAgentResponse* response);
    bool OnlineAgent(const OnlineAgentRequest& request, OnlineAgentResponse* response);
    bool OfflineAgent(const OfflineAgentRequest& request, OfflineAgentResponse* response);
    bool ListAgents(const ListAgentsRequest& request, ListAgentsResponse* response);
    bool CreateTag(const CreateTagRequest& request, CreateTagResponse* response);
    bool ListTags(const ListTagsRequest& request, ListTagsResponse* response);
    bool ListAgentsByTag(const ListAgentsByTagRequest& request, ListAgentsByTagResponse* response);
    bool GetTagsByAgent(const GetTagsByAgentRequest& request, GetTagsByAgentResponse* response);
    bool AddAgentToPool(const AddAgentToPoolRequest& request, AddAgentToPoolResponse* response);
    bool RemoveAgentFromPool(const RemoveAgentFromPoolRequest& request, 
                             RemoveAgentFromPoolResponse* response);
    bool ListAgentsByPool(const ListAgentsByPoolRequest& request, 
                          ListAgentsByPoolResponse* response);
    bool GetPoolByAgent(const GetPoolByAgentRequest& request, GetPoolByAgentResponse* response);
    bool AddUser(const AddUserRequest& request, AddUserResponse* response);
    bool RemoveUser(const RemoveUserRequest& request, RemoveUserResponse* response);
    bool ListUsers(const ListUsersRequest& request, ListUsersResponse* response);
    bool ShowUser(const ShowUserRequest& request, ShowUserResponse* response);
    bool GrantUser(const GrantUserRequest& request, GrantUserResponse* response);
    bool AssignQuota(const AssignQuotaRequest& request, AssignQuotaResponse* response);
    bool Preempt(const PreemptRequest& request, PreemptResponse* response);

private:
    ::galaxy::ins::sdk::InsSDK* nexus_;
    ::baidu::galaxy::RpcClient* rpc_client_;
    ::baidu::galaxy::proto::ResMan_Stub* res_stub_;
    std::string full_key_;
    std::string endpoint_;
};

bool ResourceManagerImpl::MasterEndpoint(const std::string& appmaster_path, 
             std::string* appmaster_endpoint, 
             std::string* resman_endpoint) {
    if (appmaster_endpoint == NULL || resman_endpoint == NULL) {
        return false;
    }
    ::galaxy::ins::sdk::SDKError err;
    bool ok = nexus_->Get(appmaster_path, appmaster_endpoint, &err);
    if (!ok || err != ::galaxy::ins::sdk::kOK) {
        fprintf(stderr, "get appmaster endpoint from nexus failed: %s\n", 
                ::galaxy::ins::sdk::InsSDK::StatusToString(err).c_str());
        return false;
    }
    *resman_endpoint = endpoint_;
    return true;
}

bool ResourceManagerImpl::GetStub() {
    ::galaxy::ins::sdk::SDKError err;
    bool ok = nexus_->Get(full_key_, &endpoint_, &err);
    if (!ok || err != ::galaxy::ins::sdk::kOK) {
        fprintf(stderr, "get resmananager endpoint from nexus failed: %s\n",
                ::galaxy::ins::sdk::InsSDK::StatusToString(err).c_str());
        return false;
    }
    if(!rpc_client_->GetStub(endpoint_, &res_stub_)) {
        fprintf(stderr, "connect resmanager fail, resmanager: %s\n", endpoint_.c_str());
        return false;
    }
    return true;
}

bool ResourceManagerImpl::EnterSafeMode(const EnterSafeModeRequest& request, 
                                        EnterSafeModeResponse* response) {
    
    ::baidu::galaxy::proto::EnterSafeModeRequest pb_request;
    ::baidu::galaxy::proto::EnterSafeModeResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::EnterSafeMode, 
                                        &pb_request, &pb_response, 5, 1);
    
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (pb_response.error_code().status() != ::baidu::galaxy::proto::kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::LeaveSafeMode(const LeaveSafeModeRequest& request, 
                                        LeaveSafeModeResponse* response) {
    ::baidu::galaxy::proto::LeaveSafeModeRequest pb_request;
    ::baidu::galaxy::proto::LeaveSafeModeResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::LeaveSafeMode, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }
    
    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (pb_response.error_code().status() != ::baidu::galaxy::proto::kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::Status(const StatusRequest& request, StatusResponse* response) {
    ::baidu::galaxy::proto::StatusRequest pb_request;
    ::baidu::galaxy::proto::StatusResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::Status, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (pb_response.error_code().status() != ::baidu::galaxy::proto::kOk) {
        return false;
    }

    response->alive_agents = pb_response.alive_agents();
    response->dead_agents = pb_response.dead_agents();
    response->total_agents = pb_response.total_agents();
    response->total_groups = pb_response.total_groups();
    response->total_containers = pb_response.total_containers();
    response->in_safe_mode = pb_response.in_safe_mode();
    
    response->cpu.total = pb_response.cpu().total();
    response->cpu.assigned = pb_response.cpu().assigned();
    response->cpu.used = pb_response.cpu().used();
    response->memory.total = pb_response.memory().total();
    response->memory.assigned = pb_response.memory().assigned();
    response->memory.used = pb_response.memory().used();
        
    for (int i = 0; i < pb_response.volum().size(); ++i) {
        const ::baidu::galaxy::proto::VolumResource& pb_volum = pb_response.volum(i);
        VolumResource volum;
        volum.medium = (::baidu::galaxy::sdk::VolumMedium)pb_volum.medium();
        volum.volum.total = pb_volum.volum().total();
        volum.volum.assigned = pb_volum.volum().assigned();
        volum.volum.used = pb_volum.volum().used();
        volum.device_path = pb_volum.device_path();
        response->volum.push_back(volum);
    }

    for (int i = 0; i < pb_response.pools().size(); ++i) {
        const ::baidu::galaxy::proto::PoolStatus& pb_status = pb_response.pools(i);
        PoolStatus status;
        status.name = pb_status.name();
        status.total_agents = pb_status.total_agents();
        status.alive_agents = pb_status.alive_agents();
        response->pools.push_back(status);
    }

    return true;
}

bool ResourceManagerImpl::CreateContainerGroup(const CreateContainerGroupRequest& request, 
                                               CreateContainerGroupResponse* response) {
    ::baidu::galaxy::proto::CreateContainerGroupRequest pb_request;
    ::baidu::galaxy::proto::CreateContainerGroupResponse pb_response;
   
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    
    if (request.replica <= 0 || request.replica >= 10000) {
        fprintf(stderr, "deploy replica must be greater than 0 and less than 10000\n");
        return false;
    }
    pb_request.set_replica(request.replica);
    
    if (request.name.empty()) {
        fprintf(stderr, "name must not be empty\n");
        return false;
    }
    pb_request.set_name(request.name);

    if (!FillContainerDescription(request.desc, pb_request.mutable_desc())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                            &::baidu::galaxy::proto::ResMan_Stub::CreateContainerGroup, 
                                            &pb_request, &pb_response, 5, 1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }
    
    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (pb_response.error_code().status() != ::baidu::galaxy::proto::kOk) {
        return false;
    }
    response->id = pb_response.id(); 
    return true;
}

bool ResourceManagerImpl::RemoveContainerGroup(const RemoveContainerGroupRequest& request, 
                                               RemoveContainerGroupResponse* response) {
    ::baidu::galaxy::proto::RemoveContainerGroupRequest pb_request;
    ::baidu::galaxy::proto::RemoveContainerGroupResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string id = Strim(request.id);

    if (id.empty()) {
        fprintf(stderr, "container id is needed\n");
        return false;
    }
    pb_request.set_id(id);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::RemoveContainerGroup, 
                                        &pb_request, &pb_response, 5,1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::UpdateContainerGroup(const UpdateContainerGroupRequest& request, 
                                                UpdateContainerGroupResponse* response) {
    ::baidu::galaxy::proto::UpdateContainerGroupRequest pb_request;
    ::baidu::galaxy::proto::UpdateContainerGroupResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    if (request.replica <= 0 || request.replica >= 10000) {
        fprintf(stderr, "deploy replica must be greater than 0 and less than 10000\n");
        return false;
    }
    pb_request.set_replica(request.replica);
    
    std::string id = Strim(request.id);
    if (id.empty()) {
        fprintf(stderr, "container id is needed\n");
        return false;
    }
    pb_request.set_id(id);

    if (request.interval < 0U || request.interval > 3600) {
        fprintf(stderr, "deploy interval must be greater than or equal to 0 and less than 3600\n");
        return false;
    }
    pb_request.set_interval(request.interval);

    if (!FillContainerDescription(request.desc, pb_request.mutable_desc())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::UpdateContainerGroup, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }
    
    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::ListContainerGroups(const ListContainerGroupsRequest& request, 
                                              ListContainerGroupsResponse* response) {
    ::baidu::galaxy::proto::ListContainerGroupsRequest pb_request;
    ::baidu::galaxy::proto::ListContainerGroupsResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ListContainerGroups, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }
        
    response->error_code.status = (baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    for (int i = 0; i < pb_response.containers().size(); ++i) {
        const ::baidu::galaxy::proto::ContainerGroupStatistics& pb_container = pb_response.containers(i);
        ContainerGroupStatistics container;
        container.id = pb_container.id();
        container.replica = pb_container.replica();
        container.ready = pb_container.ready();
        container.pending = pb_container.pending();
        container.allocating = pb_container.allocating();
        container.destroying = pb_container.destroying();
        container.cpu.total = pb_container.cpu().total();
        container.cpu.assigned = pb_container.cpu().assigned();
        container.cpu.used = pb_container.cpu().used();
        //container.memory.total = pb_container.memory().total();
        container.memory.assigned = pb_container.memory().assigned();
        container.memory.used = pb_container.memory().used();
        
        for (int j = 0; j < pb_container.volums().size(); ++j) {
            const ::baidu::galaxy::proto::VolumResource& pb_volum = pb_container.volums(j);
            VolumResource volum;
            volum.medium = (VolumMedium)pb_volum.medium();
            //volum.volum.total = pb_volum.volum().total();
            volum.volum.assigned = pb_volum.volum().assigned();
            volum.volum.used = pb_volum.volum().used();
            volum.device_path = pb_volum.device_path();
            container.volums.push_back(volum);
        }

        container.submit_time = pb_container.submit_time();
        container.update_time = pb_container.update_time();
        container.user_name = pb_container.user_name();
        container.container_type = (ContainerType)pb_container.container_type();
        response->containers.push_back(container);
    }
    return true;
}

bool ResourceManagerImpl::ShowContainerGroup(const ShowContainerGroupRequest& request, 
                                             ShowContainerGroupResponse* response) {
    
    ::baidu::galaxy::proto::ShowContainerGroupRequest pb_request;
    ::baidu::galaxy::proto::ShowContainerGroupResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string id = Strim(request.id);
    if (id.empty()) {
        fprintf(stderr, "container id is needed\n");
        return false;
    }
    pb_request.set_id(id);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ShowContainerGroup, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    response->desc.priority = (JobType)pb_response.desc().priority();
    response->desc.run_user = pb_response.desc().run_user();
    response->desc.version = pb_response.desc().version();
    response->desc.cmd_line = pb_response.desc().cmd_line();
    response->desc.max_per_host = pb_response.desc().max_per_host();
    response->desc.tag = pb_response.desc().tag();
    response->desc.container_type = (::baidu::galaxy::sdk::ContainerType)pb_response.desc().container_type();
    for (int i = 0; i < pb_response.desc().pool_names().size(); ++i) {
        response->desc.pool_names.push_back(pb_response.desc().pool_names(i));
    }

    response->desc.workspace_volum.size = pb_response.desc().workspace_volum().size();
    response->desc.workspace_volum.type = (VolumType)pb_response.desc().workspace_volum().type();
    response->desc.workspace_volum.medium = (VolumMedium)pb_response.desc().workspace_volum().medium();
    response->desc.workspace_volum.source_path = pb_response.desc().workspace_volum().source_path();
    response->desc.workspace_volum.dest_path = pb_response.desc().workspace_volum().dest_path();
    response->desc.workspace_volum.readonly = pb_response.desc().workspace_volum().readonly();
    response->desc.workspace_volum.exclusive = pb_response.desc().workspace_volum().exclusive();
    response->desc.workspace_volum.use_symlink = pb_response.desc().workspace_volum().use_symlink();

    for (int i = 0; i < pb_response.desc().data_volums().size(); ++i) {
        const ::baidu::galaxy::proto::VolumRequired& pb_volum = pb_response.desc().data_volums(i);
        VolumRequired volum;
        volum.size = pb_volum.size();
        volum.source_path = pb_volum.source_path();
        volum.dest_path = pb_volum.dest_path();
        volum.readonly = pb_volum.readonly();
        volum.exclusive = pb_volum.exclusive();
        volum.use_symlink = pb_volum.use_symlink();
        volum.medium = (VolumMedium)pb_volum.medium();
        volum.type = (VolumType)pb_volum.type();
        response->desc.data_volums.push_back(volum);
    }

    for (int i = 0; i < pb_response.desc().cgroups().size(); ++i) {
        const ::baidu::galaxy::proto::Cgroup& pb_cgroup = pb_response.desc().cgroups(i);
        Cgroup cgroup;
        cgroup.id = pb_cgroup.id();
        cgroup.cpu.milli_core = pb_cgroup.cpu().milli_core();
        cgroup.cpu.excess = pb_cgroup.cpu().excess();
        cgroup.memory.size = pb_cgroup.memory().size();
        cgroup.memory.excess = pb_cgroup.memory().excess();
        cgroup.blkio.weight = pb_cgroup.blkio().weight();
        cgroup.tcp_throt.recv_bps_quota = pb_cgroup.tcp_throt().recv_bps_quota();
        cgroup.tcp_throt.recv_bps_excess = pb_cgroup.tcp_throt().recv_bps_excess();
        cgroup.tcp_throt.send_bps_quota = pb_cgroup.tcp_throt().send_bps_quota();
        cgroup.tcp_throt.send_bps_excess = pb_cgroup.tcp_throt().send_bps_excess();
    
        for (int j = 0; j < pb_cgroup.ports().size(); ++j) {
            const ::baidu::galaxy::proto::PortRequired& pb_port = pb_cgroup.ports(j);
            PortRequired port;
            port.port_name = pb_port.port_name();
            port.port = pb_port.port();
            port.real_port = pb_port.real_port();
            cgroup.ports.push_back(port);
        }
        response->desc.cgroups.push_back(cgroup);

    }

    for (int i = 0; i < pb_response.containers().size(); ++i) {
        const ::baidu::galaxy::proto::ContainerStatistics& pb_container = pb_response.containers(i);
        ContainerStatistics container;
        container.id = pb_container.id();
        container.endpoint = pb_container.endpoint();
        container.last_res_err = (::baidu::galaxy::sdk::ResourceError)pb_container.last_res_err();
        container.status = (ContainerStatus)pb_container.status();
        container.cpu.total = pb_container.cpu().total();
        container.cpu.assigned = pb_container.cpu().assigned();
        container.cpu.used = pb_container.cpu().used();
        container.memory.total = pb_container.memory().total();
        container.memory.assigned = pb_container.memory().assigned();
        container.memory.used = pb_container.memory().used();
        
        for (int j = 0; j < pb_container.volums().size(); ++j) {
            const ::baidu::galaxy::proto::VolumResource& pb_volum = pb_container.volums(j);
            VolumResource volum;
            volum.medium = (VolumMedium)pb_volum.medium();
            volum.volum.total = pb_volum.volum().total();
            volum.volum.assigned = pb_volum.volum().assigned();
            volum.volum.used = pb_volum.volum().used();
            volum.device_path = pb_volum.device_path();
            container.volums.push_back(volum);
        }
        response->containers.push_back(container);
    }

    return true;
}

bool ResourceManagerImpl::AddAgent(const AddAgentRequest& request, 
                                   AddAgentResponse* response) {
    ::baidu::galaxy::proto::AddAgentRequest pb_request;
    ::baidu::galaxy::proto::AddAgentResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string endpoint = Strim(request.endpoint);

    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    } 
    pb_request.set_endpoint(endpoint);

    std::string pool = Strim(request.pool);
    if (pool.empty()) {
        fprintf(stderr, "pool is needed\n");
        return false;
    }
    pb_request.set_pool(pool);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::AddAgent, 
                                        &pb_request, &pb_response, 5, 1);

    if (!ok) { 
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::ShowAgent(const ShowAgentRequest& request, ShowAgentResponse* response) {
    ::baidu::galaxy::proto::ShowAgentRequest pb_request;
    ::baidu::galaxy::proto::ShowAgentResponse pb_response;
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string endpoint = Strim(request.endpoint);
    
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    } 
    pb_request.set_endpoint(endpoint);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ShowAgent,
                                        &pb_request, &pb_response, 5, 1);
 
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    
    for (int i = 0; i < pb_response.containers().size(); ++i) {
        const ::baidu::galaxy::proto::ContainerStatistics& pb_container = pb_response.containers(i);
        ContainerStatistics container;
        container.id = pb_container.id();
        container.endpoint = pb_container.endpoint();
        container.last_res_err = (::baidu::galaxy::sdk::ResourceError)pb_container.last_res_err();
        container.status = (ContainerStatus)pb_container.status();
        container.cpu.total = pb_container.cpu().total();
        container.cpu.assigned = pb_container.cpu().assigned();
        container.cpu.used = pb_container.cpu().used();
        container.memory.total = pb_container.memory().total();
        container.memory.assigned = pb_container.memory().assigned();
        container.memory.used = pb_container.memory().used();
        
        for (int j = 0; j < pb_container.volums().size(); ++j) {
            const ::baidu::galaxy::proto::VolumResource& pb_volum = pb_container.volums(j);
            VolumResource volum;
            volum.medium = (VolumMedium)pb_volum.medium();
            volum.volum.total = pb_volum.volum().total();
            volum.volum.assigned = pb_volum.volum().assigned();
            volum.volum.used = pb_volum.volum().used();
            volum.device_path = pb_volum.device_path();
            container.volums.push_back(volum);
        }
        response->containers.push_back(container);
    }

    return true;

}

bool ResourceManagerImpl::RemoveAgent(const RemoveAgentRequest& request, 
                                      RemoveAgentResponse* response) {
    ::baidu::galaxy::proto::RemoveAgentRequest pb_request;
    ::baidu::galaxy::proto::RemoveAgentResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string endpoint = Strim(request.endpoint); 
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    } 
    pb_request.set_endpoint(endpoint);
    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::RemoveAgent, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::OnlineAgent(const OnlineAgentRequest& request, 
                                      OnlineAgentResponse* response) {
    ::baidu::galaxy::proto::OnlineAgentRequest pb_request;
    ::baidu::galaxy::proto::OnlineAgentResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string endpoint = Strim(request.endpoint);
    
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    } 
    pb_request.set_endpoint(endpoint);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::OnlineAgent, 
                                        &pb_request, &pb_response, 5, 1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::OfflineAgent(const OfflineAgentRequest& request, 
                                       OfflineAgentResponse* response) {
    ::baidu::galaxy::proto::OfflineAgentRequest pb_request;
    ::baidu::galaxy::proto::OfflineAgentResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string endpoint = Strim(request.endpoint);
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    }
    pb_request.set_endpoint(endpoint);
    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::OfflineAgent, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::ListAgents(const ListAgentsRequest& request, 
                                     ListAgentsResponse* response) {
    ::baidu::galaxy::proto::ListAgentsRequest pb_request;
    ::baidu::galaxy::proto::ListAgentsResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ListAgents, 
                                        &pb_request, &pb_response, 5, 1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    for (int i = 0; i < pb_response.agents().size(); ++i) {
        const ::baidu::galaxy::proto::AgentStatistics& pb_agent = pb_response.agents(i);
        AgentStatistics agent;
        agent.endpoint = pb_agent.endpoint();
        agent.status = (AgentStatus)pb_agent.status();
        agent.pool = pb_agent.pool();
        agent.cpu.total = pb_agent.cpu().total();
        agent.cpu.assigned = pb_agent.cpu().assigned();
        agent.cpu.used = pb_agent.cpu().used();
        agent.memory.total = pb_agent.memory().total();
        agent.memory.assigned = pb_agent.memory().assigned();
        agent.memory.used = pb_agent.memory().used();
        for (int j = 0; j < pb_agent.volums().size(); ++j) {
            const ::baidu::galaxy::proto::VolumResource& pb_volum = pb_agent.volums(j);
            VolumResource volum;
            volum.medium = (VolumMedium)pb_volum.medium();
            volum.volum.total = pb_volum.volum().total();
            volum.volum.assigned = pb_volum.volum().assigned();
            volum.volum.used = pb_volum.volum().used();
            volum.device_path = pb_volum.device_path();
            agent.volums.push_back(volum);
        }
        agent.total_containers = pb_agent.total_containers();
        
        for (int j = 0; j < pb_agent.tags().size(); ++j) {
            agent.tags.push_back(pb_agent.tags(j));
        }
        response->agents.push_back(agent);
    }

    return true;
}

bool ResourceManagerImpl::CreateTag(const CreateTagRequest& request, CreateTagResponse* response) {
    ::baidu::galaxy::proto::CreateTagRequest pb_request;
    ::baidu::galaxy::proto::CreateTagResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string tag = Strim(request.tag);

    if (tag.empty()) {
        fprintf(stderr, "tag is needed\n");
        return false;
    }
    pb_request.set_tag(tag);

    bool ok = true;
    for(size_t i = 0; i < request.endpoint.size(); ++i) {
        std::string endpoint = Strim(request.endpoint[i]);
        if (endpoint.empty() || !CheckEndPoint(endpoint)) {
            ok =  false;
            break;
        }
        pb_request.add_endpoint(endpoint);
    }

    if (!ok) {
        fprintf(stderr, "endpoint cannot be empty\n");
        return false;
    }
    
    ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::CreateTag, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    return true;
}

bool ResourceManagerImpl::ListTags(const ListTagsRequest& request, ListTagsResponse* response) {
    ::baidu::galaxy::proto::ListTagsRequest  pb_request;
    ::baidu::galaxy::proto::ListTagsResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ListTags, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    for (int i = 0; i < pb_response.tags().size(); ++i) {
        response->tags.push_back(pb_response.tags(i));
    }

    return true;
}

bool ResourceManagerImpl::ListAgentsByTag(const ListAgentsByTagRequest& request, 
                                          ListAgentsByTagResponse* response) {
    ::baidu::galaxy::proto::ListAgentsByTagRequest  pb_request;
    ::baidu::galaxy::proto::ListAgentsByTagResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string tag = Strim(request.tag);
    if (tag.empty()) {
        fprintf(stderr, "tag is needed\n");
        return false;
    }
    pb_request.set_tag(tag);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ListAgentsByTag, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    for (int i = 0; i < pb_response.agents().size(); ++i) {
        const ::baidu::galaxy::proto::AgentStatistics& pb_agent = pb_response.agents(i);
        AgentStatistics agent;
        agent.endpoint = pb_agent.endpoint();
        agent.status = (AgentStatus)pb_agent.status();
        agent.pool = pb_agent.pool();
        agent.cpu.total = pb_agent.cpu().total();
        agent.cpu.assigned = pb_agent.cpu().assigned();
        agent.cpu.used = pb_agent.cpu().used();
        agent.memory.total = pb_agent.memory().total();
        agent.memory.assigned = pb_agent.memory().assigned();
        agent.memory.used = pb_agent.memory().used();
        for (int j = 0; j < pb_agent.volums().size(); ++j) {
            const ::baidu::galaxy::proto::VolumResource& pb_volum = pb_agent.volums(j);
            VolumResource volum;
            volum.medium = (VolumMedium)pb_volum.medium();
            volum.volum.total = pb_volum.volum().total();
            volum.volum.assigned = pb_volum.volum().assigned();
            volum.volum.used = pb_volum.volum().used();
            volum.device_path = pb_volum.device_path();
            agent.volums.push_back(volum);
        }
        agent.total_containers = pb_agent.total_containers();
        
        for (int j = 0; j < pb_agent.tags().size(); ++j) {
            agent.tags.push_back(pb_agent.tags(j));
        }
        response->agents.push_back(agent);
    }

    return true;
}

bool ResourceManagerImpl::GetTagsByAgent(const GetTagsByAgentRequest& request, 
                                         GetTagsByAgentResponse* response) {
    ::baidu::galaxy::proto::GetTagsByAgentRequest pb_request;
    ::baidu::galaxy::proto::GetTagsByAgentResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string endpoint = Strim(request.endpoint);
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    }
    pb_request.set_endpoint(endpoint);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::GetTagsByAgent, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    for (int i = 0; i < pb_response.tags().size(); ++i) {
        response->tags.push_back(pb_response.tags(i));
    }

    return true;
}

bool ResourceManagerImpl::AddAgentToPool(const AddAgentToPoolRequest& request, 
                                         AddAgentToPoolResponse* response) {
    ::baidu::galaxy::proto::AddAgentToPoolRequest pb_request;
    ::baidu::galaxy::proto::AddAgentToPoolResponse pb_response;

    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    
    std::string endpoint = Strim(request.endpoint);
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    }
    pb_request.set_endpoint(endpoint);
    
    std::string pool = Strim(request.pool);
    if (pool.empty()) {
        fprintf(stderr, "pool is needed\n");
        return false;
    }

    pb_request.set_pool(pool);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::AddAgentToPool, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    return true;
}

bool ResourceManagerImpl::RemoveAgentFromPool(const RemoveAgentFromPoolRequest& request, 
                                              RemoveAgentFromPoolResponse* response) {
    ::baidu::galaxy::proto::RemoveAgentFromPoolRequest pb_request;
    ::baidu::galaxy::proto::RemoveAgentFromPoolResponse pb_response;
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string endpoint = Strim(request.endpoint);
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    }
    pb_request.set_endpoint(endpoint);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::RemoveAgentFromPool, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    return true;
}

bool ResourceManagerImpl::ListAgentsByPool(const ListAgentsByPoolRequest& request, 
                                           ListAgentsByPoolResponse* response) {
    ::baidu::galaxy::proto::ListAgentsByPoolRequest pb_request;
    ::baidu::galaxy::proto::ListAgentsByPoolResponse pb_response;
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    std::string pool = Strim(request.pool);
    if (pool.empty()) {
        fprintf(stderr, "pool is needed\n");
        return false;
    }
    pb_request.set_pool(pool);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ListAgentsByPool, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    for (int i = 0; i < pb_response.agents().size(); ++i) {
        const ::baidu::galaxy::proto::AgentStatistics& pb_agent = pb_response.agents(i);
        AgentStatistics agent;
        agent.endpoint = pb_agent.endpoint();
        agent.status = (AgentStatus)pb_agent.status();
        agent.pool = pb_agent.pool();
        agent.cpu.total = pb_agent.cpu().total();
        agent.cpu.assigned = pb_agent.cpu().assigned();
        agent.cpu.used = pb_agent.cpu().used();
        agent.memory.total = pb_agent.memory().total();
        agent.memory.assigned = pb_agent.memory().assigned();
        agent.memory.used = pb_agent.memory().used();
        for (int j = 0; j < pb_agent.volums().size(); ++j) {
            const ::baidu::galaxy::proto::VolumResource& pb_volum = pb_agent.volums(j);
            VolumResource volum;
            volum.medium = (VolumMedium)pb_volum.medium();
            volum.volum.total = pb_volum.volum().total();
            volum.volum.assigned = pb_volum.volum().assigned();
            volum.volum.used = pb_volum.volum().used();
            volum.device_path = pb_volum.device_path();
            agent.volums.push_back(volum);
        }
        agent.total_containers = pb_agent.total_containers();
        
        for (int j = 0; j < pb_agent.tags().size(); ++j) {
            agent.tags.push_back(pb_agent.tags(j));
        }
        response->agents.push_back(agent);
    }

    return true;
}

bool ResourceManagerImpl::GetPoolByAgent(const GetPoolByAgentRequest& request, 
                                         GetPoolByAgentResponse* response) {
    ::baidu::galaxy::proto::GetPoolByAgentRequest pb_request;
    ::baidu::galaxy::proto::GetPoolByAgentResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    
    std::string endpoint = Strim(request.endpoint);
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    }
    pb_request.set_endpoint(endpoint);
    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::GetPoolByAgent, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    response->pool = pb_response.pool();
    return true;
}

bool ResourceManagerImpl::AddUser(const AddUserRequest& request, AddUserResponse* response) {
    ::baidu::galaxy::proto::AddUserRequest pb_request;
    ::baidu::galaxy::proto::AddUserResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        fprintf(stderr, "user error\n");
        return false;
    }

    if (!FillUser(request.admin, pb_request.mutable_admin())) {
        fprintf(stderr, "admin error\n");
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::AddUser, 
                                        &pb_request, &pb_response, 5, 1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::RemoveUser(const RemoveUserRequest& request, 
                                     RemoveUserResponse* response) {
    ::baidu::galaxy::proto::RemoveUserRequest pb_request;
    ::baidu::galaxy::proto::RemoveUserResponse pb_response;

    std::string user = Strim(request.user.user);
    if (user.empty()) {
        fprintf(stderr, "user should not be empty\n");
        return false;
    }
    pb_request.mutable_user()->set_user(user);

    if (!FillUser(request.admin, pb_request.mutable_admin())) {
        fprintf(stderr, "admin error\n");
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::RemoveUser, 
                                        &pb_request, &pb_response, 5, 1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

bool ResourceManagerImpl::ListUsers(const ListUsersRequest& request, ListUsersResponse* response) {
    ::baidu::galaxy::proto::ListUsersRequest pb_request;
    ::baidu::galaxy::proto::ListUsersResponse pb_response;

    if(!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ListUsers, 
                                        &pb_request, &pb_response, 5, 1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    for (int i = 0; i < pb_response.user().size(); ++i) {
        response->user.push_back(pb_response.user(i));
    }
    return true;
}

bool ResourceManagerImpl::ShowUser(const ShowUserRequest& request, ShowUserResponse* response) {
    ::baidu::galaxy::proto::ShowUserRequest pb_request;
    ::baidu::galaxy::proto::ShowUserResponse pb_response;

    std::string user = Strim(request.user.user);
    if(user.empty()) {
        fprintf(stderr, "user should not be empty\n");
        return false;
    }
    pb_request.mutable_user()->set_user(user);

    if (!FillUser(request.admin, pb_request.mutable_admin())) {
        fprintf(stderr, "admin error\n");
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::ShowUser, 
                                        &pb_request, &pb_response, 5, 1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    response->quota.millicore = pb_response.quota().millicore();
    response->quota.memory = pb_response.quota().memory();
    response->quota.disk = pb_response.quota().disk();
    response->quota.ssd = pb_response.quota().ssd();
    response->quota.replica = pb_response.quota().replica();
    
    response->assigned.millicore = pb_response.assigned().millicore();
    response->assigned.memory = pb_response.assigned().memory();
    response->assigned.disk = pb_response.assigned().disk();
    response->assigned.ssd = pb_response.assigned().ssd();
    response->assigned.replica = pb_response.assigned().replica();

    for (int i = 0; i < pb_response.grants().size(); ++i) {
        Grant grant;
        grant.pool = pb_response.grants(i).pool();
        grant.action = (AuthorityAction)pb_response.grants(i).action();
        for (int j = 0; j < pb_response.grants(i).authority().size(); ++j) {
            grant.authority.push_back((Authority)pb_response.grants(i).authority(j));
        }
        response->grants.push_back(grant);
    }

    return true;
}

bool ResourceManagerImpl::GrantUser(const GrantUserRequest& request, GrantUserResponse* response) {
    ::baidu::galaxy::proto::GrantUserRequest pb_request;
    ::baidu::galaxy::proto::GrantUserResponse pb_response;

    std::string user = Strim(request.user.user);
    if (user.empty()) {
        fprintf(stderr, "user should not be empty\n");
        return false;
    }
    pb_request.mutable_user()->set_user(user);

    if (!FillUser(request.admin, pb_request.mutable_admin())) {
        fprintf(stderr, "admin error\n");
        return false;
    }

    if (!FillGrant(request.grant, pb_request.mutable_grant())) {
        return false;
    }

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::GrantUser, 
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    return true;
}

bool ResourceManagerImpl::AssignQuota(const AssignQuotaRequest& request, 
                                      AssignQuotaResponse* response) {
    ::baidu::galaxy::proto::AssignQuotaRequest pb_request;
    ::baidu::galaxy::proto::AssignQuotaResponse pb_response;
    
    std::string user = Strim(request.user.user);
    if (user.empty()) {
        fprintf(stderr, "user should not be empty\n");
        return false;
    }
    pb_request.mutable_user()->set_user(user);

    if (!FillUser(request.admin, pb_request.mutable_admin())) {
        fprintf(stderr, "admin error\n");
        return false;
    }

    ::baidu::galaxy::proto::Quota* quota = pb_request.mutable_quota();

    if (request.quota.millicore <= 0) {
        fprintf(stderr, "cpu millicores must be greater than 0\n");
        return false;
    }
    quota->set_millicore(request.quota.millicore);
    
    if (request.quota.memory <= 0) {
        fprintf(stderr, "memory size must be greater than 0\n");
        return false;
    }
    quota->set_memory(request.quota.memory);

    if (request.quota.disk <= 0) {
        fprintf(stderr, "disk size must be greater than 0\n");
        return false;
    }
    quota->set_disk(request.quota.disk);

    if (request.quota.ssd <= 0) {
        fprintf(stderr, "ssd size must be greater than 0\n");
        return false;
    }
    quota->set_ssd(request.quota.ssd);

    if (request.quota.replica <= 0) {
        fprintf(stderr, "assign replica must be greater than 0\n");
        return false;
    }
    quota->set_replica(request.quota.replica);

    bool ok = rpc_client_->SendRequest(res_stub_, 
                                        &::baidu::galaxy::proto::ResMan_Stub::AssignQuota, 
                                        &pb_request, &pb_response, 5, 1);

    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }

    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }

    return true;
}

bool ResourceManagerImpl::Preempt(const PreemptRequest& request, PreemptResponse* response) {
    ::baidu::galaxy::proto::PreemptRequest pb_request;
    ::baidu::galaxy::proto::PreemptResponse pb_response;
    
    if (!FillUser(request.user, pb_request.mutable_user())) {
        return false;
    }
    
    std::string id = Strim(request.container_group_id);
    if (id.empty()) {
        fprintf(stderr, "container_group_id must not be empty\n");
        return false;
    }
    pb_request.set_container_group_id(id);

    std::string endpoint = Strim(request.endpoint);
    if (endpoint.empty() || !CheckEndPoint(endpoint)) {
        fprintf(stderr, "endpoint must not be empty\n");
        return false;
    }
    pb_request.set_endpoint(endpoint);

    bool ok = rpc_client_->SendRequest(res_stub_,
                                        &::baidu::galaxy::proto::ResMan_Stub::Preempt,
                                        &pb_request, &pb_response, 5, 1);
    if (!ok) {
        response->error_code.reason = "ResourceManager Rpc SendRequest failed";
        return false;
    }
    response->error_code.status = (::baidu::galaxy::sdk::Status)pb_response.error_code().status();
    response->error_code.reason = pb_response.error_code().reason();
    if (response->error_code.status != kOk) {
        return false;
    }
    return true;
}

ResourceManager* ResourceManager::ConnectResourceManager(const std::string& nexus_addr, 
                                                         const std::string& path) {
    ResourceManagerImpl* resman = new ResourceManagerImpl(nexus_addr, path);
    if (!resman->GetStub()) {
        delete resman;
        return NULL;
    }
    return resman;
}

} //namespace sdk
} //namespace galaxy
} //namespace baidu
