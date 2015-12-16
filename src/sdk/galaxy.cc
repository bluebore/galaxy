// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "galaxy.h"

#include <stdio.h>
#include "proto/master.pb.h"
#include "proto/galaxy.pb.h"
#include "rpc/rpc_client.h"
#include "logging.h"
#include "ins_sdk.h"
#include <boost/scoped_ptr.hpp>
namespace baidu {
namespace galaxy {

class GalaxyImpl : public Galaxy {
public:
    GalaxyImpl(const std::string& nexus_servers, 
               const std::string& master_key):master_key_(master_key){
        rpc_client_ = new RpcClient();
        nexus_ = new ::galaxy::ins::sdk::InsSDK(nexus_servers);
    }
    virtual ~GalaxyImpl() {}
    bool SubmitJob(const JobDescription& job, std::string* job_id);
    bool UpdateJob(const std::string& jobid, const JobDescription& job);
    bool ListJobs(std::vector<JobInformation>* jobs);
    bool ListAgents(std::vector<NodeDescription>* nodes);
    bool TerminateJob(const std::string& job_id);
    bool LabelAgents(const std::string& label, 
                     const std::vector<std::string>& agents);
    bool ShowPod(const std::string& jobid,
                 std::vector<PodInformation>* pods);
    bool GetPodsByName(const std::string& jobname,
                 std::vector<PodInformation>* pods);
    bool GetStatus(MasterStatus* status);
    bool SwitchSafeMode(bool mode);
    bool Preempt(const PreemptPropose& propose);
    bool GetMasterAddr(std::string* master_addr);
private:
    bool FillJobDescriptor(const JobDescription& sdk_job, JobDescriptor* job);
    void FillResource(const Resource& res, ResDescription* res_desc);
    bool BuildMasterClient(Master_Stub** master);
private:
    RpcClient* rpc_client_;
    std::string master_key_;
    ::galaxy::ins::sdk::InsSDK* nexus_; 
};

bool GalaxyImpl::Preempt(const PreemptPropose& propose) {
    PreemptRequest request;
    PreemptResponse response;
    PreemptEntity* pending_pod = request.mutable_pending_pod();
    pending_pod->set_jobid(propose.pending_pod.first);
    pending_pod->set_podid(propose.pending_pod.second);
    for (size_t i = 0; i < propose.preempted_pods.size(); i++) {
        PreemptEntity* preempt_pod = request.add_preempted_pods();
        preempt_pod->set_jobid(propose.preempted_pods[i].first);
        preempt_pod->set_podid(propose.preempted_pods[i].second);
    }
    Master_Stub* master = NULL;
    bool  ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    request.set_addr(propose.addr);
    bool ret = rpc_client_->SendRequest(master, &Master_Stub::Preempt,
                                        &request, &response, 5, 1);
    if (!ret || 
            (response.has_status() 
                    && response.status() != kOk)) {
        return false;     
    }    
    return true;

}

bool GalaxyImpl::GetMasterAddr(std::string* master_addr) {
    if (master_addr == NULL) {
        return false;
    }
    ::galaxy::ins::sdk::SDKError err;
    bool ok = nexus_->Get(master_key_, master_addr, &err);
    if (ok && err == ::galaxy::ins::sdk::kOK) {
        return true;
    }
    return false;
}

bool GalaxyImpl::BuildMasterClient(Master_Stub** master) {
    std::string master_addr;
    bool ok = GetMasterAddr(&master_addr);
    if (!ok) {
        return false;
    }
    ok = rpc_client_->GetStub(master_addr, master);
    if (!ok) {
        LOG(WARNING, "fail to ge master stub");
        return false;
    }
    return true;
}


bool GalaxyImpl::LabelAgents(const std::string& label, 
                             const std::vector<std::string>& agents) {
    LabelAgentRequest request; 
    LabelAgentResponse response;
    request.mutable_labels()->set_label(label);
    for (size_t i = 0; i < agents.size(); i++) {
        request.mutable_labels()->add_agents_endpoint(agents[i]);     
    }
    Master_Stub* master = NULL;
    bool  ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    bool ret = rpc_client_->SendRequest(master, &Master_Stub::LabelAgents,
                                        &request, &response, 5, 1);
    if (!ret || 
        (response.has_status() 
        && response.status() != kOk)) {
        return false;
    }
    return true;
}

bool GalaxyImpl::TerminateJob(const std::string& job_id) {
    TerminateJobRequest request;
    TerminateJobResponse response;
    request.set_jobid(job_id);
    Master_Stub* master = NULL;
    bool  ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    rpc_client_->SendRequest(master, &Master_Stub::TerminateJob,
                             &request,&response,5,1);
    if (response.status() == kOk) {
        return true;
    }
    return false;
}

bool GalaxyImpl::FillJobDescriptor(const JobDescription& sdk_job, 
                                   JobDescriptor* job) {
    job->set_name(sdk_job.job_name);
    // job meta
    JobType job_type;
    bool ok = JobType_Parse(sdk_job.type, &job_type);
    if (!ok) {
        return false;
    }
    job->set_type(job_type);
    job->set_priority(sdk_job.priority);
    job->set_deploy_step(sdk_job.deploy_step);
    job->set_replica(sdk_job.replica);

    // pod meta
    PodDescriptor* pod_pb = job->mutable_pod();
    pod_pb->set_version(sdk_job.pod.version);
    Resource* pod_res = pod_pb->mutable_requirement();
    // pod res
    pod_res->set_millicores(sdk_job.pod.requirement.millicores);
    pod_res->set_memory(sdk_job.pod.requirement.memory);
    for (size_t i = 0; i < sdk_job.pod.requirement.ports.size(); i++) {
        pod_res->add_ports(sdk_job.pod.requirement.ports[i]);
    }
    for (size_t i = 0; i < sdk_job.pod.tasks.size(); i++) {
        TaskDescriptor* task = pod_pb->add_tasks();
        task->set_binary(sdk_job.pod.tasks[i].binary);
        task->set_start_command(sdk_job.pod.tasks[i].start_cmd);
        task->set_stop_command(sdk_job.pod.tasks[i].stop_cmd);
        SourceType source_type;
        ok = SourceType_Parse(sdk_job.pod.tasks[i].source_type, &source_type);
        if (!ok) {
            return false;
        }
        task->set_source_type(source_type);
        MemIsolationType mem_isolation_type;
        ok = MemIsolationType_Parse(sdk_job.pod.tasks[i].mem_isolation_type, &mem_isolation_type);
        if (!ok) {
            return false;
        }
        CpuIsolationType cpu_isolation_type;
        ok = CpuIsolationType_Parse(sdk_job.pod.tasks[i].cpu_isolation_type, &cpu_isolation_type);
        if (!ok) {
            return false;
        }
        task->set_cpu_isolation_type(cpu_isolation_type);
        task->set_mem_isolation_type(mem_isolation_type);
        task->set_offset(sdk_job.pod.tasks[i].offset);
        std::set<std::string>::iterator envs_it = sdk_job.pod.tasks[i].envs.begin();
        for (;envs_it != sdk_job.pod.tasks[i].envs.end(); i++) {
            task->add_env(*envs_it);
        }
        Resource* task_res = task->mutable_requirement();
        const ResDescription& task_res_desc = sdk_job.pod.tasks[i].requirement;
        task_res->set_millicores(task_res_desc.millicores);
        task_res->set_memory(task_res_desc.memory);
        for (size_t j = 0; j < task_res_desc.ports.size(); j++) {
            task_res->add_ports(task_res_desc.ports[j]);
        }
        for (size_t j = 0; j < task_res_desc.disks.size(); j++) {
            Volume* disk = task_res->add_disks();
            disk->set_quota(task_res_desc.disks[j].quota);
            disk->set_path(task_res_desc.disks[j].path);
        }
        for (size_t j = 0; j < task_res_desc.ssds.size(); j++) {
            Volume* ssd = task_res->add_ssds();
            ssd->set_quota(task_res_desc.ssds[j].quota);
            ssd->set_path(task_res_desc.ssds[j].path);
        }
    }
    if (!sdk_job.label.empty()) {
        job->mutable_pod()->add_labels(sdk_job.label);
    }
    return true;
}

void GalaxyImpl::FillResource(const Resource& res, ResDescription* res_desc) {
    res_desc->millicores = res.millicores();
    res_desc->memory = res.memory();
    for (int j = 0; j < res.ports_size(); j++) {
        res_desc->ports.push_back(res.ports(j));
    }
    for (int j = 0; j < res.ssds_size(); j++) {
        VolumeDescription vol;
        vol.quota = res.ssds(j).quota();
        vol.path = res.ssds(j).path();
        res_desc->ssds.push_back(vol);
    }
    for (int j = 0; j < res.disks_size(); j++) {
        VolumeDescription vol;
        vol.quota= res.disks(j).quota();
        vol.path = res.disks(j).path();
        res_desc->disks.push_back(vol);
    }
}


bool GalaxyImpl::SubmitJob(const JobDescription& job, std::string* job_id){
    if (job_id == NULL) {
        return false;
    }
    SubmitJobRequest request;
    SubmitJobResponse response;
    bool ok = FillJobDescriptor(job, request.mutable_job());
    if (!ok) {
        return false;
    }
    Master_Stub* master = NULL;
    ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    rpc_client_->SendRequest(master, &Master_Stub::SubmitJob,
                             &request,&response,5,1);
    if (response.status() != kOk) {
        return false;
    }
    *job_id = response.jobid();
    return true;
}

bool GalaxyImpl::UpdateJob(const std::string& jobid, const JobDescription& job) {
    UpdateJobRequest request;
    UpdateJobResponse response;
    request.set_jobid(jobid);
    bool ok = FillJobDescriptor(job, request.mutable_job());
    if (!ok) {
        return false;
    }
    Master_Stub* master = NULL;
    ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    rpc_client_->SendRequest(master, &Master_Stub::UpdateJob,
                             &request, &response, 5, 1);
    if (response.status() != kOk) {
        return false;
    }
    return true;
}

bool GalaxyImpl::ListJobs(std::vector<JobInformation>* jobs) {
    ListJobsRequest request;
    ListJobsResponse response;
    Master_Stub* master = NULL;
    bool  ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    bool ret = rpc_client_->SendRequest(master, &Master_Stub::ListJobs,
                             &request,&response,5,1);
    if (!ret || response.status() != kOk) {
        return false;
    }
    int job_num = response.jobs_size();
    for(int i = 0; i< job_num;i++){
        const JobOverview& job = response.jobs(i);
        JobInformation job_info ;
        job_info.job_id = job.jobid();
        job_info.job_name = job.desc().name();
        job_info.replica = job.desc().replica();
        job_info.priority = job.desc().priority();
        job_info.running_num = job.running_num();
        job_info.pending_num = job.pending_num();
        job_info.deploying_num = job.deploying_num();
        job_info.cpu_used = job.resource_used().millicores();
        job_info.mem_used = job.resource_used().memory();
        job_info.is_batch = (job.desc().type() == kBatch);
        job_info.state = JobState_Name(job.state());
        jobs->push_back(job_info);
    }
    return true;
}
bool GalaxyImpl::ShowPod(const std::string& jobid,
                         std::vector<PodInformation>* pods){
    ShowPodRequest request;
    request.set_jobid(jobid);
    ShowPodResponse response;
    Master_Stub* master = NULL;
    bool ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    ok = rpc_client_->SendRequest(master, &Master_Stub::ShowPod,
                             &request,&response, 5, 1);
    if (!ok || response.status() != kOk) {
        return false;
    }
    for (int i = 0; i < response.pods_size(); i++) {
        const PodOverview& pod_overview = response.pods(i);
        PodInformation pod_info;
        pod_info.jobid = pod_overview.jobid();
        pod_info.podid = pod_overview.podid();
        pod_info.stage = PodStage_Name(pod_overview.stage());
        pod_info.state = PodState_Name(pod_overview.state());
        pod_info.version = pod_overview.version();
        pod_info.endpoint = pod_overview.endpoint();
        FillResource(pod_overview.used(), &pod_info.used);
        FillResource(pod_overview.assigned(), &pod_info.assigned);
        pods->push_back(pod_info);
    }
    return true;

}

bool GalaxyImpl::GetPodsByName(const std::string& jobname,
                         std::vector<PodInformation>* pods){
    ShowPodRequest request;
    request.set_name(jobname);
    ShowPodResponse response;
    Master_Stub* master = NULL;
    bool ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    ok = rpc_client_->SendRequest(master, &Master_Stub::ShowPod,
                             &request,&response, 5, 1);
    if (!ok || response.status() != kOk) {
        return false;
    }
    for (int i = 0; i < response.pods_size(); i++) {
        const PodOverview& pod_overview = response.pods(i);
        PodInformation pod_info;
        pod_info.jobid = pod_overview.jobid();
        pod_info.podid = pod_overview.podid();
        pod_info.stage = PodStage_Name(pod_overview.stage());
        pod_info.state = PodState_Name(pod_overview.state());
        pod_info.version = pod_overview.version();
        pod_info.endpoint = pod_overview.endpoint();
        FillResource(pod_overview.used(), &pod_info.used);
        FillResource(pod_overview.assigned(), &pod_info.assigned);
        pods->push_back(pod_info);
    }
    return true;

}
bool GalaxyImpl::GetStatus(MasterStatus* status) {
    GetMasterStatusRequest request;
    GetMasterStatusResponse response;
    Master_Stub* master = NULL;
    bool  ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    rpc_client_->SendRequest(master, &Master_Stub::GetStatus,
                             &request, &response, 5, 1);
    if (response.status() != kOk) {
        return false;
    }
    status->safe_mode = response.safe_mode();

    status->agent_total = response.agent_total();
    status->agent_live_count = response.agent_live_count();
    status->agent_dead_count = response.agent_dead_count();

    status->cpu_total = response.cpu_total();
    status->cpu_used = response.cpu_used();
    status->cpu_assigned = response.cpu_assigned();

    status->mem_total = response.mem_total();
    status->mem_used = response.mem_used();
    status->mem_assigned = response.mem_assigned();

    status->job_count = response.job_count();
    status->pod_count = response.pod_count();
    status->scale_up_job_count = response.scale_up_job_count();
    status->scale_down_job_count = response.scale_down_job_count();
    status->need_update_job_count = response.need_update_job_count();
    return true;
}
bool GalaxyImpl::SwitchSafeMode(bool mode) {
    SwitchSafeModeRequest request;
    SwitchSafeModeResponse response;
    request.set_enter_or_leave(mode);
    Master_Stub* master = NULL;
    bool  ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    rpc_client_->SendRequest(master, &Master_Stub::SwitchSafeMode, 
                             &request, &response, 5, 1);
    if (response.status() != kOk) {
        return false;
    }
    return true;
}

bool GalaxyImpl::ListAgents(std::vector<NodeDescription>* nodes) {
    ListAgentsRequest request;
    ListAgentsResponse response;
    Master_Stub* master = NULL;
    bool  ok = BuildMasterClient(&master);
    if (!ok) {
        return false;
    }
    boost::scoped_ptr<Master_Stub> scoped_master(master);
    rpc_client_->SendRequest(master, &Master_Stub::ListAgents,
                             &request,&response, 5, 1);
    int node_num = response.agents_size();
    for (int i = 0; i < node_num; i++) {
        const AgentInfo& node = response.agents(i);
        NodeDescription node_desc;
        node_desc.addr = node.endpoint();
        node_desc.task_num = node.pods_size();
        node_desc.cpu_share = node.total().millicores();
        node_desc.mem_share = node.total().memory();
        node_desc.cpu_assigned = node.assigned().millicores();
        node_desc.mem_assigned = node.assigned().memory();
        node_desc.cpu_used = node.used().millicores();
        node_desc.mem_used = node.used().memory();
        std::string labels;
        for (int label_ind = 0; label_ind < node.tags_size(); label_ind++) {
            labels.append(node.tags(label_ind));     
            if (label_ind != node.tags_size() - 1) {
                labels.append(",");
            }
        }
        node_desc.labels = labels;
        node_desc.state = AgentState_Name(node.state());
        nodes->push_back(node_desc);
    }
    return true;

}

Galaxy* Galaxy::ConnectGalaxy(const std::string& nexus_servers, const std::string& master_key) {
    return new GalaxyImpl(nexus_servers, master_key);
}

} // namespace galaxy
} // namespace baidu

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
