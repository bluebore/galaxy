// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "galaxy.h"

#include "proto/master.pb.h"
#include "rpc/rpc_client.h"

static const char* MEMORY_UNIT_NAME[] = {"Byte", "KB", "MB", "GB", "PB"};
static int MEMORY_UNIT_SIZE = 5;

namespace galaxy {
class GalaxyImpl : public Galaxy {
public:
    GalaxyImpl(const std::string& master_addr)
      : master_(NULL) {
        rpc_client_ = new RpcClient();
        rpc_client_->GetStub(master_addr, &master_);
    }
    virtual ~GalaxyImpl() {}
    int64_t NewJob(const JobDescription& job);
    bool UpdateJob(const JobDescription& job);
    bool ListJob(std::vector<JobInstanceDescription>* jobs);
    bool ListNode(std::vector<NodeDescription>* nodes);
    bool TerminateJob(int64_t job_id);
    bool ListTask(int64_t job_id,
                  int64_t task_id,
                  std::vector<TaskDescription>* tasks);
    bool KillTask(int64_t task_id);
    bool ListTaskByAgent(const std::string& agent_addr,
                         std::vector<TaskDescription> * tasks) ;
    bool TagAgent(const std::string& tag, std::set<std::string>* agents);
private:
    RpcClient* rpc_client_;
    Master_Stub* master_;
};

bool GalaxyImpl::KillTask(int64_t task_id){
    TerminateTaskRequest request;
    request.set_task_id(task_id);
    TerminateTaskResponse response;
    rpc_client_->SendRequest(master_,
            &Master_Stub::TerminateTask,
            &request, &response, 5, 1);
    if (response.has_status()
            && response.status() == kMasterResponseOK) {
        fprintf(stdout, "SUCCESS\n");
    }
    else {
        if (response.has_status()) {
            fprintf(stdout, "FAIL %d\n", response.status());
        }
        else {
            fprintf(stdout, "FAIL unkown\n");
        }
    }
    return true;
}
bool GalaxyImpl::TerminateJob(int64_t job_id) {
    KillJobRequest request;
    KillJobResponse response;
    request.set_job_id(job_id);
    rpc_client_->SendRequest(master_, &Master_Stub::KillJob,
                             &request,&response,5,1);

    return true;
}

int64_t GalaxyImpl::NewJob(const JobDescription& job){
    NewJobRequest request;
    NewJobResponse response;
    request.set_job_name(job.job_name);
    request.set_job_raw(job.pkg.source);
    request.set_cmd_line(job.cmd_line);
    request.set_replica_num(job.replicate_count);
    request.set_cpu_share(job.cpu_share);
    request.set_mem_share(job.mem_share);
    request.set_monitor_conf(job.monitor_conf);
    //目前只支持单个tag
    if (!job.restrict_tag.empty()) {
        request.add_restrict_tags(job.restrict_tag);
    }
    if(job.deploy_step_size > 0){
        request.set_deploy_step_size(job.deploy_step_size);
    }
    if (job.cpu_limit > 0) {
        request.set_cpu_limit(job.cpu_limit); 
    }
    request.set_one_task_per_host(job.one_task_per_host);
    rpc_client_->SendRequest(master_, &Master_Stub::NewJob,
                             &request,&response,5,1);
    return response.job_id();
}

bool GalaxyImpl::UpdateJob(const JobDescription& job) {
    UpdateJobRequest request;
    UpdateJobResponse response;
    request.set_job_id(job.job_id);
    request.set_replica_num(job.replicate_count);
    request.set_deploy_step_size(job.deploy_step_size);
    rpc_client_->SendRequest(master_, &Master_Stub::UpdateJob,
                             &request, &response, 5, 1);
    if (response.status() != kMasterResponseOK) {
        return false;
    }
    return true;
}

bool GalaxyImpl::ListJob(std::vector<JobInstanceDescription>* jobs) {
    ListJobRequest request;
    ListJobResponse response;
    rpc_client_->SendRequest(master_, &Master_Stub::ListJob,
                             &request,&response,5,1);
    int job_num = response.jobs_size();
    for(int i = 0; i< job_num;i++){
        const JobInstance& job = response.jobs(i);
        JobInstanceDescription job_instance ;
        job_instance.job_id = job.job_id();
        job_instance.job_name = job.job_name();
        job_instance.running_task_num = job.running_task_num();
        job_instance.replicate_count = job.replica_num();
        jobs->push_back(job_instance);
    }
    return true;
}

bool GalaxyImpl::ListNode(std::vector<NodeDescription>* nodes) {
    ListNodeRequest request;
    ListNodeResponse response;
    rpc_client_->SendRequest(master_, &Master_Stub::ListNode,
                             &request,&response,5,1);
    int node_num = response.nodes_size();
    for (int i = 0; i < node_num; i++) {
        const NodeInstance& node = response.nodes(i);
        NodeDescription node_desc;
        node_desc.addr = node.addr();
        node_desc.node_id = node.node_id();
        node_desc.task_num = node.task_num();
        node_desc.cpu_share = node.cpu_share();
        node_desc.mem_share = node.mem_share();
        node_desc.cpu_used = node.cpu_used();
        node_desc.mem_used = node.mem_used();
        for (int inner_index = 0; inner_index < node.tags_size(); inner_index++) {
            node_desc.tags.insert(node.tags(inner_index));
        }
        nodes->push_back(node_desc);
    }
    return true;

}
bool GalaxyImpl::ListTaskByAgent(const std::string& agent_addr,
                                 std::vector<TaskDescription>* /*tasks*/){


    ListTaskRequest request;
    request.set_agent_addr(agent_addr);
    ListTaskResponse response;
    rpc_client_->SendRequest(master_, &Master_Stub::ListTask,
                             &request, &response, 5, 1);
    fprintf(stdout, "================================\n");
    int task_size = response.tasks_size();
    for (int i = 0; i < task_size; i++) {
        if (!response.tasks(i).has_info() ||
                !response.tasks(i).info().has_task_id()) {
            continue;
        }
        int64_t task_id = response.tasks(i).info().task_id();
        std::string task_name;
        std::string agent_addr;
        std::string state;
        if (response.tasks(i).has_info()){
            if (response.tasks(i).info().has_task_name()) {
                task_name = response.tasks(i).info().task_name();
            }
        }
        if (response.tasks(i).has_status()) {
            int task_state = response.tasks(i).status();
            if (TaskState_IsValid(task_state)) {
                state = TaskState_Name((TaskState)task_state);
            }
        }
        if (response.tasks(i).has_agent_addr()) {
            agent_addr = response.tasks(i).agent_addr();
        }
        fprintf(stdout, "%ld\t%s\t%s\t%s\n", task_id, task_name.c_str(), state.c_str(), agent_addr.c_str());
    }
    fprintf(stdout, "================================\n");
    return true;

}
bool GalaxyImpl::ListTask(int64_t job_id,
                          int64_t task_id,
                          std::vector<TaskDescription>* /*tasks*/) {
    ListTaskRequest request;
    if (task_id != -1) {
        request.set_task_id(task_id);
    } else if (job_id != -1) {
        request.set_job_id(job_id);
    }
    ListTaskResponse response;
    rpc_client_->SendRequest(master_, &Master_Stub::ListTask,
                             &request, &response, 5, 1);
    fprintf(stdout, "================================\n");
    int task_size = response.tasks_size();
    for (int i = 0; i < task_size; i++) {
        if (!response.tasks(i).has_info() ||
                !response.tasks(i).info().has_task_id()) {
            continue;
        }
        int64_t task_id = response.tasks(i).info().task_id();
        std::string task_name;
        std::string agent_addr;
        std::string state;
        double cpu_usage = 0.0;
        int64_t memory_usage = 0;
        if (response.tasks(i).has_info()){
            if (response.tasks(i).info().has_task_name()) {
                task_name = response.tasks(i).info().task_name();
            }
        }
        if (response.tasks(i).has_status()) {
            int task_state = response.tasks(i).status();
            if (TaskState_IsValid(task_state)) {
                state = TaskState_Name((TaskState)task_state);
            }
        }
        if (response.tasks(i).has_agent_addr()) {
            agent_addr = response.tasks(i).agent_addr();
        }

        if (response.tasks(i).has_cpu_usage()) {
            cpu_usage = response.tasks(i).cpu_usage();
        }

        if (response.tasks(i).has_memory_usage()) {
            memory_usage = response.tasks(i).memory_usage();
        }

        fprintf(stdout, "%ld\t%s\t%s\t%s\t%f\t",
                task_id,
                task_name.c_str(),
                state.c_str(),
                agent_addr.c_str(),
                cpu_usage);
        double memory_formated = 0.0;
        int order;
        for (order = MEMORY_UNIT_SIZE - 1; order >= 0; order--) {
            memory_formated = memory_usage * 1.0 / (1L << (order * 10));
            if (memory_formated - 1 > 0.001) {
                fprintf(stdout, "%f %s\n",
                        memory_formated,
                        MEMORY_UNIT_NAME[order]);
                break;
            }
        }
        if (order < 0) {
            fprintf(stdout, "\n");
        }
    }
    fprintf(stdout, "================================\n");
    if (response.scheduled_tasks_size() > 0) {
        fprintf(stdout, "=============sched tasks =======\n");
        int sched_tasks = response.scheduled_tasks_size();
        for (int sched_index = 0; 
                sched_index < sched_tasks; ++sched_index) {
            const TaskInstance& instance = response.scheduled_tasks(sched_index);
            std::string state = TaskState_Name((TaskState)instance.status());
            fprintf(stdout, "%ld\t%s\t%d\t%d\t%s:%s\n", 
                    instance.info().task_id(),
                    state.c_str(),
                    instance.start_time(),
                    instance.end_time(),
                    instance.agent_addr().c_str(),
                    instance.root_path().c_str());     

        }
    }

    return true;
}

bool GalaxyImpl::TagAgent(const std::string& tag, std::set<std::string>* agents) {
    TagAgentRequest request;
    TagEntity entity = request.tag_entity();
    entity.set_tag(tag);
    std::set<std::string>::iterator it = agents->begin();
    for (; it != agents->end(); ++it) {
        entity.add_agents(*it);
    }
    TagAgentResponse response;
    rpc_client_->SendRequest(master_, &Master_Stub::TagAgent,
                             &request, &response, 5, 1);
    if (response.status() == 0) {
        return true;
    }
    return false; 
}


Galaxy* Galaxy::ConnectGalaxy(const std::string& master_addr) {
    return new GalaxyImpl(master_addr);
}

}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
