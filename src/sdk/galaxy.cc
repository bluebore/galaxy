// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "galaxy.h"

#include "proto/master.pb.h"
#include "rpc/rpc_client.h"

namespace baidu {
namespace galaxy {

class GalaxyImpl : public Galaxy {
public:
    GalaxyImpl(const std::string& master_addr)
      : master_(NULL) {
        rpc_client_ = new RpcClient();
        rpc_client_->GetStub(master_addr, &master_);
    }
    virtual ~GalaxyImpl() {}
    std::string SubmitJob(const JobDescription& job);
    bool UpdateJob(const std::string& jobid, const JobDescription& job);
    bool ListJobs(std::vector<JobInformation>* jobs);
    bool ListAgents(std::vector<NodeDescription>* nodes);
    bool TerminateJob(const std::string& job_id);
    bool LabelAgents(const std::string& label, 
                     const std::vector<std::string>& agents);
private:
    RpcClient* rpc_client_;
    Master_Stub* master_;
};

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

    return true;
}

std::string GalaxyImpl::SubmitJob(const JobDescription& job){
    SubmitJobRequest request;
    SubmitJobResponse response;
    request.mutable_job()->set_name(job.job_name);
    if (job.is_batch) {
        request.mutable_job()->set_type(kBatch);
    }
    request.mutable_job()->set_deploy_step(job.deploy_step);
    Resource* pod_resource = request.mutable_job()->mutable_pod()->mutable_requirement();
    pod_resource->set_millicores(job.cpu_required);
    pod_resource->set_memory(job.mem_required);
    TaskDescriptor* task = request.mutable_job()->mutable_pod()->add_tasks();
    task->set_binary(job.binary);
    task->set_start_command(job.cmd_line);
    task->mutable_requirement()->set_millicores(job.cpu_required);
    task->mutable_requirement()->set_memory(job.mem_required);
    request.mutable_job()->set_replica(job.replica);
    rpc_client_->SendRequest(master_, &Master_Stub::SubmitJob,
                             &request,&response,5,1);
    return response.jobid();
}

bool GalaxyImpl::UpdateJob(const std::string& /*jobid*/, const JobDescription& job) {
    UpdateJobRequest request;
    UpdateJobResponse response;
    request.mutable_job()->set_name(job.job_name);
    TaskDescriptor* task = request.mutable_job()->mutable_pod()->add_tasks();
    task->set_binary(job.binary);
    task->set_start_command(job.cmd_line);
    task->mutable_requirement()->set_millicores(job.cpu_required);
    task->mutable_requirement()->set_memory(job.mem_required);
    request.mutable_job()->set_replica(job.replica);

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
        jobs->push_back(job_info);
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
        nodes->push_back(node_desc);
    }
    return true;

}

Galaxy* Galaxy::ConnectGalaxy(const std::string& master_addr) {
    return new GalaxyImpl(master_addr);
}

} // namespace galaxy
} // namespace baidu

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
