// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "agent_impl.h"

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <errno.h>
#include <string.h>
#include "common/util.h"
#include "rpc/rpc_client.h"
#include "proto/task.pb.h"
#include "common/httpserver.h"
#include <gflags/gflags.h>
#include "agent/dynamic_resource_scheduler.h"
#include "agent/persistence_handler.h"

DECLARE_string(master_addr);
DECLARE_string(agent_port);
DECLARE_int32(agent_http_port);
DECLARE_int32(agent_http_server_threads);
DECLARE_string(agent_work_dir);
DECLARE_double(cpu_num);
DECLARE_int64(mem_bytes);
DECLARE_string(pam_pwd_dir);
DECLARE_string(agent_checkpoint_path);
DECLARE_int32(agent_heartbeat_timeout);
DECLARE_int32(agent_heartbeat_interval);

namespace galaxy {

bool AgentImpl::Init() {
    if (persistence_handler_ == NULL
            || rpc_client_ == NULL
            || ws_mgr_ == NULL 
            || task_mgr_ == NULL) {
        return false;
    }

    if (persistence_handler_->Init() != 0) {
        LOG(WARNING, "persistence handler init failed"); 
        return false;
    }

    if (!ws_mgr_->Init()) {
        LOG(FATAL, "workspace manager init failed");
        return false;
    }

    if (!task_mgr_->Init()) {
        LOG(FATAL, "task manager init failed");
        return false;
    }

    std::vector<PersistenceCell> cells;
    if (persistence_handler_->Scan(&cells) != 0) {
        LOG(FATAL, "scan tasks info failed"); 
        return false;
    }

    for (size_t i = 0; i < cells.size(); i++) {
        TaskInfo task_info; 
        if (!task_info.ParseFromString(cells[i].value)) {
            LOG(FATAL, "task info parse from failed");         
            return false;
        }
        TaskResourceRequirement requirement;
        requirement.cpu_limit = task_info.required_cpu();
        requirement.mem_limit = task_info.required_mem();
        if (resource_mgr_->Allocate(requirement, 
                    task_info.task_id()) != 0) {
            LOG(FATAL, "fail to allocate resource"
                    " for task %ld job %ld", 
                    task_info.task_id(),
                    task_info.job_id());
            return false;
        }
        if (ws_mgr_->Add(task_info) != 0) {
            LOG(FATAL, "recover workspace for "
                    "task %ld job %ld failed",
                    task_info.task_id(),
                    task_info.job_id()); 
            return false;
        }

        if (task_mgr_->Add(
                    task_info, 
                    ws_mgr_->GetWorkspace(task_info), 
                    false) != 0) {
            LOG(FATAL, "recover task runner for "
                    "task %ld job %ld failed",
                    task_info.task_id(),
                    task_info.job_id()); 
            return false;
        }
        std::vector<TaskStatus> task_status;
        // if status not run, clear it.
        task_mgr_->Status(task_status, task_info.task_id());
        LOG(INFO, "recover task %ld job %ld status %s",
                task_info.task_id(),
                task_info.job_id(),
                TaskState_Name((TaskState)(task_status[0].status())).c_str());
    }

    // start to report
    thread_pool_.AddTask(boost::bind(&AgentImpl::Report, this));
    return true;
}

AgentImpl::AgentImpl() {
    persistence_handler_ = new PersistenceHandler(FLAGS_agent_checkpoint_path);
    rpc_client_ = new RpcClient();
    ws_mgr_ = new WorkspaceManager(FLAGS_agent_work_dir);
    task_mgr_ = new TaskManager();
    
    AgentResource resource;
    resource.total_cpu = FLAGS_cpu_num;
    resource.total_mem = FLAGS_mem_bytes;
    resource.the_left_cpu = resource.total_cpu;
    resource.the_left_mem = resource.total_mem;
    resource_mgr_ = new ResourceManager(resource);

    // give all idle cpu to scheduler 
    DynamicResourceScheduler* dy_scheduler 
        = GetDynamicResourceScheduler();
    TaskResourceRequirement require;
    require.cpu_limit = resource.total_cpu;
    dy_scheduler->Release(require);

    if (!rpc_client_->GetStub(FLAGS_master_addr, &master_)) {
        assert(0);
    }
    version_ = 0;
    http_server_ = new common::HttpFileServer(FLAGS_agent_work_dir,
                                              FLAGS_agent_http_port);
    http_server_->Start(FLAGS_agent_http_server_threads);
}

AgentImpl::~AgentImpl() {
    delete ws_mgr_;
    delete task_mgr_;
    delete resource_mgr_;
    delete http_server_;
    delete persistence_handler_;
}

void AgentImpl::Report() {
    HeartBeatRequest request;
    HeartBeatResponse response;
    std::string addr = common::util::GetLocalHostName() + ":" + FLAGS_agent_port;
    std::vector<TaskStatus > status_vector;
    task_mgr_->Status(status_vector);
    std::vector<TaskStatus>::iterator it = status_vector.begin();
    for(; it != status_vector.end(); ++it){
        TaskStatus* req_status = request.add_task_status();
        req_status->CopyFrom(*it);
    }
    request.set_agent_addr(addr);
    AgentResource resource;
    resource_mgr_->Status(&resource);
    request.set_cpu_share(resource.total_cpu);
    request.set_mem_share(resource.total_mem);
    request.set_used_cpu_share(resource.total_cpu - resource.the_left_cpu);
    request.set_used_mem_share(resource.total_mem - resource.the_left_mem);
    request.set_version(version_);
    LOG(INFO, "Report to master %s,task count %d,"
        "cpu_share %f, cpu_used %f, mem_share %ld, mem_used %ld,version %d",
        addr.c_str(),request.task_status_size(), FLAGS_cpu_num,
        request.used_cpu_share(), FLAGS_mem_bytes, request.used_mem_share(),version_);
    bool ret = rpc_client_->SendRequest(master_, &Master_Stub::HeartBeat,
                                &request, &response, FLAGS_agent_heartbeat_timeout / 1000, 1);
    version_ = response.version();
    int delaytime = FLAGS_agent_heartbeat_interval;
    if (!ret) {
        delaytime -= FLAGS_agent_heartbeat_timeout;
        if (delaytime <= 0) {
            delaytime = 100;     
        }
        LOG(WARNING, "Report to master failed, delay %d ms to heartbeat again",
                delaytime);
    }
    LOG(INFO,"Report response version is %d timeout %ld delaytime %d", 
            version_, FLAGS_agent_heartbeat_timeout, delaytime);
    thread_pool_.DelayTask(delaytime, boost::bind(&AgentImpl::Report, this));
}

std::string AgentImpl::GetPersistenceKey(const TaskInfo& task_info) {
    std::string persistence_key = 
        boost::lexical_cast<std::string>(task_info.task_id());
    return persistence_key;
}

void AgentImpl::RunTask(::google::protobuf::RpcController* /*controller*/,
                        const ::galaxy::RunTaskRequest* request,
                        ::galaxy::RunTaskResponse* response,
                        ::google::protobuf::Closure* done) {
    TaskInfo task_info;
    task_info.set_task_id(request->task_id());
    task_info.set_task_name(request->task_name());
    task_info.set_cmd_line(request->cmd_line());
    task_info.set_task_raw(request->task_raw());
    task_info.set_required_cpu(request->cpu_share());
    task_info.set_required_mem(request->mem_share());
    task_info.set_task_offset(request->task_offset());
    task_info.set_job_replicate_num(request->job_replicate_num());
    task_info.set_job_id(request->job_id());
    task_info.set_monitor_conf(request->monitor_conf()); 
    if (request->has_cpu_limit()) {
        task_info.set_limited_cpu(request->cpu_limit());
    } else {
        task_info.set_limited_cpu(request->cpu_share()); 
    }
    std::string persistence_data;
    if (!task_info.SerializeToString(&persistence_data)
            || persistence_handler_->Write(
                GetPersistenceKey(task_info), 
                persistence_data) != 0) {
        LOG(FATAL, "serialize task info failed"); 
        done->Run();
        return;
    }

    LOG(INFO, "Run Task %s %s [cpu_quota: %lf, cpu_limit: %lf, mem_limit: %ld, monitor_conf:%s]", 
            task_info.task_name().c_str(),
            task_info.cmd_line().c_str(),
            task_info.required_cpu(),
            task_info.limited_cpu(),
            task_info.required_mem(),
            task_info.monitor_conf().c_str());
    TaskResourceRequirement requirement;
    requirement.cpu_limit = request->cpu_share();
    requirement.mem_limit = request->mem_share();
    int ret = resource_mgr_->Allocate(requirement,request->task_id());
    if(ret != 0){
        LOG(FATAL,"fail to allocate resource for task %ld",request->task_id());
        response->set_status(-3);
        done->Run();
        return;
    }
    ret = ws_mgr_->Add(task_info);
    if (ret != 0 ){
        LOG(FATAL,"fail to prepare workspace ");
        response->set_status(-2);
        resource_mgr_->Free(request->task_id());
        done->Run();
        return ;
    }

    LOG(INFO,"start to prepare workspace for %s",request->task_name().c_str());
    DefaultWorkspace * workspace ;
    workspace = ws_mgr_->GetWorkspace(task_info);
    LOG(INFO,"start task for %s",request->task_name().c_str());
    ret = task_mgr_->Add(task_info,workspace);
    if (ret != 0){
        LOG(FATAL,"fail to start task");
        ws_mgr_->Remove(task_info.task_id());
        response->set_status(-1);
        resource_mgr_->Free(request->task_id());
        done->Run();
        return;
    }
    response->set_status(0);
    done->Run();
}

void AgentImpl::KillTask(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::KillTaskRequest* request,
                         ::galaxy::KillTaskResponse* response,
                         ::google::protobuf::Closure* done){
    std::string gc_path;
    LOG(INFO,"kill task %d",request->task_id());
    int status = task_mgr_->Remove(request->task_id());
    LOG(INFO,"kill task %d status %d",request->task_id(),status);
    if (status != 0) {
        response->set_status(status);
        done->Run();
        return;
    }
    status = ws_mgr_->Remove(request->task_id(), &gc_path, true); 
    if (status != 0) {
        LOG(FATAL, "clean workspace failed %d status %d", 
                request->task_id(), status); 
    }
    resource_mgr_->Free(request->task_id());

    TaskInfo task;
    task.set_task_id(request->task_id());
    if (persistence_handler_->Delete(
                GetPersistenceKey(task)) != 0) {
        LOG(FATAL, "delete persistence task %ld data failed",
                request->task_id()); 
        status = -1;
    }

    response->set_status(status);
    response->set_gc_path(gc_path);
    done->Run();
}

void AgentImpl::SetPassword(::google::protobuf::RpcController* controller,
                            const ::galaxy::SetPasswordRequest* request,
                            ::galaxy::SetPasswordResponse* response,
                            ::google::protobuf::Closure* done) {
    (void)controller;
    std::string user_name = request->user_name();
    std::string password = request->password();
    std::string pwd_file_name = FLAGS_pam_pwd_dir + "/" + user_name + ".pwd";
    FILE* fh = fopen(pwd_file_name.c_str(), "w");
    int ret = 0;
    if (fh) {
        fprintf(fh, "%s", password.c_str());
        ret = fclose(fh);
        if (ret == 0) {
            ret = chmod(pwd_file_name.c_str(), 
                        S_IRUSR | S_IWUSR);   //chmod 600
        }
    }
    if (!fh || ret != 0) {
        LOG(WARNING, "write password to %s failed, err:[%d, %s]", 
            pwd_file_name.c_str(),
            errno, strerror(errno));
        response->set_status(-1);
        done->Run();
        return;
    }
    response->set_status(0);
    done->Run();
}

} // namespace galxay

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
