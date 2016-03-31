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
               const std::string& master_key):rpc_client_(NULL),
               master_key_(master_key),nexus_(NULL),
               use_master_addr_direct_(false),
               master_addr_(),
               master_(NULL){
        rpc_client_ = new RpcClient();
        nexus_ = new ::galaxy::ins::sdk::InsSDK(nexus_servers);
    }
    
    GalaxyImpl(const std::string& master_addr):rpc_client_(NULL),
               master_key_(),nexus_(NULL),
               use_master_addr_direct_(true),
               master_addr_(master_addr),
               master_(NULL){
        rpc_client_ = new RpcClient();
    }
    virtual ~GalaxyImpl() {
        delete master_;
        delete rpc_client_;
        delete nexus_;
    }
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
    bool GetPodsByAgent(const std::string& endpoint,
                        std::vector<PodInformation>* pods);
    bool GetTasksByJob(const std::string& jobid,
                      std::vector<TaskInformation>* tasks);
    bool GetTasksByAgent(const std::string& endpoint,
                      std::vector<TaskInformation>* tasks);

    bool GetStatus(MasterStatus* status);
    bool SwitchSafeMode(bool mode);
    bool Preempt(const PreemptPropose& propose);
    bool GetMasterAddr(std::string* master_addr);
    bool OfflineAgent(const std::string& agent_addr);
    bool OnlineAgent(const std::string& agent_addr);
    bool BuildMasterClient();
private:
    bool FillJobDescriptor(const JobDescription& sdk_job, JobDescriptor* job);
    void FillResource(const Resource& res, ResDescription* res_desc);
    template<class Stub, class Request, class Response, class Callback>
    bool SendRequest(Stub*, void(Stub::*func)(
                    google::protobuf::RpcController*,
                    const Request*, Response*, Callback*),
                    const Request* request, Response* response,
                    int32_t rpc_timeout, int retry_times) {
        bool ok = rpc_client_->SendRequest(master_, func,
                                           request, response,
                                           rpc_timeout,
                                           retry_times);
        if (!ok) {
            ok = BuildMasterClient();
            if (!ok) {
                return false;
            }
            ok = rpc_client_->SendRequest(master_, func,
                                          request, response,
                                          rpc_timeout,
                                          retry_times);
        }
        return ok;
    }
private:
    RpcClient* rpc_client_;
    std::string master_key_;
    ::galaxy::ins::sdk::InsSDK* nexus_;
    bool use_master_addr_direct_;
    std::string master_addr_;
    Master_Stub* master_;
};


bool GalaxyImpl::BuildMasterClient() {
    delete master_;
    master_ = NULL;
    if (!use_master_addr_direct_) {
        bool ok = GetMasterAddr(&master_addr_);
        if (!ok) {
            return false;
        }
    }
    bool ok = rpc_client_->GetStub(master_addr_, &master_);
    if (!ok) {
        LOG(WARNING, "fail to ge master stub");
        return false;
    }
    return true;
}

bool GalaxyImpl::OnlineAgent(const std::string& agent_addr) {
    OnlineAgentRequest request;
    OnlineAgentResponse response;
    request.set_endpoint(agent_addr); 
    bool ret = rpc_client_->SendRequest(master_, &Master_Stub::OnlineAgent,
                                        &request, &response, 5, 1);
    if (!ret || 
            (response.has_status() 
            && response.status() != kOk)) {
        return false;     
    }    
    return true;
}

bool GalaxyImpl::OfflineAgent(const std::string& agent_addr) {
    OfflineAgentRequest request;
    OfflineAgentResponse response;
    request.set_endpoint(agent_addr); 
    bool ret = rpc_client_->SendRequest(master_, &Master_Stub::OfflineAgent,
                                        &request, &response, 5, 1);
    if (!ret || 
            (response.has_status() 
            && response.status() != kOk)) {
        return false;     
    }    
    return true;
}

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
    request.set_addr(propose.addr);
    bool ret = rpc_client_->SendRequest(master_, &Master_Stub::Preempt,
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



bool GalaxyImpl::LabelAgents(const std::string& label, 
                             const std::vector<std::string>& agents) {
    LabelAgentRequest request; 
    LabelAgentResponse response;
    request.mutable_labels()->set_label(label);
    for (size_t i = 0; i < agents.size(); i++) {
        request.mutable_labels()->add_agents_endpoint(agents[i]);     
    }
    bool ret = rpc_client_->SendRequest(master_, &Master_Stub::LabelAgents,
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
    rpc_client_->SendRequest(master_, &Master_Stub::TerminateJob,
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
    pod_pb->set_namespace_isolation(sdk_job.pod.namespace_isolation);
    Resource* pod_res = pod_pb->mutable_requirement();
    // pod res
    pod_res->set_millicores(sdk_job.pod.requirement.millicores);
    pod_res->set_memory(sdk_job.pod.requirement.memory);
    for (size_t i = 0; i < sdk_job.pod.requirement.ports.size(); i++) {
        pod_res->add_ports(sdk_job.pod.requirement.ports[i]);
    }
    pod_res->set_read_bytes_ps(sdk_job.pod.requirement.read_bytes_ps);
    pod_res->set_write_bytes_ps(sdk_job.pod.requirement.write_bytes_ps);
    pod_res->set_read_io_ps(sdk_job.pod.requirement.read_io_ps);
    pod_res->set_write_io_ps(sdk_job.pod.requirement.write_io_ps);
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
        task->set_namespace_isolation(sdk_job.pod.namespace_isolation);
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
        task_res->set_read_bytes_ps(task_res_desc.read_bytes_ps);
        task_res->set_write_bytes_ps(task_res_desc.write_bytes_ps);
        task_res->set_read_io_ps(task_res_desc.read_io_ps);
        task_res->set_write_io_ps(task_res_desc.write_io_ps);
        task_res->set_io_weight(task_res_desc.io_weight);
    }
    if (!sdk_job.label.empty()) {
        job->mutable_pod()->add_labels(sdk_job.label);
    }
    return true;
}

bool GalaxyImpl::GetTasksByJob(const std::string& jobid,
                               std::vector<TaskInformation>* tasks) {
    ShowTaskRequest request;
    request.set_jobid(jobid);
    ShowTaskResponse response; 
    bool ok = rpc_client_->SendRequest(master_, &Master_Stub::ShowTask,
                                  &request,&response, 5, 1);

    if (!ok || response.status() != kOk) {
        return false;
    }
    for (int i = 0; i < response.tasks_size(); i++) {
        const TaskOverview& task_overview = response.tasks(i);
        TaskInformation task_info;
        task_info.podid = task_overview.podid();
        task_info.state = TaskState_Name(task_overview.state());
        task_info.endpoint = task_overview.endpoint();
        task_info.deploy_time = task_overview.deploy_time();
        task_info.start_time = task_overview.start_time(); 
        FillResource(task_overview.used(), &task_info.used);
        tasks->push_back(task_info);
    }
    return true;

}

bool GalaxyImpl::GetTasksByAgent(const std::string& endpoint,
                                 std::vector<TaskInformation>* tasks) {
    ShowTaskRequest request;
    request.set_endpoint(endpoint);
    ShowTaskResponse response;

    bool ok = rpc_client_->SendRequest(master_, &Master_Stub::ShowTask,
                             &request,&response, 5, 1);
    if (!ok || response.status() != kOk) {
        return false;
    }
    for (int i = 0; i < response.tasks_size(); i++) {
        const TaskOverview& task_overview = response.tasks(i);
        TaskInformation task_info;
        task_info.podid = task_overview.podid();
        task_info.state = TaskState_Name(task_overview.state());
        task_info.endpoint = task_overview.endpoint();
        task_info.deploy_time = task_overview.deploy_time();
        task_info.start_time = task_overview.start_time(); 
        FillResource(task_overview.used(), &task_info.used);
        tasks->push_back(task_info);
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
    res_desc->read_bytes_ps = res.read_bytes_ps();
    res_desc->write_bytes_ps = res.write_bytes_ps();
    res_desc->syscr_ps = res.syscr_ps();
    res_desc->syscw_ps = res.syscw_ps();
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
    rpc_client_->SendRequest(master_, &Master_Stub::SubmitJob,
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
    rpc_client_->SendRequest(master_, &Master_Stub::UpdateJob,
                             &request, &response, 5, 1);
    if (response.status() != kOk) {
        return false;
    }
    return true;
}

bool GalaxyImpl::ListJobs(std::vector<JobInformation>* jobs) {
    ListJobsRequest request;
    ListJobsResponse response; 
    bool ret = rpc_client_->SendRequest(master_, &Master_Stub::ListJobs,
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
        job_info.read_bytes_ps = job.resource_used().read_bytes_ps();
        job_info.write_bytes_ps = job.resource_used().write_bytes_ps();
        job_info.syscr_ps = job.resource_used().syscr_ps();
        job_info.syscw_ps = job.resource_used().syscw_ps();
        job_info.state = JobState_Name(job.state());
        job_info.death_num = job.death_num();
        job_info.create_time = job.create_time();
        job_info.update_time = job.update_time();
        jobs->push_back(job_info);
    }
    return true;
}
bool GalaxyImpl::ShowPod(const std::string& jobid,
                         std::vector<PodInformation>* pods){
    ShowPodRequest request;
    request.set_jobid(jobid);
    ShowPodResponse response;
    bool ok = rpc_client_->SendRequest(master_, &Master_Stub::ShowPod,
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
        pod_info.pending_time = pod_overview.pending_time();
        pod_info.sched_time = pod_overview.sched_time();
        pod_info.start_time = pod_overview.start_time();
        FillResource(pod_overview.used(), &pod_info.used);
        FillResource(pod_overview.assigned(), &pod_info.assigned);
        pods->push_back(pod_info);
    }
    return true;

}

bool GalaxyImpl::GetPodsByAgent(const std::string& endpoint,
                                std::vector<PodInformation>* pods) {
    ShowPodRequest request;
    request.set_endpoint(endpoint);
    ShowPodResponse response;

    bool ok = rpc_client_->SendRequest(master_, &Master_Stub::ShowPod,
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
        pod_info.pending_time = pod_overview.pending_time();
        pod_info.sched_time = pod_overview.sched_time();
        pod_info.start_time = pod_overview.start_time(); 
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
    
    bool ok = rpc_client_->SendRequest(master_, &Master_Stub::ShowPod,
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
        pod_info.pending_time = pod_overview.pending_time();
        pod_info.sched_time = pod_overview.sched_time();
        pod_info.start_time = pod_overview.start_time(); 
        FillResource(pod_overview.used(), &pod_info.used);
        FillResource(pod_overview.assigned(), &pod_info.assigned);
        pods->push_back(pod_info);
    }
    return true;

}
bool GalaxyImpl::GetStatus(MasterStatus* status) {
    GetMasterStatusRequest request;
    GetMasterStatusResponse response;
    
    rpc_client_->SendRequest(master_, &Master_Stub::GetStatus,
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
    rpc_client_->SendRequest(master_, &Master_Stub::SwitchSafeMode, 
                             &request, &response, 5, 1);
    if (response.status() != kOk) {
        return false;
    }
    return true;
}

bool GalaxyImpl::ListAgents(std::vector<NodeDescription>* nodes) {
    ListAgentsRequest request;
    ListAgentsResponse response; 
    rpc_client_->SendRequest(master_, &Master_Stub::ListAgents,
                             &request,&response, 5, 1);
    int node_num = response.agents_size();
    for (int i = 0; i < node_num; i++) {
        const AgentInfo& node = response.agents(i);
        NodeDescription node_desc;
        node_desc.addr = node.endpoint();
        node_desc.build = node.build();
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
	GalaxyImpl* galaxy = new GalaxyImpl(nexus_servers, master_key);
    bool ok = galaxy->BuildMasterClient();
    if (!ok) {
		delete galaxy;
        return NULL;
	}
	return galaxy;
}

Galaxy* Galaxy::ConnectGalaxy(const std::string& master_addr) {
	GalaxyImpl* galaxy = new GalaxyImpl(master_addr);
    bool ok = galaxy->BuildMasterClient();
    if (!ok) {
		delete galaxy;
        return NULL;
	}
	return galaxy;
}

} // namespace galaxy
} // namespace baidu

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
