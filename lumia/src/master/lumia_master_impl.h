// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_LUMIA_MASTER_LUMIA_IMPL_H
#define BAIDU_LUMIA_MASTER_LUMIA_IMPL_H
#include <set>
#include "proto/lumia.pb.h"
#include "mutex.h"
#include "thread_pool.h"
#include "ins_sdk.h"
#include "master/minion_ctrl.h"
#include "rpc/rpc_client.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

using ::galaxy::ins::sdk::InsSDK;

namespace baidu {
namespace lumia {

struct MinionIndex {
    std::string id_;
    std::string hostname_;
    std::string ip_;
    Minion* minion_;
    MinionIndex(std::string id, const std::string& hostname, const std::string ip, Minion* minion):id_(id), hostname_(hostname), ip_(ip), minion_(minion){}
};

enum CtrlTaskState {
    kCtrlTaskPending,
    kCtrlTaskRunning,
    kCtrlTaskFails,
    kCtrlTaskExit
};

struct Task {
    CtrlTaskState state_;
    std::string addr_;
    int32_t offset_;
    std::string job_id_;
};

struct Job {
    std::string id_;
    std::string content_;
    std::string user_;
    int64_t running_num_;
    std::set<std::string> hosts_;
    std::map<int32_t, Task*> tasks_;
    int32_t step_size_;
};


struct id_tag{};
struct hostname_tag{};
struct ip_tag{};

typedef boost::multi_index_container<
    MinionIndex,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<id_tag>,  BOOST_MULTI_INDEX_MEMBER(MinionIndex , std::string ,id_)>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<hostname_tag>, BOOST_MULTI_INDEX_MEMBER(MinionIndex, std::string, hostname_)>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<ip_tag>, BOOST_MULTI_INDEX_MEMBER(MinionIndex, std::string, ip_)>
    >
> MinionSet;

typedef boost::multi_index::index<MinionSet, id_tag>::type minion_set_id_index_t;

typedef boost::multi_index::index<MinionSet, hostname_tag>::type minion_set_hostname_index_t;

typedef boost::multi_index::index<MinionSet, ip_tag>::type minion_set_ip_index_t;

class LumiaMasterImpl : public LumiaCtrl {

public:
    LumiaMasterImpl();
    ~LumiaMasterImpl();
    void Init();
    // galaxy log checker report dead agent
    void ReportDeadMinion(::google::protobuf::RpcController* controller,
                          const ::baidu::lumia::ReportDeadMinionRequest* request,
                          ::baidu::lumia::ReportDeadMinionResponse* response,
                          ::google::protobuf::Closure* done);

    void GetMinion(::google::protobuf::RpcController* controller,
                   const ::baidu::lumia::GetMinionRequest* request,
                   ::baidu::lumia::GetMinionResponse* response,
                   ::google::protobuf::Closure* done);

    void ImportData(::google::protobuf::RpcController* controller,
                    const ::baidu::lumia::ImportDataRequest* request,
                    ::baidu::lumia::ImportDataResponse* response,
                    ::google::protobuf::Closure* done);
    void Ping(::google::protobuf::RpcController* controller,
              const ::baidu::lumia::PingRequest* request,
              ::baidu::lumia::PingResponse* response,
              ::google::protobuf::Closure* done);
    void GetOverview(::google::protobuf::RpcController* controller,
                    const ::baidu::lumia::GetOverviewRequest* request,
                    ::baidu::lumia::GetOverviewResponse* response,
                    ::google::protobuf::Closure* done);
    void GetStatus(::google::protobuf::RpcController* controller,
                    const ::baidu::lumia::GetStatusRequest* request,
                    ::baidu::lumia::GetStatusResponse* response,
                    ::google::protobuf::Closure* done);
    void OnSessionTimeout();

    void OnLockChange(const std::string& sessionid);

    void Exec(::google::protobuf::RpcController* controller,
              const ::baidu::lumia::ExecTaskRequest* request,
              ::baidu::lumia::ExecTaskResponse* response,
              ::google::protobuf::Closure* done);
private:
    void HandleDeadReport(const std::string& ip);
    void CheckDeadCallBack(const std::string sessionid, 
                           const std::vector<std::string> success,
                           const std::vector<std::string> fails);
    void RebootCallBack(const std::string sessionid,
                        const std::vector<std::string> success,
                        const std::vector<std::string> fails);

    void HandleInitAgent(const std::vector<std::string> hosts);
    void InitAgentCallBack(const std::string sessionid,
                           const std::vector<std::string> success,
                           const std::vector<std::string> fails);

    bool DoInitAgent(const std::vector<std::string> hosts,
                     const std::string scripts);

    void AcquireLumiaLock();

    void HandleNodeOffline(const std::string& node_addr);

    void ScheduleTask();
    void ScheduleNextQuery();
    bool RunTask(Job* job, Task* task);
    void RunTaskCallback(const ExecRequest* request,
                         ExecResponse* response,
                         bool fails, int , 
                         const std::string& node_addr);
    void LaunchQuery();
    void QueryNode(const std::string& node_addr);
    void QueryCallBack(const QueryRequest* request,
                       QueryResponse* response,
                       bool fails,
                       int error,
                       const std::string& node_addr);
    std::string GetUUID();
private:
    MinionSet minion_set_;
    ::baidu::common::Mutex mutex_;
    ::baidu::common::Mutex timer_mutex_;
    ::baidu::common::ThreadPool workers_;
    ::baidu::common::ThreadPool dead_checkers_;
    MinionCtrl* minion_ctrl_;

    // system check script config
    std::map<std::string, std::string> scripts_;

    // minion under process
    std::set<std::string> under_process_;

    InsSDK* nexus_;

    // 
    std::set<std::string> live_nodes_;
    std::set<std::string> dead_nodes_;
    std::map<std::string, int64_t>  node_timers_;
    typedef std::map<std::string, std::map<std::string, Task*> > TasksOnAgent;
    std::map<std::string, Job*> jobs_;
    TasksOnAgent tasks_on_agent_;
    int64_t query_node_count_;
    ::baidu::galaxy::RpcClient* rpc_client_;

    std::set<std::string>* job_under_working_;
    std::set<std::string>* job_completed_;
};

}
}
#endif
