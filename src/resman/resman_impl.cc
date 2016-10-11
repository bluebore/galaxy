// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "resman_impl.h"
#include <string>
#include <sstream>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <sys/utsname.h>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

DECLARE_string(nexus_root);
DECLARE_string(nexus_addr);
DECLARE_int32(agent_timeout);
DECLARE_int32(agent_query_interval);
DECLARE_int32(container_group_max_replica);
DECLARE_double(safe_mode_percent);

const std::string sAgentPrefix = "/agent";
const std::string sUserPrefix = "/user";
const std::string sContainerGroupPrefix = "/container_group";
const std::string sTagPrefix = "/tag";
const std::string sRMLock = "/resman_lock";
const std::string sRMAddr = "/resman";

#define CHECK_USER() do {\
    if (!CheckUserExist(request, response, done)) {\
        LOG(WARNING) << "rpc refused, method:" << __FUNCTION__;\
        return;\
    }\
} while(0);

namespace baidu {
namespace galaxy {

ResManImpl::ResManImpl() : scheduler_(new sched::Scheduler()),
                           safe_mode_(true),
                           force_safe_mode_(false), 
                           start_time_(0) {
    nexus_ = new InsSDK(FLAGS_nexus_addr);
}

ResManImpl::~ResManImpl() {
    delete scheduler_;
    delete nexus_;
}

bool ResManImpl::Init() {
    bool load_ok = false;
    load_ok = LoadObjects(sAgentPrefix, agents_);
    if (!load_ok) {
        LOG(WARNING) << "fail to load agent meta";
        return false;
    }
    std::map<std::string, proto::AgentMeta>::const_iterator agent_it;
    for (agent_it = agents_.begin(); agent_it != agents_.end(); agent_it++) {
        const std::string& endpoint = agent_it->first;
        const proto::AgentMeta& agent_meta = agent_it->second;
        pools_[agent_meta.pool()].insert(endpoint);
    }
    
    std::map<std::string, proto::TagMeta> tag_map;
    load_ok = LoadObjects(sTagPrefix, tag_map);
    if (!load_ok) {
        LOG(WARNING) << "fail to load tags meta";
        return false;
    }
    for (std::map<std::string, proto::TagMeta>::const_iterator tag_it = tag_map.begin();
         tag_it != tag_map.end(); tag_it++) {
        const std::string& tag_name = tag_it->first;
        const proto::TagMeta& tag_meta = tag_it->second;
        for (int i = 0 ; i < tag_meta.endpoints_size(); i++) {
            tags_[tag_name].insert(tag_meta.endpoints(i));
            agent_tags_[tag_meta.endpoints(i)].insert(tag_name);
        }
    }

    load_ok = LoadObjects(sUserPrefix, users_);
    if (!load_ok) {
        LOG(WARNING) << "fail to load user meta";
        return false;
    }
    
    ReloadUsersAuth();
    load_ok = LoadObjects(sContainerGroupPrefix, container_groups_);
    if (!load_ok) {
        LOG(WARNING) << "failt to load container groups meta";
        return false;
    }
    std::map<std::string, proto::ContainerGroupMeta>::const_iterator it;
    for (it = container_groups_.begin(); it != container_groups_.end(); it++) {
        const proto::ContainerGroupMeta& container_group_meta = it->second;
        LOG(INFO) << "scheduler reaload: " << container_group_meta.id();
        scheduler_->Reload(container_group_meta);
        VLOG(10) << "TRACE BEGIN, meta of " << container_group_meta.id()
                 << "\n" << container_group_meta.DebugString()
                 << "TRACE END";
    }
    start_time_ = common::timer::get_micros();
    return true;
}

void ResManImpl::ReloadUsersAuth() {
    std::map<std::string, proto::UserMeta>::const_iterator user_it;
    for (user_it = users_.begin(); user_it != users_.end(); user_it++) {
        const std::string& user_name = user_it->first;
        const proto::UserMeta& user_meta = user_it->second;
        for (int i = 0; i < user_meta.grants_size(); i++) {
            const proto::Grant& grant = user_meta.grants(i);
            const std::string& pool_name = grant.pool();
            for (int j = 0; j < grant.authority_size(); j++) {
                const proto::Authority& auth = grant.authority(j);
                if (auth == proto::kAuthorityCreateContainer) {
                    users_can_create_[pool_name].insert(user_name);
                } else if (auth == proto::kAuthorityUpdateContainer) {
                    users_can_update_[pool_name].insert(user_name);
                } else if (auth == proto::kAuthorityRemoveContainer) {
                    users_can_remove_[pool_name].insert(user_name);
                } else if (auth == proto::kAuthorityListContainer) {
                    users_can_list_[pool_name].insert(user_name);
                }
            }
        }
    }
}

bool ResManImpl::RegisterOnNexus(const std::string& endpoint) {
    ::galaxy::ins::sdk::SDKError err;
    bool ret = nexus_->Lock(FLAGS_nexus_root + sRMLock, &err);
    if (!ret) {
        LOG(WARNING) << "failed to acquire resman lock, " << err;
        return false;
    }
    ret = nexus_->Put(FLAGS_nexus_root + sRMAddr, endpoint, &err);
    if (!ret) {
        LOG(WARNING) << "failed to write resman endpoint to nexus, " << err;
        return false;
    }
    ret = nexus_->Watch(FLAGS_nexus_root + sRMLock, &OnRMLockChange, this, &err);
    if (!ret) {
        LOG(WARNING) << "failed to watch resman lock, " << err;
        return false;
    }
    return true;
}

void ResManImpl::OnRMLockChange(const ::galaxy::ins::sdk::WatchParam& param,
                                ::galaxy::ins::sdk::SDKError err) {
    ResManImpl* rm = static_cast<ResManImpl*>(param.context);
    rm->OnLockChange(param.value);
}


void ResManImpl::OnLockChange(std::string lock_session_id) {
    std::string self_session_id = nexus_->GetSessionID();
    if (self_session_id != lock_session_id) {
        LOG(FATAL) << "RM lost lock , die.";
    }
}

void ResManImpl::EnterSafeMode(::google::protobuf::RpcController* controller,
                               const ::baidu::galaxy::proto::EnterSafeModeRequest* request,
                               ::baidu::galaxy::proto::EnterSafeModeResponse* response,
                               ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (!safe_mode_) {
        safe_mode_ = true;
        force_safe_mode_ = true;
        scheduler_->Stop();
        response->mutable_error_code()->set_status(proto::kOk);
    } else {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("already in safe mode");
    }
    done->Run();
}

void ResManImpl::LeaveSafeMode(::google::protobuf::RpcController* controller,
                               const ::baidu::galaxy::proto::LeaveSafeModeRequest* request,
                               ::baidu::galaxy::proto::LeaveSafeModeResponse* response,
                               ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (safe_mode_) {
        safe_mode_ = false;
        force_safe_mode_ = false;
        scheduler_->Start();
        response->mutable_error_code()->set_status(proto::kOk);
    } else {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("invalid op, cluster is not in safe mode");
    }
    done->Run();
}

void ResManImpl::Status(::google::protobuf::RpcController* controller,
                        const ::baidu::galaxy::proto::StatusRequest* request,
                        ::baidu::galaxy::proto::StatusResponse* response,
                        ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    uint32_t total_agents = agents_.size();
    uint32_t alive_agents = 0;
    uint32_t dead_agents = 0;
    int64_t cpu_total = 0;
    int64_t cpu_assigned = 0;
    int64_t cpu_used = 0;
    int64_t memory_total = 0;
    int64_t memory_assigned = 0;
    int64_t memory_used = 0;
    int64_t total_containers = 0;
    int64_t total_container_groups = container_groups_.size();
    std::map<proto::VolumMedium, int64_t> volum_total;
    std::map<proto::VolumMedium, int64_t> volum_assigned;
    std::map<proto::VolumMedium, int64_t> volum_used; 
    std::map<std::string, AgentStat>::const_iterator it;
    std::map<std::string, int64_t> pool_total;
    std::map<std::string, int64_t> pool_alive;
    for (it = agent_stats_.begin(); it != agent_stats_.end(); it ++) { 
        const std::string& endpoint = it->first;
        if (agents_.find(endpoint) == agents_.end()) {
            continue;
        }
        std::string pool_name = agents_[endpoint].pool();
        proto::AgentStatus agent_status = it->second.status;
        const proto::AgentInfo& agent_info = it->second.info;
        pool_total[pool_name]++;
        if (agent_status == proto::kAgentAlive) {
            alive_agents++;
            pool_alive[pool_name]++;
        } else if (agent_status == proto::kAgentDead) {
            dead_agents++;
        }
        total_containers += agent_info.container_info_size();
        cpu_total += agent_info.cpu_resource().total();
        cpu_assigned += agent_info.cpu_resource().assigned();
        cpu_used += agent_info.cpu_resource().used();
        memory_total += agent_info.memory_resource().total();
        memory_assigned += agent_info.memory_resource().assigned();
        memory_used += agent_info.memory_resource().used();
        for (int i = 0; i < agent_info.volum_resources_size(); i++) {
            proto::VolumMedium medium = agent_info.volum_resources(i).medium(); 
            const proto::Resource& vs = agent_info.volum_resources(i).volum();
            volum_total[medium] += vs.total();
            volum_assigned[medium] += vs.assigned();
            volum_used[medium] += vs.used();
        }
    }
    response->mutable_error_code()->set_status(proto::kOk);
    response->mutable_cpu()->set_total(cpu_total);
    response->mutable_cpu()->set_assigned(cpu_assigned);
    response->mutable_cpu()->set_used(cpu_used);
    response->mutable_memory()->set_total(memory_total);
    response->mutable_memory()->set_assigned(memory_assigned);
    response->mutable_memory()->set_used(memory_used);
    response->set_total_agents(total_agents);
    response->set_alive_agents(alive_agents);
    response->set_dead_agents(dead_agents);
    std::map<proto::VolumMedium,int64_t>::const_iterator v_it;
    for (v_it = volum_total.begin(); v_it != volum_total.end(); v_it++) {
        proto::VolumMedium medium = v_it->first;
        int64_t size_total = v_it->second;
        proto::VolumResource* vrs = response->add_volum();
        vrs->mutable_volum()->set_total(size_total);
        vrs->mutable_volum()->set_assigned(volum_assigned[medium]);
        vrs->mutable_volum()->set_used(volum_used[medium]);
        vrs->set_medium(medium);
    }
    response->set_total_containers(total_containers);
    response->set_total_groups(total_container_groups);
    std::map<std::string, int64_t >::const_iterator p_it;
    for (p_it = pool_total.begin(); p_it != pool_total.end(); p_it++) {
        const std::string& pool_name = p_it->first;
        proto::PoolStatus* pool_status = response->add_pools();
        pool_status->set_total_agents(p_it->second);
        pool_status->set_name(pool_name);
        pool_status->set_alive_agents(pool_alive[pool_name]);
    }
    response->set_in_safe_mode(safe_mode_);
    VLOG(10) << "cluster status:" << response->DebugString();
    done->Run();
}

void ResManImpl::QueryAgent(const std::string& agent_endpoint, bool is_first_query) {
    MutexLock lock(&mu_);
    std::map<std::string, AgentStat>::iterator agent_it;
    agent_it = agent_stats_.find(agent_endpoint);
    if (agent_it == agent_stats_.end()) {
        LOG(WARNING) << "no need to query on expired agent: " << agent_endpoint;
        return;
    }
    AgentStat& agent = agent_it->second;
    int32_t now_tm = common::timer::now_time();
    VLOG(10) << agent_endpoint << ",  last:" << agent.last_heartbeat_time 
             << ",timeout:" << FLAGS_agent_timeout
             << ",now_tm:" << now_tm;
    if (agent.last_heartbeat_time + FLAGS_agent_timeout < now_tm) {
        LOG(WARNING) << "this agent maybe dead:" << agent_endpoint;
        agent.status = proto::kAgentDead;
        scheduler_->RemoveAgent(agent_endpoint);
        query_pool_.DelayTask(FLAGS_agent_query_interval * 1000,
            boost::bind(&ResManImpl::QueryAgent, this, agent_endpoint, true)
        );
        return;
    }
    proto::Agent_Stub* stub;
    rpc_client_.GetStub(agent_endpoint, &stub);
    boost::scoped_ptr<proto::Agent_Stub> stub_guard(stub);
    boost::function<void (const proto::QueryRequest*, 
                          proto::QueryResponse*, bool, int)> callback;
    callback = boost::bind(&ResManImpl::QueryAgentCallback, this, 
                           agent_endpoint, is_first_query,
                           _1, _2, _3, _4);
    proto::QueryRequest* request = new proto::QueryRequest();
    request->set_full_report(is_first_query);
    proto::QueryResponse* response = new proto::QueryResponse();
    rpc_client_.AsyncRequest(stub, &proto::Agent_Stub::Query,
                             request, response, callback, 5, 1);
    VLOG(10) << "send query command to:" << agent_endpoint;
}

void ResManImpl::QueryAgentCallback(std::string agent_endpoint,
                                    bool is_first_query,
                                    const proto::QueryRequest* request,
                                    proto::QueryResponse* response,
                                    bool rpc_fail, int err) {
    boost::scoped_ptr<const proto::QueryRequest> request_guard(request);
    boost::scoped_ptr<proto::QueryResponse> response_guard(response);
    if (response->code().status() != proto::kOk || rpc_fail) {
        LOG(WARNING) << "failed to query on: " << agent_endpoint
                     << " err: " << err << ", rpc_fail:" << rpc_fail;
        query_pool_.DelayTask(FLAGS_agent_query_interval * 1000,
            boost::bind(&ResManImpl::QueryAgent, this, agent_endpoint, is_first_query)
        );
        return;
    }
    if (is_first_query) {
        MutexLock lock(&mu_);
        std::map<std::string, proto::AgentMeta>::iterator agent_it 
            = agents_.find(agent_endpoint);
        if (agent_it == agents_.end()) {
            LOG(WARNING) << "query result for expired agent:" << agent_endpoint;
            return;
        }
        proto::AgentMeta& agent_meta = agent_it->second;
        const proto::AgentInfo& agent_info = response->agent_info();
        int64_t cpu = agent_info.cpu_resource().total();
        int64_t memory = agent_info.memory_resource().total();
        std::map<sched::DevicePath, sched::VolumInfo> volums;
        for (int i = 0; i < agent_info.volum_resources_size(); i++) {
            const proto::VolumResource& vres = agent_info.volum_resources(i);
            sched::VolumInfo& vinfo = volums[vres.device_path()];
            vinfo.size = vres.volum().total();
            vinfo.medium = vres.medium();
        }
        const std::set<std::string>& tags = agent_tags_[agent_endpoint];
        std::string pool_name = agent_meta.pool();
        sched::Agent::Ptr agent(new sched::Agent(agent_endpoint,
                                                 cpu, memory,
                                                 volums,
                                                 tags,
                                                 pool_name));
        scheduler_->RemoveAgent(agent_endpoint);
        scheduler_->AddAgent(agent, agent_info);
        LOG(INFO) << "TRACE BEGIN, first query result from:" << agent_endpoint
                  << "\n" << agent_info.DebugString()
                  << "\nTRACE END";
        is_first_query = false;
    } else {
        VLOG(10) << "TRACE BEGIN, query result from: " << agent_endpoint
                 << "\n" << response->agent_info().DebugString()
                 << "\nTRACE END";
        std::vector<sched::AgentCommand> commands;
        scheduler_->MakeCommand(agent_endpoint, response->agent_info(), commands);
        SendCommandsToAgent(agent_endpoint, commands);
    }

    bool leave_safe_mode_event = false;
    {
        MutexLock lock(&mu_);
        if (agent_stats_.find(agent_endpoint) == agent_stats_.end()) {
            LOG(INFO) << "this agent may be removed, no need to query again";
            return;
        }
        agent_stats_[agent_endpoint].info = response->agent_info();
        if (!force_safe_mode_ && 
            safe_mode_ && 
            agent_stats_.size() > (double)agents_.size() * FLAGS_safe_mode_percent) {
            int64_t running_time = (common::timer::get_micros() - start_time_) / 1000000;
            LOG(INFO) << "running time: " << running_time << " seconds";
            if ( running_time > FLAGS_agent_timeout) {
                LOG(INFO) << "leave safe mode";
                safe_mode_ = false;
                leave_safe_mode_event = true;
            }
        }
    }
    if (leave_safe_mode_event) {
        scheduler_->Start();
    }
    //query again later
    query_pool_.DelayTask(FLAGS_agent_query_interval * 1000,
        boost::bind(&ResManImpl::QueryAgent, this, agent_endpoint, is_first_query)
    );
}

void ResManImpl::SendCommandsToAgent(const std::string& agent_endpoint,
                                     const std::vector<sched::AgentCommand>& commands) {
    std::vector<sched::AgentCommand>::const_iterator it;
    for (it = commands.begin(); it != commands.end(); it++) {
        const sched::AgentCommand& cmd = *it;
        proto::Agent_Stub* stub;
        rpc_client_.GetStub(agent_endpoint, &stub);
        boost::scoped_ptr<proto::Agent_Stub> stub_guard(stub);
        if (cmd.action == sched::kCreateContainer) {
            proto::CreateContainerRequest* request = new proto::CreateContainerRequest();
            proto::CreateContainerResponse* response = new proto::CreateContainerResponse();
            request->set_id(cmd.container_id);
            request->set_container_group_id(cmd.container_group_id);
            request->mutable_container()->CopyFrom(cmd.desc);
            boost::function<void (const proto::CreateContainerRequest*,
                                  proto::CreateContainerResponse*,
                                  bool, int)> callback;
            callback = boost::bind(&ResManImpl::CreateContainerCallback, this,
                                   agent_endpoint, _1, _2, _3, _4);
            LOG(INFO) << "send create command, container: "
                      << cmd.container_id << ", agent:"
                      << agent_endpoint;
            VLOG(10) << "TRACE BEGIN create container: " << cmd.container_id;
            VLOG(10) <<  request->DebugString();
            VLOG(10) << "TRACE END";
            rpc_client_.AsyncRequest(stub, &proto::Agent_Stub::CreateContainer,
                                     request, response, callback, 5, 1);
        } else if (cmd.action == sched::kDestroyContainer) {
            proto::RemoveContainerRequest* request = new proto::RemoveContainerRequest();
            proto::RemoveContainerResponse* response = new proto::RemoveContainerResponse();
            request->set_id(cmd.container_id);
            request->set_container_group_id(cmd.container_group_id);
            boost::function<void (const proto::RemoveContainerRequest*,
                                  proto::RemoveContainerResponse*,
                                  bool, int)> callback;
            callback = boost::bind(&ResManImpl::RemoveContainerCallback, this,
                                   agent_endpoint, _1, _2, _3, _4);
            VLOG(10) << "TRACE BEGIN remove container: " << cmd.container_id;
            VLOG(10) <<  request->DebugString();
            VLOG(10) << "TRACE END";
            rpc_client_.AsyncRequest(stub, &proto::Agent_Stub::RemoveContainer,
                                     request, response, callback, 5, 1);
            LOG(INFO) << "send remove command, container: "
                      << cmd.container_id << ", agent:"
                      << agent_endpoint;
        }
    }
}

void ResManImpl::KeepAlive(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::proto::KeepAliveRequest* request,
                           ::baidu::galaxy::proto::KeepAliveResponse* response,
                           ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    const std::string agent_ep = request->endpoint();
    bool agent_is_registered = false;
    bool agent_first_heartbeat = false;

    if (agents_.find(agent_ep) != agents_.end()) {
        agent_is_registered = true;
    }
    if (agent_stats_.find(agent_ep) == agent_stats_.end()) {
        agent_first_heartbeat = true;
    }
    if (!agent_is_registered) {
        LOG(WARNING) << "this agent is not registered, please check: " << agent_ep;
        done->Run();
        return;
    }
    if (agent_first_heartbeat) {
        LOG(INFO) << "first heartbeat of: " << agent_ep;
    }
    AgentStat& agent = agent_stats_[agent_ep];
    if (agent.status != proto::kAgentOffline) {
        agent.status = proto::kAgentAlive;
    }
    agent.last_heartbeat_time = common::timer::now_time();
    VLOG(10) << "heartbeat of: " << agent_ep << ", last: " << agent.last_heartbeat_time;
    if (agent_first_heartbeat) {
        query_pool_.AddTask(
            boost::bind(&ResManImpl::QueryAgent, this, agent_ep, true)
        );
    }
    done->Run();
}

void ResManImpl::CreateContainerGroup(::google::protobuf::RpcController* controller,
                                      const ::baidu::galaxy::proto::CreateContainerGroupRequest* request,
                                      ::baidu::galaxy::proto::CreateContainerGroupResponse* response,
                                      ::google::protobuf::Closure* done) {
    CHECK_USER()
    std::string user = request->user().user();
    LOG(INFO) << "user:" << user << " create container group";
    LOG(INFO) << request->DebugString();
    std::string invalid_pool;
    if (!CheckUserAuth(request->desc(), user, users_can_create_, invalid_pool)) {
        response->mutable_error_code()->set_status(proto::kCreateContainerGroupFail);
        response->mutable_error_code()->set_reason("no create permission on pool: " + invalid_pool);
        done->Run();
        return;
    }
    proto::ContainerGroupMeta container_group_meta;
    container_group_meta.set_name(request->name());
    container_group_meta.set_user_name(request->user().user());
    container_group_meta.set_replica(request->replica());
    container_group_meta.set_status(proto::kContainerGroupNormal);
    container_group_meta.set_submit_time(common::timer::get_micros());
    container_group_meta.mutable_desc()->CopyFrom(request->desc());
    std::string fail_reason;
    if (!HasQuotaToCreate(user, container_group_meta, fail_reason)) {
        response->mutable_error_code()->set_status(proto::kCreateContainerGroupFail);
        response->mutable_error_code()->set_reason("no quota to create:" + fail_reason);
        done->Run();
        return;
    }
    std::string container_group_id = scheduler_->Submit(request->name(), 
                                                        request->desc(), 
                                                        request->replica(), 
                                                        request->desc().priority(),
                                                        request->user().user());
    if (container_group_id == "") {
        proto::ErrorCode* err = response->mutable_error_code();
        err->set_status(proto::kCreateContainerGroupFail);
        err->set_reason("container group id conflict");
        done->Run();
        return;
    }
    container_group_meta.set_id(container_group_id);
    LOG(INFO) << container_group_meta.DebugString();
    bool ret = SaveObject(sContainerGroupPrefix + "/" + container_group_id, 
                          container_group_meta);
    if (!ret) {
        proto::ErrorCode* err = response->mutable_error_code();
        err->set_status(proto::kCreateContainerGroupFail);
        err->set_reason("fail to save container group meta in nexus");
        scheduler_->Kill(container_group_id);
    } else {
        response->mutable_error_code()->set_status(proto::kOk);
        response->set_id(container_group_id);
        {
            MutexLock lock(&mu_);
            container_groups_[container_group_id] = container_group_meta;
        }
    }
    done->Run();
}

void ResManImpl::RemoveContainerGroup(::google::protobuf::RpcController* controller,
                                      const ::baidu::galaxy::proto::RemoveContainerGroupRequest* request,
                                      ::baidu::galaxy::proto::RemoveContainerGroupResponse* response,
                                      ::google::protobuf::Closure* done) {
    CHECK_USER()
    proto::ContainerDescription desc;
    {
        MutexLock lock(&mu_);
        std::map<std::string, proto::ContainerGroupMeta>::iterator it;
        it = container_groups_.find(request->id());
        if (it == container_groups_.end()) {
            response->mutable_error_code()->set_status(proto::kJobNotFound);
            response->mutable_error_code()->set_reason("no such group");
            done->Run();
            return;
        } else {
            desc =  it->second.desc();
        }
    }
    std::string user = request->user().user();
    LOG(INFO) << "user: " << user << " remove container group: " << request->id();
    std::string invalid_pool;
    if (!CheckUserAuth(desc, user, users_can_remove_, invalid_pool)) {
        response->mutable_error_code()->set_status(proto::kRemoveContainerGroupFail);
        response->mutable_error_code()->set_reason("no remove permission on pool: " + invalid_pool);
        done->Run();
        return;
    }
    std::string top_container_group_id;
    if (desc.container_type() == proto::kVolumContainer
        && scheduler_->IsBeingShared(request->id(), top_container_group_id)) {
        response->mutable_error_code()->set_status(proto::kRemoveContainerGroupFail);
        response->mutable_error_code()->set_reason("shared by: " + top_container_group_id);
        done->Run();
        return;
    }
    bool ret = RemoveObject(sContainerGroupPrefix + "/" + request->id());
    if (!ret) {
        proto::ErrorCode* err = response->mutable_error_code();
        err->set_status(proto::kRemoveContainerGroupFail);
        err->set_reason("fail to remove container group meta from nexus");
    } else {
        {
            MutexLock lock(&mu_);
            container_groups_.erase(request->id());
        }
        scheduler_->Kill(request->id());
        response->mutable_error_code()->set_status(proto::kOk);
    }
    done->Run();
}

void ResManImpl::UpdateContainerGroup(::google::protobuf::RpcController* controller,
                                      const ::baidu::galaxy::proto::UpdateContainerGroupRequest* request,
                                      ::baidu::galaxy::proto::UpdateContainerGroupResponse* response,
                                      ::google::protobuf::Closure* done) {
    CHECK_USER()
    LOG(INFO) << "user:" << request->user().user()
              << " update container group: " << request->id();
    LOG(INFO) << request->DebugString();
    proto::ContainerGroupMeta new_meta;
    proto::ContainerGroupMeta old_meta;
    bool replica_changed = false;
    {
        MutexLock lock(&mu_);
        std::map<std::string, proto::ContainerGroupMeta>::iterator it;
        it = container_groups_.find(request->id());
        if (it == container_groups_.end()) {
            response->mutable_error_code()->set_status(proto::kUpdateContainerGroupFail);
            response->mutable_error_code()->set_reason("no such group");
            done->Run();
            return;
        }
        if (request->replica() > (size_t)FLAGS_container_group_max_replica) {
            response->mutable_error_code()->set_status(proto::kUpdateContainerGroupFail);
            response->mutable_error_code()->set_reason("invalid replica");
            done->Run();
            return;
        }
        old_meta = it->second;
    }
    std::string invalid_pool;
    if (!CheckUserAuth(old_meta.desc(), request->user().user(), users_can_update_, invalid_pool)) {
        response->mutable_error_code()->set_status(proto::kUpdateContainerGroupFail);
        response->mutable_error_code()->set_reason("no update permission on pool: " + invalid_pool);
        done->Run();
        return;
    }
    if (old_meta.user_name() != request->user().user()) {
        response->mutable_error_code()->set_status(proto::kUpdateContainerGroupFail);
        response->mutable_error_code()->set_reason(
            "user mismatch:" + old_meta.user_name() + " , " + request->user().user());
        done->Run();
        return;
    }
    std::string old_version = old_meta.desc().version();
    new_meta = old_meta; //copy
    new_meta.set_update_interval(request->interval());
    new_meta.mutable_desc()->CopyFrom(request->desc());
    new_meta.set_update_time(common::timer::get_micros());
    if (new_meta.replica() != request->replica()) {
        new_meta.set_replica(request->replica());
        replica_changed = true;
    }
    if (!CheckUserAuth(new_meta.desc(), request->user().user(), users_can_create_, invalid_pool)) {
        response->mutable_error_code()->set_status(proto::kUpdateContainerGroupFail);
        response->mutable_error_code()->set_reason("no create permission on pool: " + invalid_pool);
        done->Run();
        return;
    }
    std::string fail_reason;
    if (!HasQuotaToUpdate(request->user().user(), old_meta, new_meta, fail_reason)) {
        response->mutable_error_code()->set_status(proto::kUpdateContainerGroupFail);
        response->mutable_error_code()->set_reason("no quota to update:" + fail_reason);
        done->Run();
        return;
    }
    std::string new_version;
    bool version_changed = scheduler_->Update(new_meta.id(),
                                              new_meta.desc(),
                                              new_meta.update_interval(),
                                              new_version);
    if (version_changed) {
        LOG(INFO) << "container version changed: " << new_version;
        new_meta.mutable_desc()->set_version(new_version);
        if (old_meta.desc().container_type() == proto::kVolumContainer) {
            response->mutable_error_code()->set_status(proto::kUpdateContainerGroupFail);
            response->mutable_error_code()->set_reason("volum job cant not be updated");
            done->Run();
            return;
        }
    } else {
        if (new_meta.replica() < old_meta.replica() && old_meta.desc().container_type() == proto::kVolumContainer) {
            response->mutable_error_code()->set_status(proto::kUpdateContainerGroupFail);
            response->mutable_error_code()->set_reason("volum job cant not be scale down");
            done->Run();
            return;
        }
        new_meta.mutable_desc()->set_version(old_version);
    }
    scheduler_->ChangeReplica(new_meta.id(),
                              new_meta.replica());
    {
        MutexLock lock(&mu_);
        container_groups_[new_meta.id()] = new_meta;
    }
    bool save_ok = SaveObject(sContainerGroupPrefix + "/" + new_meta.id(), 
                              new_meta);
    if (!save_ok) {
        proto::ErrorCode* err = response->mutable_error_code();
        err->set_status(proto::kUpdateContainerGroupFail);
        err->set_reason("fail to save container group meta in nexus");
    } else {
        response->mutable_error_code()->set_status(proto::kOk);
        if (version_changed) {
            response->set_resource_change(true);
        }
    }
    done->Run();
}

void ResManImpl::ListContainerGroups(::google::protobuf::RpcController* controller,
                                     const ::baidu::galaxy::proto::ListContainerGroupsRequest* request,
                                     ::baidu::galaxy::proto::ListContainerGroupsResponse* response,
                                     ::google::protobuf::Closure* done) {
    std::vector<proto::ContainerGroupStatistics> container_groups;
    scheduler_->ListContainerGroups(container_groups);
    for (size_t i = 0; i < container_groups.size(); i++) {
        response->add_containers()->CopyFrom(container_groups[i]);
    }
    response->mutable_error_code()->set_status(proto::kOk);
    VLOG(16) << "list containers:" << response->DebugString();
    done->Run();
}

void ResManImpl::ShowContainerGroup(::google::protobuf::RpcController* controller,
                                    const ::baidu::galaxy::proto::ShowContainerGroupRequest* request,
                                    ::baidu::galaxy::proto::ShowContainerGroupResponse* response,
                                    ::google::protobuf::Closure* done) {
    {
        MutexLock lock(&mu_);
        std::map<std::string, proto::ContainerGroupMeta>::iterator it;
        it = container_groups_.find(request->id());
        if (it != container_groups_.end()) {
            response->mutable_desc()->CopyFrom(it->second.desc());
        }
    }
    std::vector<proto::ContainerStatistics> containers;
    bool ret = scheduler_->ShowContainerGroup(request->id(), containers);
    if (!ret) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("no such group");
        done->Run();
        return;
    }
    for (size_t i = 0; i < containers.size(); i++) {
        response->add_containers()->CopyFrom(containers[i]);
    }
    response->mutable_error_code()->set_status(proto::kOk);
    VLOG(16) << "show containers:" << response->DebugString();
    done->Run();   
}

void ResManImpl::AddAgent(::google::protobuf::RpcController* controller,
                          const ::baidu::galaxy::proto::AddAgentRequest* request,
                          ::baidu::galaxy::proto::AddAgentResponse* response,
                          ::google::protobuf::Closure* done) {
    if (request->pool().empty()) {
        proto::ErrorCode* err = response->mutable_error_code();
        err->set_status(proto::kAddAgentFail);
        err->set_reason("agent must belongs to a Pool");
        done->Run();
        return;
    }
    {
        MutexLock lock(&mu_);
        std::map<std::string, proto::AgentMeta>::const_iterator it;
        it = agents_.find(request->endpoint());
        if (it != agents_.end()) {
            response->mutable_error_code()->set_status(proto::kAddAgentFail);
            response->mutable_error_code()->set_reason("agent already exist");
            done->Run();
            return;
        }
    }
    proto::AgentMeta agent_meta;
    agent_meta.set_endpoint(request->endpoint());
    agent_meta.set_pool(request->pool());
    bool ret = SaveObject(sAgentPrefix + "/" + agent_meta.endpoint(), agent_meta);
    if (!ret) {
        proto::ErrorCode* err = response->mutable_error_code();
        err->set_status(proto::kAddAgentFail);
        err->set_reason("fail to save agent in nexus");
    } else {
        {
            MutexLock lock(&mu_);
            agents_[agent_meta.endpoint()] = agent_meta;
            pools_[agent_meta.pool()].insert(agent_meta.endpoint());
        }
        response->mutable_error_code()->set_status(proto::kOk);
    }
    done->Run();
}

void ResManImpl::RemoveAgent(::google::protobuf::RpcController* controller,
                             const ::baidu::galaxy::proto::RemoveAgentRequest* request,
                             ::baidu::galaxy::proto::RemoveAgentResponse* response,
                             ::google::protobuf::Closure* done) {
    LOG(INFO) << "remove agent:" << request->endpoint();
    std::set<std::string> agent_tags;
    std::string agent_pool;
    const std::string& endpoint = request->endpoint();
    {
        MutexLock lock(&mu_);
        std::map<std::string, proto::AgentMeta>::const_iterator it;
        it = agents_.find(endpoint);
        if (it == agents_.end()) {
            response->mutable_error_code()->set_status(proto::kRemoveAgentFail);
            response->mutable_error_code()->set_reason("agent not exist");
            done->Run();
            return;
        } else {
            agent_pool = it->second.pool();
            if (agent_tags_.find(endpoint) != agent_tags_.end()) {
                agent_tags = agent_tags_[endpoint];
            }
        }
    }
    bool remove_ok = RemoveObject(sAgentPrefix + "/" + request->endpoint());
    if (!remove_ok) {
        response->mutable_error_code()->set_status(proto::kRemoveAgentFail);
        response->mutable_error_code()->set_reason("fail to delete meta from nexus");
    } else {
        MutexLock lock(&mu_);
        agents_.erase(endpoint);
        agent_stats_.erase(endpoint);
        pools_[agent_pool].erase(endpoint);
        std::set<std::string>::const_iterator tag_it;
        for (tag_it = agent_tags.begin(); tag_it != agent_tags.end(); tag_it++) {
            const std::string& tag_name = *tag_it;
            tags_[tag_name].erase(endpoint);
        }
        scheduler_->RemoveAgent(endpoint);
        response->mutable_error_code()->set_status(proto::kOk);
    }
    done->Run();
}

void ResManImpl::OnlineAgent(::google::protobuf::RpcController* controller,
                             const ::baidu::galaxy::proto::OnlineAgentRequest* request,
                             ::baidu::galaxy::proto::OnlineAgentResponse* response,
                             ::google::protobuf::Closure* done) {

}

void ResManImpl::OfflineAgent(::google::protobuf::RpcController* controller,
                              const ::baidu::galaxy::proto::OfflineAgentRequest* request,
                              ::baidu::galaxy::proto::OfflineAgentResponse* response,
                              ::google::protobuf::Closure* done) {

}

void ResManImpl::ListAgents(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::proto::ListAgentsRequest* request,
                            ::baidu::galaxy::proto::ListAgentsResponse* response,
                            ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    std::map<std::string, proto::AgentMeta>::iterator it;
    for (it = agents_.begin(); it != agents_.end(); it++) {
        const std::string& endpoint = it->first;
        const proto::AgentMeta& agent_meta = it->second;
        proto::AgentStatistics* agent_st = response->add_agents();
        agent_st->set_endpoint(endpoint);
        agent_st->set_pool(agent_meta.pool());
        const std::set<std::string>& tags = agent_tags_[endpoint];
        for (std::set<std::string>::iterator tag_it = tags.begin();
             tag_it != tags.end(); tag_it++) {
            agent_st->add_tags(*tag_it);
        }
        std::map<std::string, AgentStat>::iterator jt = agent_stats_.find(endpoint);
        if (jt == agent_stats_.end()) {
            agent_st->set_status(proto::kAgentUnknown);
            continue;
        }
        agent_st->set_status(jt->second.status);
        agent_st->mutable_cpu()->CopyFrom(jt->second.info.cpu_resource());
        agent_st->mutable_memory()->CopyFrom(jt->second.info.memory_resource());
        agent_st->mutable_volums()->CopyFrom(jt->second.info.volum_resources());
        agent_st->set_total_containers(jt->second.info.container_info().size());
    }
    VLOG(10) << "list agents:" << response->DebugString();
    response->mutable_error_code()->set_status(proto::kOk);
    done->Run();
}

void ResManImpl::ShowAgent(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::proto::ShowAgentRequest* request,
                           ::baidu::galaxy::proto::ShowAgentResponse* response,
                           ::google::protobuf::Closure* done) {
    const std::string& endpoint = request->endpoint();
    std::vector<proto::ContainerStatistics> containers;
    bool ret = scheduler_->ShowAgent(endpoint, containers);
    if (!ret) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("no such agent");
        done->Run();
        return;
    }
    for (size_t i = 0; i < containers.size(); i++) {
        response->add_containers()->CopyFrom(containers[i]);
    }
    response->mutable_error_code()->set_status(proto::kOk);
    done->Run();
}

void ResManImpl::CreateTag(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::proto::CreateTagRequest* request,
                           ::baidu::galaxy::proto::CreateTagResponse* response,
                           ::google::protobuf::Closure* done) {
    LOG(INFO) << "create tag:" << request->tag();
    const std::string& tag = request->tag();
    proto::TagMeta tag_meta;
    tag_meta.set_tag(tag);
    std::set<std::string> tag_new;
    for (int i = 0; i < request->endpoint_size(); i++) {
        const std::string& endpoint = request->endpoint(i);
        tag_new.insert(endpoint);
        tag_meta.add_endpoints(endpoint);
    }
    bool ret = SaveObject(sTagPrefix + "/" + tag, tag_meta);
    if (!ret) {
        proto::ErrorCode* err = response->mutable_error_code();
        err->set_status(proto::kCreateTagFail);
        err->set_reason("fail to save tag to nexus");
    } else {
        MutexLock lock(&mu_);
        std::set<std::string>& tag_old = tags_[tag];
        std::set<std::string>::iterator it_old = tag_old.begin();
        std::set<std::string>::iterator it_new = tag_new.begin();
        while (it_old != tag_old.end() && it_new != tag_new.end()) {
            if (*it_old < *it_new) {
                agent_tags_[*it_old].erase(tag);
                scheduler_->RemoveTag(*it_old, tag);
                it_old++;
            } else if (*it_old == *it_new) {
                it_old++;
                it_new++;
            } else {
                agent_tags_[*it_new].insert(tag);
                scheduler_->AddTag(*it_new, tag);
                it_new++;
            }
        }
        while (it_old != tag_old.end()) {
            agent_tags_[*it_old].erase(tag);
            scheduler_->RemoveTag(*it_old, tag);
            it_old++;
        }
        while (it_new != tag_new.end()) {
            agent_tags_[*it_new].insert(tag);
            scheduler_->AddTag(*it_new, tag);
            it_new++;
        }
        tag_old = tag_new;
    }
    response->mutable_error_code()->set_status(proto::kOk);
    done->Run();
}

void ResManImpl::ListTags(::google::protobuf::RpcController* controller,
                          const ::baidu::galaxy::proto::ListTagsRequest* request,
                          ::baidu::galaxy::proto::ListTagsResponse* response,
                          ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    std::map<std::string, std::set<std::string> >::iterator it;
    for (it = tags_.begin(); it != tags_.end(); it++) {
        response->add_tags(it->first);
    }
    response->mutable_error_code()->set_status(proto::kOk);
    done->Run();
}

void ResManImpl::ListAgentsByTag(::google::protobuf::RpcController* controller,
                                 const ::baidu::galaxy::proto::ListAgentsByTagRequest* request,
                                 ::baidu::galaxy::proto::ListAgentsByTagResponse* response,
                                 ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    std::map<std::string, std::set<std::string> >::iterator it;
    it = tags_.find(request->tag());
    if (it == tags_.end()) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("fail to list agents, no such tag");
        done->Run();
        return;
    }
    std::set<std::string>::const_iterator jt;
    for (jt = it->second.begin(); jt != it->second.end(); jt++) {
        const std::string& endpoint = *jt;
        const proto::AgentMeta& agent_meta = agents_[endpoint];
        proto::AgentStatistics* agent_st = response->add_agents();
        agent_st->set_endpoint(endpoint);
        agent_st->set_pool(agent_meta.pool());
        const std::set<std::string>& tags = agent_tags_[endpoint];
        for (std::set<std::string>::iterator tag_it = tags.begin();
            tag_it != tags.end(); tag_it++) {
            agent_st->add_tags(*tag_it);
        }
        std::map<std::string, AgentStat>::iterator stat_it = agent_stats_.find(endpoint);
        if (stat_it == agent_stats_.end()) {
            continue;
        }
        agent_st->set_status(stat_it->second.status);
        agent_st->mutable_cpu()->CopyFrom(stat_it->second.info.cpu_resource());
        agent_st->mutable_memory()->CopyFrom(stat_it->second.info.memory_resource());
        agent_st->mutable_volums()->CopyFrom(stat_it->second.info.volum_resources());
        agent_st->set_total_containers(stat_it->second.info.container_info().size());
    }
    response->mutable_error_code()->set_status(proto::kOk);
    done->Run();
}

void ResManImpl::GetTagsByAgent(::google::protobuf::RpcController* controller,
                                const ::baidu::galaxy::proto::GetTagsByAgentRequest* request,
                                ::baidu::galaxy::proto::GetTagsByAgentResponse* response,
                                ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    std::map<std::string, std::set<std::string> >::iterator it;
    it = agent_tags_.find(request->endpoint());
    if (it == agent_tags_.end()) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("no such agent");
        done->Run();
        return;
    }
    std::set<std::string>::iterator jt;
    for (jt = it->second.begin(); jt != it->second.end(); jt++) {
        response->add_tags(*jt);
    }
    response->mutable_error_code()->set_status(proto::kOk);
    done->Run();
}

void ResManImpl::AddAgentToPool(::google::protobuf::RpcController* controller,
                                const ::baidu::galaxy::proto::AddAgentToPoolRequest* request,
                                ::baidu::galaxy::proto::AddAgentToPoolResponse* response,
                                ::google::protobuf::Closure* done) {
    {
        MutexLock lock(&mu_);
        if (agents_.find(request->endpoint()) == agents_.end()) {
            response->mutable_error_code()->set_status(proto::kAddAgentToPoolFail);
            response->mutable_error_code()->set_reason("agent not exist");
            done->Run();
            return;
        }
    }
    if (request->pool().empty()) {
        proto::ErrorCode* err = response->mutable_error_code();
        err->set_status(proto::kAddAgentToPoolFail);
        err->set_reason("the pool can not be empty");
        done->Run();
        return;
    }
    const std::string& endpoint = request->endpoint();
    const std::string& pool = request->pool();
    proto::AgentMeta agent_meta = agents_[endpoint];
    std::string old_pool = agent_meta.pool();
    agent_meta.set_pool(pool);
    bool ret = SaveObject(sAgentPrefix + "/" + endpoint, agent_meta);
    if (!ret) {
        response->mutable_error_code()->set_status(proto::kAddAgentToPoolFail);
        response->mutable_error_code()->set_reason("fail to save agent meta to nexus");
    } else {
        MutexLock lock(&mu_);
        agents_[endpoint].set_pool(pool);
        if (pools_.find(old_pool) != pools_.end()) {
            pools_[old_pool].erase(endpoint);
        }
        pools_[pool].insert(endpoint);
        scheduler_->SetPool(endpoint, pool);
        response->mutable_error_code()->set_status(proto::kOk);
    }
    done->Run();
}

void ResManImpl::RemoveAgentFromPool(::google::protobuf::RpcController* controller,
                                     const ::baidu::galaxy::proto::RemoveAgentFromPoolRequest* request,
                                     ::baidu::galaxy::proto::RemoveAgentFromPoolResponse* response,
                                     ::google::protobuf::Closure* done) {

}

void ResManImpl::ListAgentsByPool(::google::protobuf::RpcController* controller,
                                  const ::baidu::galaxy::proto::ListAgentsByPoolRequest* request,
                                  ::baidu::galaxy::proto::ListAgentsByPoolResponse* response,
                                  ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    std::map<std::string, std::set<std::string> >::iterator it;
    it = pools_.find(request->pool());
    if (it == pools_.end()) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("fail to list agents, no such pool:" + request->pool());
        done->Run();
        return;
    }
    std::set<std::string>::const_iterator jt;
    for (jt = it->second.begin(); jt != it->second.end(); jt++) {
        const std::string& endpoint = *jt;
        const proto::AgentMeta& agent_meta = agents_[endpoint];
        proto::AgentStatistics* agent_st = response->add_agents();
        agent_st->set_endpoint(endpoint);
        agent_st->set_pool(agent_meta.pool());
        const std::set<std::string>& tags = agent_tags_[endpoint];
        for (std::set<std::string>::iterator tag_it = tags.begin();
            tag_it != tags.end(); tag_it++) {
            agent_st->add_tags(*tag_it);
        }
        std::map<std::string, AgentStat>::iterator stat_it = agent_stats_.find(endpoint);
        if (stat_it == agent_stats_.end()) {
            continue;
        }
        agent_st->set_status(stat_it->second.status);
        agent_st->mutable_cpu()->CopyFrom(stat_it->second.info.cpu_resource());
        agent_st->mutable_memory()->CopyFrom(stat_it->second.info.memory_resource());
        agent_st->mutable_volums()->CopyFrom(stat_it->second.info.volum_resources());
        agent_st->set_total_containers(stat_it->second.info.container_info().size());
    }
    response->mutable_error_code()->set_status(proto::kOk);
    done->Run();
}

void ResManImpl::GetPoolByAgent(::google::protobuf::RpcController* controller,
                                const ::baidu::galaxy::proto::GetPoolByAgentRequest* request,
                                ::baidu::galaxy::proto::GetPoolByAgentResponse* response,
                                ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (agents_.find(request->endpoint()) == agents_.end()) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("agent not exist");
        done->Run();
        return;
    }
    response->set_pool(agents_[request->endpoint()].pool());
    response->mutable_error_code()->set_status(proto::kOk);
    done->Run();
}

void ResManImpl::AddUser(::google::protobuf::RpcController* controller,
                        const ::baidu::galaxy::proto::AddUserRequest* request,
                        ::baidu::galaxy::proto::AddUserResponse* response,
                        ::google::protobuf::Closure* done) {
    const std::string& user_name = request->user().user();
    {
        MutexLock lock(&mu_);
        if (users_.find(user_name) != users_.end()) {
            response->mutable_error_code()->set_status(proto::kAddUserFail);
            response->mutable_error_code()->set_reason("user already exist");
            done->Run();
            return;
        }
    }
    proto::UserMeta user_meta;
    user_meta.mutable_user()->CopyFrom(request->user());
    LOG(INFO) << "add user: " << user_meta.DebugString();
    bool ret = SaveObject(sUserPrefix + "/" + user_name, user_meta);
    if (!ret) {
        response->mutable_error_code()->set_status(proto::kAddUserFail);
        response->mutable_error_code()->set_reason("fail to save user meta in nexus");
    } else {
        MutexLock lock(&mu_);
        users_[user_name] = user_meta;
        response->mutable_error_code()->set_status(proto::kOk);
    }
    done->Run();
}

void ResManImpl::RemoveUser(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::proto::RemoveUserRequest* request,
                            ::baidu::galaxy::proto::RemoveUserResponse* response,
                            ::google::protobuf::Closure* done) {
    const std::string& user_name = request->user().user();
    LOG(INFO) << "remove user: " << user_name;
    {
        MutexLock lock(&mu_);
        if (users_.find(user_name) == users_.end()) {
            response->mutable_error_code()->set_status(proto::kRemoveUserFail);
            response->mutable_error_code()->set_reason("user not exist");
            done->Run();
            return;
        }
    }
    bool ret = RemoveObject(sUserPrefix + "/" + user_name);
    if (!ret) {
        response->mutable_error_code()->set_status(proto::kRemoveUserFail);
        response->mutable_error_code()->set_reason("fail to delete user meta from nexus");
    } else {
        MutexLock lock(&mu_);
        users_.erase(user_name);
        response->mutable_error_code()->set_status(proto::kOk);
    }
    done->Run();
}

void ResManImpl::ListUsers(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::proto::ListUsersRequest* request,
                           ::baidu::galaxy::proto::ListUsersResponse* response,
                           ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    std::map<std::string, proto::UserMeta>::const_iterator it;
    for (it = users_.begin(); it != users_.end(); it++) {
        response->add_user(it->first);
    }
    done->Run();
}

void ResManImpl::ShowUser(::google::protobuf::RpcController* controller,
                          const ::baidu::galaxy::proto::ShowUserRequest* request,
                          ::baidu::galaxy::proto::ShowUserResponse* response,
                          ::google::protobuf::Closure* done) {
    const std::string& user_name = request->user().user();
    MutexLock lock(&mu_);
    if (users_.find(user_name) == users_.end()) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("no such user");
        done->Run();
        return;
    }
    proto::Quota alloc;
    scheduler_->ShowUserAlloc(user_name, alloc);
    response->mutable_assigned()->CopyFrom(alloc);
    response->mutable_quota()->CopyFrom(users_[user_name].quota());
    response->mutable_error_code()->set_status(proto::kOk);
    const proto::UserMeta& user_meta = users_[user_name];
    response->mutable_grants()->CopyFrom(user_meta.grants());
    VLOG(10) << "show user: " << response->DebugString();
    done->Run();
}

void ResManImpl::GrantUser(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::proto::GrantUserRequest* request,
                           ::baidu::galaxy::proto::GrantUserResponse* response,
                           ::google::protobuf::Closure* done) {
    const std::string& user_name = request->user().user();
    proto::UserMeta user_meta;
    {
        MutexLock lock(&mu_);
        if (users_.find(user_name) == users_.end()) {
            response->mutable_error_code()->set_status(proto::kGrantUserFail);
            response->mutable_error_code()->set_reason("no such user");
            done->Run();
            return;
        }
        user_meta = users_[user_name];
    }
    const proto::Grant& grant = request->grant();
    /// pool_name -> [auth1, auth2, auth3]
    std::map<std::string, std::set<proto::Authority> > user_grants;
    for (int i = 0; i < user_meta.grants_size(); i++) {
        const std::string& pool_name = user_meta.grants(i).pool();
        for (int j =0; j < user_meta.grants(i).authority_size(); j++) {
            user_grants[pool_name].insert(user_meta.grants(i).authority(j));
        }
    }
    if (grant.action() == proto::kActionSet) {
        user_grants[grant.pool()].clear();
        for (int i = 0; i < grant.authority_size(); i++) {
            user_grants[grant.pool()].insert(grant.authority(i));
        }
    } else if (grant.action() == proto::kActionClear) {
        user_grants[grant.pool()].clear();
    } else if (grant.action() == proto::kActionAdd) {
        for (int i = 0; i < grant.authority_size(); i++) {
            user_grants[grant.pool()].insert(grant.authority(i));
        }
    } else if (grant.action() == proto::kActionRemove) {
        for (int i = 0; i < grant.authority_size(); i++) {
            user_grants[grant.pool()].erase(grant.authority(i));
        }
    }
    user_meta.mutable_grants()->Clear();
    std::map<std::string, std::set<proto::Authority> >::const_iterator it;
    for (it = user_grants.begin(); it != user_grants.end(); it++) {
        proto::Grant* the_grant = user_meta.add_grants();
        const std::string& pool_name = it->first;
        const std::set<proto::Authority>& auths = it->second;
        the_grant->set_pool(pool_name);
        std::set<proto::Authority>::const_iterator jt;
        for (jt = auths.begin(); jt != auths.end(); jt++) {
            the_grant->add_authority(*jt);
        }
    }

    bool save_ret = SaveObject(sUserPrefix + "/" + user_name, user_meta);
    if (!save_ret) {
        response->mutable_error_code()->set_status(proto::kGrantUserFail);
        response->mutable_error_code()->set_reason("fail to save meta to nexus");
    } else {
        MutexLock lock(&mu_);
        users_[user_name] = user_meta;
        for (int i = 0; i < user_meta.grants_size(); i++) {
            const proto::Grant& grant = user_meta.grants(i);
            const std::string& pool_name = grant.pool();
            bool can_create = false;
            bool can_update = false;
            bool can_remove = false;
            bool can_list = false;
            for (int j = 0; j < grant.authority_size(); j++) {
                const proto::Authority& auth = grant.authority(j);
                if (auth == proto::kAuthorityCreateContainer) {
                    can_create = true;
                } else if (auth == proto::kAuthorityUpdateContainer) {
                    can_update = true;
                } else if (auth == proto::kAuthorityRemoveContainer) {
                    can_remove = true;
                } else if (auth == proto::kAuthorityListContainer) {
                    can_list = true;
                }
            }
            if (can_create) {
                users_can_create_[pool_name].insert(user_name);
            } else {
                users_can_create_[pool_name].erase(user_name);
            }
            if (can_update) {
                users_can_update_[pool_name].insert(user_name);
            } else {
                users_can_update_[pool_name].erase(user_name);
            }
            if (can_remove) {
                users_can_remove_[pool_name].insert(user_name);
            } else {
                users_can_remove_[pool_name].erase(user_name);
            }
            if (can_list) {
                users_can_list_[pool_name].insert(user_name);
            } else {
                users_can_list_[pool_name].erase(user_name);
            }
        }
        response->mutable_error_code()->set_status(proto::kOk);
        VLOG(10) <<  "grant user:" << user_meta.DebugString();
    }
    done->Run();
}

void ResManImpl::AssignQuota(::google::protobuf::RpcController* controller,
                             const ::baidu::galaxy::proto::AssignQuotaRequest* request,
                             ::baidu::galaxy::proto::AssignQuotaResponse* response,
                             ::google::protobuf::Closure* done) {
    const std::string& user_name = request->user().user();
    proto::UserMeta user_meta;
    {
        MutexLock lock(&mu_);
        if (users_.find(user_name) == users_.end()) {
            response->mutable_error_code()->set_status(proto::kAssignQuotaFail);
            response->mutable_error_code()->set_reason("no such user");
            done->Run();
            return;
        }
        user_meta = users_[user_name];
    }
    user_meta.mutable_quota()->CopyFrom(request->quota());
    bool save_ret = SaveObject(sUserPrefix + "/" + user_name, user_meta);
    if (!save_ret) {
        response->mutable_error_code()->set_status(proto::kAssignQuotaFail);
        response->mutable_error_code()->set_reason("fail to save meta to nexus");
    } else {
        MutexLock lock(&mu_);
        users_[user_name] = user_meta;
        response->mutable_error_code()->set_status(proto::kOk);
    }
    done->Run();
}

bool ResManImpl::RemoveObject(const std::string& key) {
    ::galaxy::ins::sdk::SDKError err;
    std::string full_key = FLAGS_nexus_root + key;
    bool ret = nexus_->Delete(full_key, &err);
    if (!ret) {
        LOG(WARNING) << "nexus error:" << err;
    }
    return ret;
}

template <class ProtoClass> 
bool ResManImpl::SaveObject(const std::string& key,
                            const ProtoClass& obj) {
    std::string raw_buf;
    if (!obj.SerializeToString(&raw_buf)) {
        LOG(WARNING) << "save object to protobuf fail";
        return false;
    }
    ::galaxy::ins::sdk::SDKError err;
    std::string full_key = FLAGS_nexus_root + key;
    bool ret = nexus_->Put(full_key, raw_buf, &err);
    if (!ret) {
        LOG(WARNING) << "nexus error: " << err;
    }
    return ret;
}

template <class ProtoClass>
bool ResManImpl::LoadObjects(const std::string& prefix,
                             std::map<std::string, ProtoClass>& objs) {
    std::string full_prefix = FLAGS_nexus_root + prefix;
    ::galaxy::ins::sdk::ScanResult* result  
        = nexus_->Scan(full_prefix + "/", full_prefix + "/\xff");
    boost::scoped_ptr< ::galaxy::ins::sdk::ScanResult > result_guard(result);
    size_t prefix_len = full_prefix.size() + 1;
    while (!result->Done()) {
        const std::string& full_key = result->Key();
        const std::string& raw_obj_buf = result->Value();
        std::string key = full_key.substr(prefix_len);
        LOG(INFO) << "try load " << key << " from nexus";
        ProtoClass& obj = objs[key];
        bool parse_ok = obj.ParseFromString(raw_obj_buf);
        if (!parse_ok) {
            LOG(WARNING) << "parse protobuf object fail ";
            return false;
        }
        result->Next();
    }
    return result->Error() == ::galaxy::ins::sdk::kOK;
}

void ResManImpl::CreateContainerCallback(std::string agent_endpoint,
                                         const proto::CreateContainerRequest* request,
                                         proto::CreateContainerResponse* response,
                                         bool fail, int err) {
    boost::scoped_ptr<const proto::CreateContainerRequest> request_guard(request);
    boost::scoped_ptr<proto::CreateContainerResponse> response_guard(response);
    VLOG(10) << "create response:" << response->DebugString();
    if (fail) {
        LOG(WARNING) << "rpc fail of creating container, err: " << err
                     << ", agent: " << agent_endpoint; 
        return;
    }
    if (response->code().status() != proto::kOk) {
        LOG(WARNING) << "fail to create contaienr, reason:" 
                     << response->code().reason()
                     << ", agent:" << agent_endpoint
                     << ", contaienr_id: " << request->id();
        const std::string& container_group_id = request->container_group_id();
        const std::string& container_id = request->id();
        scheduler_->ChangeStatus(container_group_id, container_id, proto::kContainerPending);
        return;
    }
}

void ResManImpl::RemoveContainerCallback(std::string agent_endpoint,
                                         const proto::RemoveContainerRequest* request,
                                         proto::RemoveContainerResponse* response,
                                         bool fail, int err) {
    boost::scoped_ptr<const proto::RemoveContainerRequest> request_guard(request);
    boost::scoped_ptr<proto::RemoveContainerResponse> response_guard(response);
    if (fail) {
        LOG(WARNING) << "rpc fail of remve container, err: " << err
                     << ", agent:" << agent_endpoint
                     << ", container:" << request->id();
        return;
    }
    if (response->code().status() != proto::kOk) {
        LOG(WARNING) << "fail to remove contaienr, reason:" 
                     << response->code().reason()
                     << ", agent:" << agent_endpoint
                     << ", container:" << request->id();
        return;
    }
}

template <class RpcRequest, class RpcResponse, class DoneClosure>
bool ResManImpl::CheckUserExist(const RpcRequest* request, 
                                RpcResponse* response,
                                DoneClosure* done) {
    MutexLock lock(&mu_);
    const std::string& user_name = request->user().user();
    if (users_.find(user_name) == users_.end()) {
        LOG(WARNING) << "unexpected user:" << request->user().user();
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("user not exist:" + user_name);
        done->Run();
        return false;
    }
    const std::string& right_token = users_[user_name].user().token();
    if (request->user().token() != right_token) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason("wrong token!" + user_name);
        done->Run();
        return false;
    }
    return true;
}

bool ResManImpl::CheckUserAuth(const proto::ContainerDescription& desc,
                               const std::string& user_name,
                               const std::map<std::string, std::set<std::string> > & users_auth,
                               std::string& invalid_pool) {
    MutexLock lock(&mu_);
    for (int i = 0; i < desc.pool_names_size(); i++) {
        const std::string& pool_name = desc.pool_names(i);
        std::map<std::string, std::set<std::string> >::const_iterator u_it;
        u_it = users_auth.find(pool_name);
        if (u_it == users_auth.end()) {
            invalid_pool = pool_name;
            return false;
        }
        const std::set<std::string>& valid_users = u_it->second;
        if (valid_users.find(user_name) == valid_users.end()) {
            invalid_pool = pool_name;
            return false;
        }
    }
    return true;
}

void ResManImpl::Preempt(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::proto::PreemptRequest* request,
                         ::baidu::galaxy::proto::PreemptResponse* response,
                         ::google::protobuf::Closure* done) {
    const std::string& agent_endpoint = request->endpoint();
    const std::string& container_group_id = request->container_group_id();
    LOG(INFO) << "preempt scheduling: " << container_group_id
              << " on:" << agent_endpoint;
    std::string fail_reason;
    bool ret = scheduler_->ManualSchedule(agent_endpoint, 
                                          container_group_id,
                                          fail_reason);
    if (!ret) {
        response->mutable_error_code()->set_status(proto::kError);
        response->mutable_error_code()->set_reason(fail_reason);
    } else {
        response->mutable_error_code()->set_status(proto::kOk);
    }
    done->Run();
}

bool ResManImpl::HasQuotaToCreate(const std::string& user,
                                  const proto::ContainerGroupMeta& meta,
                                  std::string& fail_reason) {
    proto::Quota quota_used;
    proto::Quota quota_added;
    proto::Quota quota_total;
    {
        MutexLock lock(&mu_);
        std::map<std::string, proto::UserMeta>::const_iterator user_it;
        user_it = users_.find(user);
        if (user_it == users_.end()) {
            fail_reason = "no such user: " + user;
            LOG(WARNING) << "no such user: " << user;
            return false;
        }
        quota_total = user_it->second.quota();
    }
    scheduler_->ShowUserAlloc(user, quota_used);
    scheduler_->MetaToQuota(meta, quota_added);
    if (quota_used.millicore() + quota_added.millicore() > quota_total.millicore()) {
        fail_reason = "no cpu quota";
        return false;
    } else if (quota_used.memory() + quota_added.memory() > quota_total.memory()) {
        fail_reason = "no memory quota";
        return false;
    } else if (quota_used.disk() + quota_added.disk() > quota_total.disk()) {
        fail_reason = "no disk quota";
        return false;
    } else if (quota_used.ssd() + quota_added.ssd() > quota_total.ssd()) {
        fail_reason = "no ssd quota";
        return false;
    } else if (quota_used.replica() + quota_added.replica() > quota_total.replica()) {
        fail_reason = "no replica quota";
        return false;
    }
    return true;
}

bool ResManImpl::HasQuotaToUpdate(const std::string& user,
                                  const proto::ContainerGroupMeta& old_meta,
                                  const proto::ContainerGroupMeta& new_meta,
                                  std::string& fail_reason) {
    proto::Quota quota_used;
    proto::Quota quota_removed;
    proto::Quota quota_added;
    proto::Quota quota_total;
    {
        MutexLock lock(&mu_);
        std::map<std::string, proto::UserMeta>::const_iterator user_it;
        user_it = users_.find(user);
        if (user_it == users_.end()) {
            fail_reason = "no such user: " + user;
            LOG(WARNING) << "no such user: " << user;
            return false;
        }
        quota_total = user_it->second.quota();
    }
    scheduler_->ShowUserAlloc(user, quota_used);
    scheduler_->MetaToQuota(old_meta, quota_removed);
    scheduler_->MetaToQuota(new_meta, quota_added);

    if (quota_used.millicore() + quota_added.millicore()
        - quota_removed.millicore()> quota_total.millicore()) {
        fail_reason = "no cpu quota";
        return false;
    } else if (quota_used.memory() + quota_added.memory() 
               - quota_removed.memory() > quota_total.memory()) {
        fail_reason = "no memory quota";
        return false;
    } else if (quota_used.disk() + quota_added.disk() 
               - quota_removed.disk() > quota_total.disk()) {
        fail_reason = "no disk quota";
        return false;
    } else if (quota_used.ssd() + quota_added.ssd() 
               - quota_removed.ssd() > quota_total.ssd()) {
        fail_reason = "no ssd quota";
        return false;
    } else if (quota_used.replica() + quota_added.replica() 
               - quota_removed.replica() > quota_total.replica()) {
        fail_reason = "no replica quota";
        return false;
    }
    return true;
}

} //namespace galaxy
} //namespace baidu
