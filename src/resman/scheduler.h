// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#include <map>
#include <set>
#include <list>
#include <vector>
#include <string>
#include <utility>
#include <boost/shared_ptr.hpp>
#include "src/protocol/galaxy.pb.h"
#include "mutex.h"
#include "thread_pool.h"

namespace baidu {
namespace galaxy {
namespace sched {

using proto::ContainerStatus;
using proto::kContainerPending;
using proto::kContainerAllocating;
using proto::kContainerReady;
using proto::kContainerError;
using proto::kContainerDestroying;
using proto::kContainerTerminated;
using proto::kContainerFinish;
using proto::ResourceError;

typedef std::string AgentEndpoint;
typedef std::string ContainerGroupId;
typedef std::string ContainerId;
typedef std::string DevicePath;

enum AgentCommandAction {
    kCreateContainer = 0,
    kDestroyContainer = 1
};

struct AgentCommand {
    AgentCommandAction action;
    ContainerId container_id;
    ContainerGroupId container_group_id;
    proto::ContainerDescription desc;
};

struct Requirement {
    std::string tag;
    std::set<std::string> pool_names;
    int max_per_host;
    std::vector<proto::CpuRequired> cpu;
    std::vector<proto::MemoryRequired> memory;
    std::vector<proto::VolumRequired> volums;
    std::vector<proto::PortRequired> ports;
    std::string version;
    std::vector<proto::TcpthrotRequired> tcp_throts;
    std::vector<proto::BlkioRequired> blkios;
    std::vector<std::string> volum_jobs;
    proto::ContainerType container_type;
    Requirement() : max_per_host(0) , container_type(proto::kNormalContainer) {};
    int64_t CpuNeed() {
        int64_t total = 0;
        for (size_t i = 0; i < cpu.size(); i++) {
            total += cpu[i].milli_core();
        }
        return total;
    }
    int64_t MemoryNeed() {
        int64_t total = 0;
        for (size_t i = 0; i < memory.size(); i++) {
            total += memory[i].size();
        }
        return total;
    }
    int64_t DiskNeed() {
        int64_t total = 0;
        for (size_t i = 0; i < volums.size(); i++) {
            if (volums[i].medium() == proto::kDisk) {
                total += volums[i].size();
            }
        }
        return total;
    }
    int64_t SsdNeed() {
        int64_t total = 0;
        for (size_t i = 0; i < volums.size(); i++) {
            if (volums[i].medium() == proto::kSsd) {
                total += volums[i].size();
            }
        }
        return total;
    }
    int64_t TmpfsNeed() {
        int64_t total = 0;
        for (size_t i = 0; i < volums.size(); i++) {
            if (volums[i].medium() == proto::kTmpfs) {
                total += volums[i].size();
            }
        }
        return total;   
    }
    typedef boost::shared_ptr<Requirement> Ptr;
};

struct VolumInfo {
    proto::VolumMedium medium;
    bool exclusive;
    int64_t size;
    VolumInfo() : exclusive(false), size(0) {}
};

struct Container {
    ContainerId id;
    ContainerGroupId container_group_id;
    int priority;
    proto::ContainerStatus status;
    Requirement::Ptr require;
    std::vector<std::pair<DevicePath, VolumInfo> > allocated_volums;
    std::vector<std::string> allocated_ports;
    AgentEndpoint allocated_agent;
    ResourceError last_res_err;
    proto::ContainerInfo remote_info;
    std::vector<ContainerId> allocated_volum_containers;
    Container() : status(kContainerPending), last_res_err(proto::kResOk) {}
    typedef boost::shared_ptr<Container> Ptr;
};

struct ContainerPriorityLess{
    bool operator() (const Container::Ptr& a, const Container::Ptr& b) {
        return a->priority < b->priority;
    }
};

typedef std::map<ContainerId, Container::Ptr> ContainerMap;

struct ContainerGroup {
    ContainerGroupId id;
    Requirement::Ptr require;
    int priority; //lower one is important
    bool terminated;
    std::map<ContainerId, Container::Ptr> containers;
    std::map<ContainerId, Container::Ptr> states[8];
    int update_interval;
    int last_update_time;
    int replica;
    std::string name;
    std::string user_name;
    proto::ContainerDescription container_desc;
    int64_t submit_time;
    int64_t update_time;
    std::string last_sched_container_id;
    ContainerGroup() : terminated(false),
                       update_interval(0),
                       last_update_time(0),
                       replica(0),
                       submit_time(0),
                       update_time(0) {};
    int Replica() const {
        return states[kContainerPending].size() 
               + states[kContainerAllocating].size() 
               + states[kContainerReady].size();
    }
    typedef boost::shared_ptr<ContainerGroup> Ptr;
};

class Agent {
public:
    friend class Scheduler;
    explicit Agent(const AgentEndpoint& endpoint,
                   int64_t cpu,
                   int64_t memory,
                   const std::map<DevicePath, VolumInfo>& volums,
                   const std::set<std::string>& tags,
                   const std::string& pool_name);
    void SetAssignment(int64_t cpu_assigned,
                       int64_t memory_assigned,
                       const std::map<DevicePath, VolumInfo>& volum_assigned,
                       const std::set<std::string> port_assigned,
                       const std::map<ContainerId, Container::Ptr>& containers);
    bool TryPut(const Container* container, ResourceError& err);
    void Put(Container::Ptr container);
    void Evict(Container::Ptr container);
    typedef boost::shared_ptr<Agent> Ptr;
private:
    bool SelectDevices(const std::vector<proto::VolumRequired>& volums,
                       std::vector<DevicePath>& devices);
    bool RecurSelectDevices(size_t i, const std::vector<proto::VolumRequired>& volums,
                            std::map<DevicePath, VolumInfo>& volum_free,
                            std::vector<DevicePath>& devices,
                            std::set<DevicePath>& path_used);
    bool SelectFreePorts(const std::vector<proto::PortRequired>& ports_need,
                         std::vector<std::string>& ports_free);
    bool SelectFreeVolumContainers(const std::vector<ContainerGroupId>& volum_jobs,
                                   std::vector<ContainerId>& volum_containers);
    ContainerGroupId ExtractGroupId(const ContainerId& container_id);
    AgentEndpoint endpoint_;
    std::set<std::string> tags_;
    std::string pool_name_;
    int64_t cpu_total_;
    int64_t cpu_assigned_;
    int64_t memory_total_;
    int64_t memory_assigned_;
    std::map<DevicePath, VolumInfo> volum_total_;
    std::map<DevicePath, VolumInfo> volum_assigned_;
    std::set<std::string> port_assigned_;
    size_t port_total_;
    std::map<ContainerId, Container::Ptr> containers_;
    std::map<ContainerGroupId, int> container_counts_;
    std::map<ContainerGroupId, std::set<ContainerId> > volum_jobs_free_;
    int32_t batch_container_count_;
};

struct ContainerGroupQueueLess {
    bool operator () (const ContainerGroup::Ptr& a, const ContainerGroup::Ptr& b) {
        if (a->priority < b->priority) {
            return true;
        } else if (a->priority == b->priority) {
            return a->id < b->id;
        } else {
            return false;
        }
    }
};

class Scheduler {
public:
    explicit Scheduler();
    //start the main schueduling loop
    void Start();
    void Stop();

    void AddAgent(Agent::Ptr agent, const proto::AgentInfo& agent_info);
    void RemoveAgent(const AgentEndpoint& endpoint);
    ContainerGroupId Submit(const std::string& container_group_name,
                            const proto::ContainerDescription& container_desc,
                            int replica, int priority,
                            const std::string& user_name);
    void Reload(const proto::ContainerGroupMeta& container_group_meta);
    bool Kill(const ContainerGroupId& container_group_id);
    bool ManualSchedule(const AgentEndpoint& endpoint,
                        const ContainerGroupId& container_group_id,
                        std::string& fail_reason);

    bool ChangeReplica(const ContainerGroupId& container_group_id, int replica);
    
    // @update_interval : 
    //      --- intervals between updateing two containers, in seconds
    bool Update(const ContainerGroupId& container_group_id,
                const proto::ContainerDescription& container_desc,
                int update_interval,
                std::string& new_version);
    void AddTag(const AgentEndpoint& endpoint, const std::string& tag);
    void RemoveTag(const AgentEndpoint& endpoint, const std::string& tag);
    void SetPool(const AgentEndpoint& endpoint, const std::string& pool_name);
    void MakeCommand(const std::string& agent_endpoint,
                     const proto::AgentInfo& agent_info, 
                     std::vector<AgentCommand>& commands);
    bool ListContainerGroups(std::vector<proto::ContainerGroupStatistics>& container_groups);
    bool ShowContainerGroup(const ContainerGroupId& container_group_id,
                            std::vector<proto::ContainerStatistics>& containers);
    bool ShowAgent(const AgentEndpoint& endpoint,
                   std::vector<proto::ContainerStatistics>& containers);
    void GetContainersStatistics(const ContainerMap& containers_map,
                                 std::vector<proto::ContainerStatistics>& containers);
    void ShowUserAlloc(const std::string& user_name, proto::Quota& alloc);
    bool ChangeStatus(const ContainerGroupId& container_group_id,
                      const ContainerId& container_id,
                      ContainerStatus new_status);
    void MetaToQuota(const proto::ContainerGroupMeta& meta, proto::Quota& quota);
    bool IsBeingShared(const ContainerGroupId& container_group_id,
                       ContainerGroupId& top_container_group_id);
private:
    void ChangeStatus(Container::Ptr container,
                      proto::ContainerStatus new_status);
    void ChangeStatus(ContainerGroup::Ptr container_group,
                      Container::Ptr container,
                      proto::ContainerStatus new_status);
    void ScaleDown(ContainerGroup::Ptr container_group, int replica);
    void ScaleUp(ContainerGroup::Ptr container_group, int replica);

    ContainerGroupId GenerateContainerGroupId(const std::string& container_group_name);
    ContainerId GenerateContainerId(const ContainerGroupId& container_group_id, int offset);
    void ScheduleNextAgent(AgentEndpoint pre_endpoint);
    void CheckTagAndPool(Agent::Ptr agent);
    void CheckVersion(Agent::Ptr agent);
    bool CheckTagAndPoolOnce(Agent::Ptr agent, Container::Ptr container);
    void CheckContainerGroupGC(ContainerGroup::Ptr container_group);
    bool RequireHasDiff(const Requirement* v1, const Requirement* v2);
    void SetRequirement(Requirement::Ptr require, 
                        const proto::ContainerDescription& container_desc);
    void SetVolumsAndPorts(const Container::Ptr& container, 
                           proto::ContainerDescription& container_desc);
    std::string GetNewVersion();
    std::map<AgentEndpoint, Agent::Ptr> agents_;
    std::map<ContainerGroupId, ContainerGroup::Ptr> container_groups_;
    std::set<ContainerGroup::Ptr, ContainerGroupQueueLess> container_group_queue_;
    Mutex mu_;
    ThreadPool sched_pool_;
    ThreadPool gc_pool_;
    bool stop_;
};

} //namespace sched
} //namespace galaxy
} //namespace baidu
