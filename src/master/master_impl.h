// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#ifndef  GALAXY_MASTER_IMPL_H_
#define  GALAXY_MASTER_IMPL_H_

#include "proto/master.pb.h"

#include <map>
#include <queue>
#include <set>
#include <functional>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include "common/mutex.h"
#include "common/thread_pool.h"

namespace galaxy {

class Agent_Stub;
class Scheduler;

struct AgentInfo {
    int64_t id;
    std::string addr;
    double cpu_share;
    int64_t mem_share;
    double cpu_used;
    int64_t mem_used;
    Agent_Stub* stub;
    int32_t alive_timestamp;
    std::set<int64_t> running_tasks;
    int64_t version;
};

struct AgentLoad{
    double load;
    double cpu_left;
    int64_t mem_left;
    int64_t agent_id;
    std::string agent_addr;
    AgentLoad(const AgentInfo& agent,double load):load(load){
        mem_left = agent.mem_share - agent.mem_used;
        cpu_left = agent.cpu_share - agent.cpu_used;
        agent_id = agent.id;
        agent_addr = agent.addr;
    }

    void operator()(AgentLoad& l){
        l.load = load;
        l.cpu_left = cpu_left;
        l.mem_left = mem_left;
        l.agent_addr = agent_addr;
    }
};

//agent load index includes agent id index and cpu-left index
typedef boost::multi_index::multi_index_container<
    AgentLoad,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
            boost::multi_index::member<AgentLoad,int64_t,&AgentLoad::agent_id>
        >,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::member<AgentLoad,double,&AgentLoad::cpu_left>,
            std::greater<double>
            >
    >
> AgentLoadIndex;

struct JobInfo {
    int64_t id;
    int32_t replica_num;
    std::string job_name;
    std::string job_raw;
    std::string cmd_line;
    int32_t running_num;
    int32_t scale_down_time;
    double cpu_share;
    int64_t mem_share;
    std::map<std::string, std::set<int64_t> > agent_tasks;
    std::map<std::string, std::set<int64_t> > complete_tasks;
    bool killed;
};

//agent load id index
typedef boost::multi_index::nth_index<AgentLoadIndex,0>::type agent_id_index;
//agent load cpu left index
typedef boost::multi_index::nth_index<AgentLoadIndex,1>::type cpu_left_index;

class RpcClient;

class MasterImpl : public Master {
public:
    MasterImpl();
    virtual ~MasterImpl();
public:
    void HeartBeat(::google::protobuf::RpcController* controller,
                   const ::galaxy::HeartBeatRequest* request,
                   ::galaxy::HeartBeatResponse* response,
                   ::google::protobuf::Closure* done);
    void NewJob(::google::protobuf::RpcController* controller,
                 const ::galaxy::NewJobRequest* request,
                 ::galaxy::NewJobResponse* response,
                 ::google::protobuf::Closure* done);
    void UpdateJob(::google::protobuf::RpcController* controller,
                   const ::galaxy::UpdateJobRequest* request,
                   ::galaxy::UpdateJobResponse* response,
                   ::google::protobuf::Closure* done);
    void KillJob(::google::protobuf::RpcController* controller,
                       const ::galaxy::KillJobRequest* request,
                       ::galaxy::KillJobResponse* response,
                       ::google::protobuf::Closure* done);
    void TerminateTask(::google::protobuf::RpcController* controller,
                       const ::galaxy::TerminateTaskRequest* request,
                       ::galaxy::TerminateTaskResponse* response,
                       ::google::protobuf::Closure* done);
    void ListJob(::google::protobuf::RpcController* controller,
                 const ::galaxy::ListJobRequest* request,
                 ::galaxy::ListJobResponse* response,
                 ::google::protobuf::Closure* done);

    void ListTask(::google::protobuf::RpcController* controller,
                 const ::galaxy::ListTaskRequest* request,
                 ::galaxy::ListTaskResponse* response,
                 ::google::protobuf::Closure* done);
    void ListNode(::google::protobuf::RpcController* controller,
                  const ::galaxy::ListNodeRequest* request,
                  ::galaxy::ListNodeResponse* response,
                  ::google::protobuf::Closure* done);

private:
    void DeadCheck();
    void Schedule();
    bool ScheduleTask(JobInfo* job, const std::string& agent_addr);
    void UpdateJobsOnAgent(AgentInfo* agent,
                           const std::set<int64_t>& running_tasks,
                           bool clear_all = false);
    bool CancelTaskOnAgent(AgentInfo* agent, int64_t task_id);
    void ScaleDown(JobInfo* job);

    void DelayRemoveZombieTaskOnAgent(AgentInfo * agent,int64_t task_id);
    void ListTaskForAgent(const std::string& agent_addr,
        ::google::protobuf::RepeatedPtrField<TaskInstance >* tasks);
    void ListTaskForJob(int64_t job_id,
        ::google::protobuf::RepeatedPtrField<TaskInstance >* tasks);

    void SaveIndex(const AgentInfo& agent);
    void RemoveIndex(int64_t agent_id);
    double CalcLoad(const AgentInfo& agent);
    std::string AllocResource(const JobInfo& job);
private:
    common::ThreadPool thread_pool_;
    std::map<std::string, AgentInfo> agents_;
    std::map<int64_t, TaskInstance> tasks_;
    std::map<int64_t, JobInfo> jobs_;
    std::map<int32_t, std::set<std::string> > alives_;
    int64_t next_agent_id_;
    int64_t next_task_id_;
    int64_t next_job_id_;
    Mutex agent_lock_;

    RpcClient* rpc_client_;
    AgentLoadIndex index_;
    Scheduler* scheduler_;
};

} // namespace galaxy

#endif  // GALAXY_MASTER_IMPL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
