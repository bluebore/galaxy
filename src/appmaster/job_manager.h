// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_GALAXY_JOB_MANGER_H
#define BAIDU_GALAXY_JOB_MANGER_H
#include <string>
#include <set>
#include <map>
#include <vector>
#include <thread_pool.h>
#include "ins_sdk.h"
#include "protocol/resman.pb.h"
#include "protocol/appmaster.pb.h"
#include "protocol/galaxy.pb.h"
#include "rpc/rpc_client.h"
#include "naming/private_sdk.h"

namespace baidu {
namespace galaxy {

using ::baidu::galaxy::proto::JobStatus;
using ::baidu::galaxy::proto::JobInfo;
using ::baidu::galaxy::proto::PodInfo;
using ::baidu::galaxy::proto::JobDescription;
using ::baidu::galaxy::proto::JobEvent;
using ::baidu::galaxy::proto::UpdateAction;
using ::baidu::galaxy::proto::Status;
using ::baidu::galaxy::proto::PodStatus;
using ::baidu::galaxy::proto::PodDescription;
using ::baidu::galaxy::proto::kOk;
using ::baidu::galaxy::proto::kError;
using ::baidu::galaxy::proto::kTerminate;
using ::baidu::galaxy::proto::kAddAgentFail;
using ::baidu::galaxy::proto::kSuspend;
using ::baidu::galaxy::proto::kJobNotFound;
using ::baidu::galaxy::proto::kPodNotFound;
using ::baidu::galaxy::proto::kUserNotMatch;
using ::baidu::galaxy::proto::kCreateContainerGroupFail;
using ::baidu::galaxy::proto::kRemoveContainerGroupFail;
using ::baidu::galaxy::proto::kUpdateContainerGroupFail;
using ::baidu::galaxy::proto::kRebuild;
using ::baidu::galaxy::proto::kReload;
using ::baidu::galaxy::proto::kQuit;
using ::baidu::galaxy::proto::kStatusConflict;
using ::baidu::galaxy::proto::kJobTerminateFail;
using ::baidu::galaxy::proto::kJobPending; 
using ::baidu::galaxy::proto::kJobRunning;
using ::baidu::galaxy::proto::kJobFinished; 
using ::baidu::galaxy::proto::kJobDestroying;
using ::baidu::galaxy::proto::kJobUpdating;
using ::baidu::galaxy::proto::kJobUpdatePause;
using ::baidu::galaxy::proto::kFetch;
using ::baidu::galaxy::proto::kUpdate;
using ::baidu::galaxy::proto::kRemove;
using ::baidu::galaxy::proto::kRemoveFinish;
using ::baidu::galaxy::proto::kUpdateFinish;
using ::baidu::galaxy::proto::kPauseUpdate;
using ::baidu::galaxy::proto::kUpdateContinue;
using ::baidu::galaxy::proto::kUpdateRollback;
using ::baidu::galaxy::proto::kUpdateCancel;
using ::baidu::galaxy::proto::kActionNull;
using ::baidu::galaxy::proto::kActionReload;
using ::baidu::galaxy::proto::kActionRebuild;
using ::baidu::galaxy::proto::kActionRecreate;
using ::baidu::galaxy::proto::kPodPending;
using ::baidu::galaxy::proto::kPodReady;
using ::baidu::galaxy::proto::kPodDeploying;
using ::baidu::galaxy::proto::kPodStarting;
using ::baidu::galaxy::proto::kPodRunning;
using ::baidu::galaxy::proto::kPodServing;
using ::baidu::galaxy::proto::kPodFailed;
using ::baidu::galaxy::proto::kPodFinished;
using ::baidu::galaxy::proto::kPodStopping;
using ::baidu::galaxy::proto::kPodTerminated;
using ::baidu::galaxy::proto::ResMan_Stub;
using ::baidu::galaxy::proto::User;
using ::baidu::galaxy::proto::ServiceInfo;
using ::baidu::galaxy::proto::User;

typedef std::string JobId;
typedef std::string Version;
typedef std::string PodId;
typedef std::string ServiceName;
typedef baidu::galaxy::proto::JobOverview JobOverview;
typedef ::google::protobuf::RepeatedPtrField<JobOverview> JobOverviewList;
typedef ::google::protobuf::RepeatedPtrField<ServiceInfo> ServiceList;

struct Job {
    JobStatus status_;
    User user_;
    std::map<PodId, PodInfo*> pods_;
    std::map<PodId, PodInfo*> history_pods_;
    JobDescription desc_;
    JobDescription last_desc_;
    JobId id_;
    std::set<PodId> deploying_pods_;
    std::set<PodId> reloading_pods_;
    std::set<PodId> recreate_pods_;
    UpdateAction action_type_;
    int64_t create_time_;
    int64_t update_time_;
    int64_t rollback_time_;
    uint32_t updated_cnt_;
    std::map<std::string, PublicSdk*> naming_sdk_;
};

typedef boost::function<Status (Job* job, void* arg)> TransFunc;
struct FsmTrans {
    JobStatus next_status_;
    TransFunc trans_func_;
};

class JobManager {
public:
    void Start();
    Status Add(const JobId& job_id, const JobDescription& job_desc, const User& user);
    Status Update(const JobId& job_id, const JobDescription& job_desc,
                    bool container_change);
    Status Terminate(const JobId& jobid, const User& user);
    Status PauseUpdate(const JobId& job_id);
    Status ContinueUpdate(const JobId& job_id, int32_t break_point);
    Status CancelUpdate(const JobId& job_id);
    Status Rollback(const JobId& job_id);


    Status HandleFetch(const ::baidu::galaxy::proto::FetchTaskRequest* request,
                     ::baidu::galaxy::proto::FetchTaskResponse* response);
    Status RecoverPod(const User& user, const std::string jobid, const std::string podid);

    void ReloadJobInfo(const JobInfo& job_info);
    void GetJobsOverview(JobOverviewList* jobs_overview);
    void SetResmanEndpoint(std::string new_endpoint);
    Status GetJobInfo(const JobId& jobid, JobInfo* job_info);
    JobDescription GetLastDesc(const JobId jonid);
    void Run();
    JobManager();
    ~JobManager();
private:
    std::string BuildFsmKey(const JobStatus& status,
                            const JobEvent& event);
    FsmTrans* BuildFsmValue(const JobStatus& status,
                            const TransFunc& func);
    void BuildFsm();
    void BuildDispatch();
    void BuildAging();
    void CheckPending(Job* job);
    void CheckRunning(Job* job);
    void CheckUpdating(Job* job);
    void CheckDestroying(Job* job);
    void CheckClear(Job* job);
    void CheckJobStatus(Job* job);
    void CheckPodAlive(PodInfo* pod, Job* job);
    void CheckPauseUpdate(Job* job);
    Status StartJob(Job* job, void* arg);
    Status RecoverJob(Job* job, void* arg);
    Status UpdateJob(Job* job, void* arg);
    Status RemoveJob(Job* job, void* arg);
    Status ClearJob(Job* job, void* arg);
    Status PauseUpdateJob(Job* job, void * arg);
    Status CancelUpdateJob(Job* job, void * arg);
    PodInfo* CreatePod(Job* job,
                std::string podid,
                std::string endpoint);
    Status PodHeartBeat(Job* job, void* arg);
    Status UpdatePod(Job* job, void* arg);
    Status DistroyPod(Job* job, void* arg);
    bool SaveToNexus(const Job* job);
    bool DeleteFromNexus(const JobId& job_id);
    Status ContinueUpdateJob(Job* job, void* arg);
    Status RollbackJob(Job* job, void* arg);
    Status PauseUpdatePod(Job* job, void* arg);
    Status TryRebuild(Job* job, PodInfo* podinfo);
    Status TryReload(Job* job, PodInfo* pod);
    Status TryReCreate(Job* job, PodInfo* pod);
    void ReduceUpdateList(Job* job, std::string podid, PodStatus pod_status,
                            PodStatus reload_status);
    bool ReachBreakpoint(Job* job);
    void RefreshPod(::baidu::galaxy::proto::FetchTaskRequest* request,
                    PodInfo* podinfo,
                    Job* job);

    bool IsSerivceSame(const ServiceInfo& src, const ServiceInfo& dest);
    void RefreshService(Job* job, ServiceList* src, PodInfo* pod);
    void DestroyService(Job* job, PodInfo* pod);
    void EraseFormDeployList(JobId jobid, std::string podid);
    void EraseFormReCreateList(JobId jobid, std::string podid);
    void RebuildPods(Job* job,
                    const ::baidu::galaxy::proto::FetchTaskRequest* request);
    void CheckDeployingAlive(std::string id, JobId jobid);

private:
    std::map<JobId, Job*> jobs_;
    // agent some custom settings eg mark agent offline
    ThreadPool job_checker_;
    ThreadPool pod_checker_;
    Mutex mutex_;
    Mutex resman_mutex_;
    std::string resman_endpoint_;
    RpcClient rpc_client_;
    // nexus
    ::galaxy::ins::sdk::InsSDK* nexus_;
    //job fsm
    typedef std::map<std::string, FsmTrans*> FSM;
    FSM fsm_;
    //job process
    typedef boost::function<Status (Job* job, void*)> DispatchFunc;
    typedef boost::function<void (Job* job)> AgingFunc;
    std::map<std::string, DispatchFunc> dispatch_;
    std::map<std::string, AgingFunc> aging_;
    bool running_;
};

}
}

#endif
