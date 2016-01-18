// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "master_impl.h"
#include <gflags/gflags.h>
#include "master_util.h"
#include <logging.h>
#include <sofa/pbrpc/pbrpc.h>

DECLARE_string(nexus_servers);
DECLARE_string(nexus_root_path);
DECLARE_string(master_lock_path);
DECLARE_string(master_path);
DECLARE_string(jobs_store_path);
DECLARE_string(agents_store_path);
DECLARE_string(labels_store_path);
DECLARE_int32(max_scale_down_size);
DECLARE_int32(max_scale_up_size);
DECLARE_int32(max_need_update_job_size);

namespace baidu {
namespace galaxy {

const std::string LABEL_PREFIX = "LABEL_";

MasterImpl::MasterImpl() : nexus_(NULL){
    nexus_ = new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_servers);
    user_manager_ = new UserManager();
}

MasterImpl::~MasterImpl() {
    delete nexus_;
}

static void OnMasterLockChange(const ::galaxy::ins::sdk::WatchParam& param, 
                               ::galaxy::ins::sdk::SDKError /*error*/) {
    MasterImpl* master = static_cast<MasterImpl*>(param.context);
    master->OnLockChange(param.value);
}

static void OnMasterSessionTimeout(void* ctx) {
    MasterImpl* master = static_cast<MasterImpl*>(ctx);
    master->OnSessionTimeout();
}

void MasterImpl::OnLockChange(std::string lock_session_id) {
    std::string self_session_id = nexus_->GetSessionID();
    if (self_session_id != lock_session_id) {
        LOG(FATAL, "master lost lock , die.");
        abort();
    }
}

void MasterImpl::OnSessionTimeout() {
    LOG(FATAL, "master lost session with nexus, die.");
    abort();
}

void MasterImpl::Init() {
    AcquireMasterLock();
    bool ok = user_manager_->Init();
    if (!ok) {
        assert(0);
    }
    LOG(INFO, "reload users from nexus successfully");
    ReloadJobInfo();
    ReloadLabelInfo();
    ReloadAgent();
}

void MasterImpl::Start() {
    job_manager_.Start();
}

void MasterImpl::ReloadLabelInfo() {
    std::string start_key = FLAGS_nexus_root_path + FLAGS_labels_store_path + "/";
    std::string end_key = start_key + "~";
    ::galaxy::ins::sdk::ScanResult* result = nexus_->Scan(start_key, end_key);
    int label_amount = 0;
    while (!result->Done()) {
        assert(result->Error() == ::galaxy::ins::sdk::kOK);
        std::string key = result->Key();
        std::string label_raw_data = result->Value();
        LabelCell label;
        bool ok = label.ParseFromString(label_raw_data);
        if (ok) {
            LOG(INFO, "reload label: %s", label.label().c_str()); 
            job_manager_.LabelAgents(label);
        } else {
            LOG(WARNING, "faild to parse label: %s", key.c_str());
        }
        result->Next();
        label_amount ++;
    }
    LOG(INFO, "reload label info %d", label_amount);
    return;
}

void MasterImpl::ReloadAgent() {
    std::string start_key = FLAGS_nexus_root_path + FLAGS_agents_store_path + "/";
    std::string end_key = start_key + "~"; 
    ::galaxy::ins::sdk::ScanResult* result = nexus_->Scan(start_key, end_key);
    int agent_amount = 0;
    while (!result->Done()) { 
        assert(result->Error() == ::galaxy::ins::sdk::kOK);
        std::string key = result->Key();
        std::string value = result->Value();
        AgentPersistenceInfo agent;
        bool ok = agent.ParseFromString(value);
        if (ok) {
            LOG(INFO, "reload agent %s", agent.endpoint().c_str());
            job_manager_.ReloadAgent(agent);
            agent_amount ++;
        }else {
            LOG(WARNING, "fail to parse agent info with key %s", key.c_str());
        }
        result->Next();
    }
    LOG(INFO, "reload agent count %d", agent_amount);
}

void MasterImpl::ReloadJobInfo() {
    std::string start_key = FLAGS_nexus_root_path + FLAGS_jobs_store_path + "/";
    std::string end_key = start_key + "~";
    ::galaxy::ins::sdk::ScanResult* result = nexus_->Scan(start_key, end_key);
    int job_amount = 0;
    User root;
    bool ok = user_manager_->GetSuperUser(&root);
    if (!ok) {
        assert(0);
    }
    while (!result->Done()) {
        assert(result->Error() == ::galaxy::ins::sdk::kOK);
        std::string key = result->Key();
        std::string job_raw_data = result->Value(); 
        JobInfo job_info;
        bool ok = job_info.ParseFromString(job_raw_data);
        if (!ok) {
            LOG(WARNING, "faild to parse job_info: %s", key.c_str());
            assert(0);
        }
        PodDescriptor pod_desc;
        bool find_desc = false;
        for (int j = 0; j < job_info.pod_descs_size(); ++j) {
            if (job_info.pod_descs(j).version() != job_info.latest_version())  {
                continue;
            }
            pod_desc.CopyFrom(job_info.pod_descs(j));
            find_desc = true;
        }
        if (!find_desc) {
            continue;
        }
        int64_t total_millicores = pod_desc.requirement().millicores() * job_info.desc().replica();
        int64_t total_memory = pod_desc.requirement().memory() * job_info.desc().replica();
 
        User owner;
        ok = user_manager_->GetUserById(job_info.uid(), &owner); 
        if (ok) {
            LOG(INFO, "reload job: %s", job_info.jobid().c_str());
            job_manager_.ReloadJobInfo(job_info, owner);
            // TODO check return
            user_manager_->AcquireQuota(owner.uid(), total_millicores, total_memory);
        } else {
            LOG(WARNING, "faild to get user with id %s, assigned to root user", job_info.uid().c_str());
            job_manager_.ReloadJobInfo(job_info, root);
            // TODO check return
            user_manager_->AcquireQuota(root.uid(), total_millicores, total_memory);
        }
        result->Next();
        job_amount ++;
    }
    LOG(INFO, "reload all job desc finish, total#: %d", job_amount);
}

void MasterImpl::GetJobDescriptor(::google::protobuf::RpcController* controller,
                                    const ::baidu::galaxy::GetJobDescriptorRequest* request,
                                    ::baidu::galaxy::GetJobDescriptorResponse* response,
                                    ::google::protobuf::Closure* done) {
    job_manager_.GetJobDescByDiff(request->jobs(),
                                  response->mutable_jobs(),
                                  response->mutable_deleted_jobs());
    response->set_status(kOk);
    done->Run();
}

void MasterImpl::AcquireMasterLock() {
    std::string master_lock = FLAGS_nexus_root_path + FLAGS_master_lock_path;
    ::galaxy::ins::sdk::SDKError err;
    nexus_->RegisterSessionTimeout(&OnMasterSessionTimeout, this);
    bool ret = nexus_->Lock(master_lock, &err); //whould block until accquired
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    std::string master_endpoint = MasterUtil::SelfEndpoint();
    std::string master_path_key = FLAGS_nexus_root_path + FLAGS_master_path;
    ret = nexus_->Put(master_path_key, master_endpoint, &err);
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    ret = nexus_->Watch(master_lock, &OnMasterLockChange, this, &err);
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    LOG(INFO, "master lock [ok].  %s -> %s", 
        master_path_key.c_str(), master_endpoint.c_str());
}

void MasterImpl::SubmitJob(::google::protobuf::RpcController* /*controller*/,
                           const ::baidu::galaxy::SubmitJobRequest* request,
                           ::baidu::galaxy::SubmitJobResponse* response,
                           ::google::protobuf::Closure* done) {
    std::string sid = request->sid();
    User user;
    bool ok = user_manager_->Auth(sid, &user);
    if (!ok) {
        response->set_status(kSessionTimeout);
        done->Run();
        return;
    }
    const JobDescriptor& job_desc = request->job();
    int64_t total_millicores = job_desc.pod().requirement().millicores() * job_desc.replica();
    int64_t total_memory = job_desc.pod().requirement().memory() * job_desc.replica();
    ok = user_manager_->HasEnoughQuota(user.uid(), total_millicores, total_memory);
    if (!ok) {
        response->set_status(kQuota);
        done->Run();
        return;
    }
    JobInfo exist_job;
    ok = job_manager_.GetJobByName(job_desc.name(), &exist_job);
    if (ok) {
        response->set_status(kNameExists);
        done->Run();
        return;
    }
    MasterUtil::TraceJobDesc(job_desc);
    JobId job_id = MasterUtil::UUID();
    Status status = job_manager_.Add(job_id, job_desc, user);
    response->set_status(status);
    if (status == kOk) {
        response->set_jobid(job_id);
        ok = user_manager_->AcquireQuota(user.uid(), total_millicores, total_memory);
        if (!ok) {
            LOG(WARNING, "acquire quota from user %s fails, require cpu %lld, require mem %lld",
                    user.name().c_str(),
                    total_millicores,
                    total_memory);
        }
    }
    done->Run();
}

void MasterImpl::ShowQuota(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::ShowQuotaRequest* request,
                           ::baidu::galaxy::ShowQuotaResponse* response,
                           ::google::protobuf::Closure* done) {
    std::string sid = request->sid();
    User user;
    bool ok = user_manager_->Auth(sid, &user);
    if (!ok) {
        response->set_status(kSessionTimeout);
        done->Run();
        return;
    }
    Quota quota;
    ok = user_manager_->GetQuota(user.uid(), &quota);
    if (!ok) {
        response->set_status(kNoQuota);
        done->Run();
        return;
    }
    response->set_status(kOk);
    response->set_cpu_quota(quota.cpu_quota());
    response->set_memory_quota(quota.memory_quota());
    response->set_cpu_assigned(quota.cpu_assigned());
    response->set_memory_assigned(quota.memory_assigned());
    done->Run();
}

void MasterImpl::UpdateJob(::google::protobuf::RpcController* /*controller*/,
                           const ::baidu::galaxy::UpdateJobRequest* request,
                           ::baidu::galaxy::UpdateJobResponse* response,
                           ::google::protobuf::Closure* done) {
    std::string sid = request->sid();
    User user;
    bool ok = user_manager_->Auth(sid, &user);
    if (!ok) {
        response->set_status(kSessionTimeout);
        done->Run();
        return;
    }
    JobInfo exist_job;
    ok = job_manager_.GetJobByName(request->name(), &exist_job);
    if (!ok) { 
        LOG(WARNING, "fail to get job name %s ", request->name().c_str());
        response->set_status(kJobNotFound);
        done->Run();
        return;
    }

    if (exist_job.uid() != user.uid()) {
        response->set_status(kPermissionDenied);
        done->Run();
        LOG(WARNING, "user % has no permission to kill job %s ", user.uid().c_str(),
                request->name().c_str());
        return;
    }  
    ok = UpdateQuotaWithNewJob(exist_job, request->job());
    if (!ok) {
        response->set_status(kQuota);
        done->Run();
        return;
    }
    LOG(INFO, "update job %s replica %d", request->name().c_str(), request->job().replica());
    Status status = job_manager_.Update(exist_job.jobid(), request->job());
    response->set_status(status);
    done->Run();
}

bool MasterImpl::UpdateQuotaWithNewJob(const JobInfo& job,
                                       const JobDescriptor& new_desc) {
    PodDescriptor old;
    bool find_desc = false;
    for (int j = 0; j < job.pod_descs_size(); ++j) {
        if (job.pod_descs(j).version() != job.latest_version())  {
            continue;
        }
        old.CopyFrom(job.pod_descs(j));
        find_desc = true;
    }
    if (!find_desc) {
        return false;
    }
    int64_t total_old_millicores = old.requirement().millicores() * job.desc().replica();
    int64_t total_old_memory = old.requirement().memory() * job.desc().replica();
    int64_t total_new_millicores = new_desc.replica() * new_desc.pod().requirement().millicores();
    int64_t total_new_memory = new_desc.replica() * new_desc.pod().requirement().memory();
    int64_t need_millicores = total_new_millicores - total_old_millicores;
    int64_t need_memory = total_new_memory - total_old_memory;
    bool ok = user_manager_->HasEnoughQuota(job.uid(), need_millicores, need_memory);
    if (!ok) {
        return false;
    }
    ok = user_manager_->ReleaseQuota(job.uid(), total_old_millicores, total_old_memory);
    ok = user_manager_->AcquireQuota(job.uid(), total_new_millicores, total_new_memory);
    return ok;
}

void MasterImpl::SuspendJob(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::SuspendJobRequest* /*request*/,
                            ::baidu::galaxy::SuspendJobResponse* /*response*/,
                            ::google::protobuf::Closure* done) {
    controller->SetFailed("Method TerminateJob() not implemented.");
    done->Run();
}

void MasterImpl::ResumeJob(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::ResumeJobRequest* /*request*/,
                           ::baidu::galaxy::ResumeJobResponse* /*response*/,
                           ::google::protobuf::Closure* done) { 
    controller->SetFailed("Method TerminateJob() not implemented.");
    done->Run();
}

void MasterImpl::TerminateJob(::google::protobuf::RpcController* ,
                              const ::baidu::galaxy::TerminateJobRequest* request,
                              ::baidu::galaxy::TerminateJobResponse* response,
                              ::google::protobuf::Closure* done) {
    std::string sid = request->sid();
    User user;
    bool ok = user_manager_->Auth(sid, &user);
    if (!ok) {
        response->set_status(kSessionTimeout);
        done->Run();
        return;
    }
    JobInfo exist_job;
    ok = job_manager_.GetJobByName(request->job_name(), &exist_job);
    if (!ok) { 
        LOG(WARNING, "fail to get job name %s ", request->job_name().c_str());
        response->set_status(kJobNotFound);
        done->Run();
        return;
    }

    if (exist_job.uid() != user.uid()) {
        response->set_status(kPermissionDenied);
        done->Run();
        LOG(WARNING, "user % has no permission to kill job %s ", user.uid().c_str(),
                request->job_name().c_str());
        return;
    }
    PodDescriptor pod_desc;
    bool find_desc = false;
    LOG(INFO, "POD DESC SIZE %d", exist_job.pod_descs_size());
    for (int j = 0; j < exist_job.pod_descs_size(); ++j) {
        if (exist_job.pod_descs(j).version() != exist_job.latest_version())  {
            continue;
        }
        pod_desc.CopyFrom(exist_job.pod_descs(j));
        find_desc = true;
    }
    if (!find_desc) {
        LOG(WARNING, "fail to pod desc for job %s", exist_job.jobid().c_str());
        response->set_status(kJobNotFound);
        done->Run();
        return;
    }
    int64_t total_millicores = pod_desc.requirement().millicores() * exist_job.desc().replica();
    int64_t total_memory = pod_desc.requirement().memory() * exist_job.desc().replica();
    ok = user_manager_->ReleaseQuota(user.uid(), total_millicores, total_memory);
    if (!ok) {
        LOG(WARNING, "fail to release quota for job %s", request->job_name().c_str());
        response->set_status(kQuota);
        return;
    }
    LOG(INFO, "terminate job %s", exist_job.jobid().c_str());
    Status status= job_manager_.Terminte(exist_job.jobid());
    response->set_status(status);
    done->Run();
}

void MasterImpl::ShowJob(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::ShowJobRequest* request,
                         ::baidu::galaxy::ShowJobResponse* response,
                         ::google::protobuf::Closure* done) {
    for (int32_t i = 0; i < request->jobsid_size(); i++) {
        const JobId& jobid = request->jobsid(i);
        if (kOk != job_manager_.GetJobInfo(jobid, response->mutable_jobs()->Add())) {
            response->mutable_jobs()->RemoveLast();
        }
    }
    response->set_status(kOk);
    done->Run();
}

void MasterImpl::ListJobs(::google::protobuf::RpcController* /*controller*/,
                          const ::baidu::galaxy::ListJobsRequest* /*request*/,
                          ::baidu::galaxy::ListJobsResponse* response,
                          ::google::protobuf::Closure* done) {
    job_manager_.GetJobsOverview(response->mutable_jobs());
    response->set_status(kOk);
    done->Run();
}

void MasterImpl::HeartBeat(::google::protobuf::RpcController* /*controller*/,
                          const ::baidu::galaxy::HeartBeatRequest* request,
                          ::baidu::galaxy::HeartBeatResponse*,
                          ::google::protobuf::Closure* done) {
    std::string agent_addr = request->endpoint();
    job_manager_.KeepAlive(agent_addr);
    done->Run();
}

void MasterImpl::SwitchSafeMode(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::SwitchSafeModeRequest* request,
                           ::baidu::galaxy::SwitchSafeModeResponse* response,
                           ::google::protobuf::Closure* done) {
    Status ok = job_manager_.SetSafeMode(request->enter_or_leave());
    response->set_status(ok);
    done->Run();
}

void MasterImpl::GetPendingJobs(::google::protobuf::RpcController* controller,
                                const ::baidu::galaxy::GetPendingJobsRequest* request,
                                ::baidu::galaxy::GetPendingJobsResponse* response,
                                ::google::protobuf::Closure* done) {
    int32_t max_scale_up_size = FLAGS_max_scale_up_size;
    int32_t max_scale_down_size = FLAGS_max_scale_down_size;
    int32_t max_need_update_job_size = FLAGS_max_need_update_job_size;
    if (request->has_max_scale_up_size() 
        && request->max_scale_up_size() > 0) {
        max_scale_up_size = request->max_scale_up_size();
    }
    if (request->has_max_scale_down_size()
        && request->max_scale_down_size() > 0) {
        max_scale_down_size = request->max_scale_down_size();
    }
    if (request->has_max_need_update_job_size()
        && request->max_need_update_job_size() > 0) {
        max_need_update_job_size = request->max_need_update_job_size();
    }

    sofa::pbrpc::RpcController* sf_ctrl = (sofa::pbrpc::RpcController*) controller;
    response->set_status(kOk);
    LOG(INFO, "sched request from %s", sf_ctrl->RemoteAddress().c_str());
    job_manager_.GetPendingPods(response->mutable_scale_up_jobs(),
                                max_scale_up_size,
                                response->mutable_scale_down_jobs(),
                                max_scale_down_size,
                                response->mutable_need_update_jobs(),
                                max_need_update_job_size,
                                done);
}

void MasterImpl::GetResourceSnapshot(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::GetResourceSnapshotRequest* request,
                         ::baidu::galaxy::GetResourceSnapshotResponse* response,
                         ::google::protobuf::Closure* done) {
    response->set_status(kOk);
    job_manager_.GetAliveAgentsByDiff(request->versions(),
                                      response->mutable_agents(), 
                                      response->mutable_deleted_agents(),
                                      done);
}

void MasterImpl::Propose(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::ProposeRequest* request,
                         ::baidu::galaxy::ProposeResponse* response,
                         ::google::protobuf::Closure* done) {
    for (int i = 0; i < request->schedule_size(); i++) {
        const ScheduleInfo& sched_info = request->schedule(i);
        Status status = job_manager_.Propose(sched_info);
        response->set_status(status);
    }
    done->Run();
}

bool MasterImpl::PrePropose(const ScheduleInfo& sched_info) {
    // only launch a pending will do quota calculation
    if (sched_info.action() != kLaunch) {
        return true;
    }
    JobInfo job_info;
    Status status = job_manager_.GetJobInfo(sched_info.jobid(), &job_info);
    if (status != kOk) {
        return false;
    }
    PodDescriptor desc;
    bool find_desc = false;
    for (int j = 0; j < job_info.pod_descs_size(); ++j) {
        if (job_info.pod_descs(j).version() != job_info.latest_version())  {
            continue;
        }
        desc.CopyFrom(job_info.pod_descs(j));
        find_desc = true;
    }
    if (!find_desc) {
        return false;
    }
    bool ok = user_manager_->HasEnoughQuota(job_info.uid(),
                                            desc.requirement().millicores(),
                                            desc.requirement().memory());
    if (!ok) {
        LOG(WARNING, "user %s has no enough quota to use", job_info.uid().c_str());
        return false;
    }
    return true;
}

bool MasterImpl::PostPropose(const ScheduleInfo& sched_info) {
    // only launch a pending will do quota calculation when run pod successfully
    if (sched_info.action() != kLaunch) {
        return true;
    }
    JobInfo job_info;
    Status status = job_manager_.GetJobInfo(sched_info.jobid(), &job_info);
    if (status != kOk) {
        LOG(WARNING, "fail to get job %s", sched_info.jobid().c_str());
        return false;
    }
    PodDescriptor desc;
    bool find_desc = false;
    for (int j = 0; j < job_info.pod_descs_size(); ++j) {
        if (job_info.pod_descs(j).version() != job_info.latest_version())  {
            continue;
        }
        desc.CopyFrom(job_info.pod_descs(j));
        find_desc = true;
    }
    if (!find_desc) {
        return false;
    }
    bool ok = user_manager_->AcquireQuota(job_info.uid(), desc.requirement().millicores(),
                                              desc.requirement().memory());
    if (!ok) {
        LOG(WARNING, "user with uid %s has no enough quota to user", job_info.uid().c_str());
    }
    return ok;
}


void MasterImpl::ListAgents(::google::protobuf::RpcController* /*controller*/,
                            const ::baidu::galaxy::ListAgentsRequest* /*request*/,
                            ::baidu::galaxy::ListAgentsResponse* response,
                            ::google::protobuf::Closure* done) {
    job_manager_.GetAgentsInfo(response->mutable_agents());
    response->set_status(kOk);
    done->Run();
}


void MasterImpl::LabelAgents(::google::protobuf::RpcController* ,
                             const ::baidu::galaxy::LabelAgentRequest* request,
                             ::baidu::galaxy::LabelAgentResponse* response,
                             ::google::protobuf::Closure* done) { 
    Status status = job_manager_.LabelAgents(request->labels());
    response->set_status(status);
    done->Run();
    return;
}

void MasterImpl::ShowPod(::google::protobuf::RpcController* /*controller*/,
                         const ::baidu::galaxy::ShowPodRequest* request,
                         ::baidu::galaxy::ShowPodResponse* response,
                         ::google::protobuf::Closure* done) {
    response->set_status(kInputError);
    do {
        std::string job_id;
        std::string agent_addr;
        if (request->has_jobid()) {
            job_id = request->jobid();
        } else if (request->has_name()) {
            JobInfo job;
            bool ok = job_manager_.GetJobByName(request->name(), &job);
            if (!ok) {
                break;
            }
            job_id = job.jobid();
        } else if (request->has_endpoint()) {
            agent_addr = request->endpoint();
        }
        if (!job_id.empty()) {
            Status ok = job_manager_.GetPods(job_id, 
                                     response->mutable_pods());
            response->set_status(ok);
        }else if (!agent_addr.empty()) {
            Status ok = job_manager_.GetPodsByAgent(agent_addr, 
                                     response->mutable_pods());
            response->set_status(ok);
        }
    }while(0);
    done->Run(); 
}

void MasterImpl::ShowTask(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::ShowTaskRequest* request,
                           ::baidu::galaxy::ShowTaskResponse* response,
                           ::google::protobuf::Closure* done) {
    response->set_status(kInputError);
    do {
        std::string jobname;
        std::string agent_addr;
        if (request->has_name()) {
            jobname = request->name();
        }else if (request->has_endpoint()) {
            agent_addr = request->endpoint();
        }
        if (!jobname.empty()) {
            Status ok = job_manager_.GetTaskByJob(jobname, 
                                     response->mutable_tasks());
            response->set_status(ok);
        }else if (!agent_addr.empty()) {
            Status ok = job_manager_.GetTaskByAgent(agent_addr, 
                                     response->mutable_tasks());
            response->set_status(ok);
        }
    }while(0);
    done->Run(); 


}

void MasterImpl::GetStatus(::google::protobuf::RpcController*,
                           const ::baidu::galaxy::GetMasterStatusRequest* ,
                           ::baidu::galaxy::GetMasterStatusResponse* response,
                           ::google::protobuf::Closure* done) {
    Status ok = job_manager_.GetStatus(response);
    response->set_status(ok);
    done->Run();
}
void MasterImpl::Preempt(::google::protobuf::RpcController* controller,
                           const ::baidu::galaxy::PreemptRequest* request,
                           ::baidu::galaxy::PreemptResponse* response,
                           ::google::protobuf::Closure* done) {
    std::vector<PreemptEntity> preempted_pods;
    for (int i = 0; i < request->preempted_pods_size(); i++) {
        preempted_pods.push_back(request->preempted_pods(i));
    }
    bool ok = job_manager_.Preempt(request->pending_pod(),
                                   preempted_pods,
                                   request->addr());
    if (ok) {
        response->set_status(kOk);
    }else {
        response->set_status(kInputError);
    }
    done->Run();
}

void MasterImpl::Login(::google::protobuf::RpcController*,
                       const LoginRequest* request,
                       LoginResponse* response,
                       ::google::protobuf::Closure* done) {
    User user;
    std::string sid;
    bool ok = user_manager_->Login(request->name(), 
                                   request->password(),
                                   &user,
                                   &sid);
    if (!ok) {
        LOG(WARNING, "user %s login failed", request->name().c_str());
        response->set_status(kInputError);
        done->Run();
        return;
    }else {
        LOG(INFO, "user %s login successed with sid %s", request->name().c_str(),
                sid.c_str());
    }
    response->set_status(kOk);
    response->set_sid(sid);
    done->Run();
}

void MasterImpl::AddUser(::google::protobuf::RpcController*,
                         const AddUserRequest* request,
                         AddUserResponse* response,
                         ::google::protobuf::Closure* done) {
    User user;
    bool ok = user_manager_->Auth(request->sid(), &user);
    if (!ok) {
        response->set_status(kSessionTimeout);
        done->Run();
        return;
    }
    if (!user.super_user()) {
        response->set_status(kPermissionDenied);
        done->Run();
        return;
    }
    ok = user_manager_->AddUser(request->user());
    if (!ok) {
        LOG(WARNING, "fail to add user %s", request->user().name().c_str());
        response->set_status(kInputError);
        done->Run();
        return;
    }
    response->set_status(kOk);
    done->Run();
}


void MasterImpl::AssignQuota(::google::protobuf::RpcController* controller,
                             const ::baidu::galaxy::AssignQuotaRequest* request,
                             ::baidu::galaxy::AssignQuotaResponse* response,
                             ::google::protobuf::Closure* done) {
    User user;
    bool ok = user_manager_->Auth(request->sid(), &user);
    if (!ok) {
        response->set_status(kSessionTimeout);
        done->Run();
        return;
    }
    if (!user.super_user()) {
        response->set_status(kPermissionDenied);
        done->Run();
        return;
    }
    Quota quota;
    quota.set_cpu_quota(request->cpu_quota());
    quota.set_memory_quota(request->memory_quota());
    ok = user_manager_->AssignQuotaByName(request->name(), quota);
    if (!ok) {
        response->set_status(kQuota);
    }else  {
        response->set_status(kOk);
    }
    done->Run();
}

void MasterImpl::OfflineAgent(::google::protobuf::RpcController* controller,
                              const ::baidu::galaxy::OfflineAgentRequest* request,
                              ::baidu::galaxy::OfflineAgentResponse* response,
                              ::google::protobuf::Closure* done) {

    bool ok = job_manager_.OfflineAgent(request->endpoint());
    if (!ok) {
        response->set_status(kAgentError);
    }else {
        response->set_status(kOk);
    }
    done->Run();
}

void MasterImpl::OnlineAgent(::google::protobuf::RpcController* controller,
                              const ::baidu::galaxy::OnlineAgentRequest* request,
                              ::baidu::galaxy::OnlineAgentResponse* response,
                              ::google::protobuf::Closure* done) {

    bool ok = job_manager_.OnlineAgent(request->endpoint());
    if (!ok) {
        response->set_status(kAgentError);
    }else {
        response->set_status(kOk);
    }
    done->Run();
}

}
}
