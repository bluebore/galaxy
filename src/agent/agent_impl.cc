// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "agent_impl.h"

#include <boost/bind.hpp>
#include <errno.h>
#include <string.h>
#include "common/util.h"
#include "rpc/rpc_client.h"
#include "proto/task.pb.h"
#include "common/httpserver.h"
#include <gflags/gflags.h>
#include "agent/dynamic_resource_scheduler.h"
#include "agent/downloader_manager.h"
#include "agent/resource_collector_engine.h"

DECLARE_string(master_addr);
DECLARE_string(agent_port);
DECLARE_int32(agent_http_port);
DECLARE_int32(agent_http_server_threads);
DECLARE_string(agent_work_dir);
DECLARE_double(cpu_num);
DECLARE_int64(mem_bytes);
DECLARE_string(pam_pwd_dir);
DECLARE_int32(agent_heartbeat_timeout);
DECLARE_int32(agent_heartbeat_interval);

namespace galaxy {

AgentImpl::AgentImpl() {
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
    thread_pool_.AddTask(boost::bind(&AgentImpl::Report, this));
    http_server_ = new common::HttpFileServer(FLAGS_agent_work_dir,
                                              FLAGS_agent_http_port);
    http_server_->Start(FLAGS_agent_http_server_threads);
}

AgentImpl::~AgentImpl() {
    delete ws_mgr_;
    delete task_mgr_;
    delete resource_mgr_;
    delete http_server_;
}

bool AgentImpl::InitData() {
    if (!task_mgr_->Init()) {
        LOG(FATAL, "task manager init failed");
        return false;
    }
    // should kill task first
    if (!ws_mgr_->Init()) {
        LOG(FATAL, "workspace manager init failed");
        return false;
    }
    return true;
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
    int last_status = COMPLETE;
    std::string gc_path;
    std::vector<TaskStatus > status_vector;
    task_mgr_->Status(status_vector, request->task_id());
    if (status_vector.size() != 1) {
        LOG(WARNING, "what happend not status task id: %ld", request->task_id()); 
    } else {
        last_status = status_vector[0].status();    
    }
    LOG(INFO,"kill task %d",request->task_id());
    int status = task_mgr_->Remove(request->task_id());
    LOG(INFO,"kill task %d status %d",request->task_id(),status);
    if (status != 0) {
        response->set_status(status);
        done->Run();
        return;
    }
    if (last_status == ERROR) {
        status = ws_mgr_->Remove(request->task_id(), &gc_path, true); 
    } else {
        status = ws_mgr_->Remove(request->task_id());
    }
    if (status != 0) {
        LOG(FATAL, "clean workspace failed %d status %d", 
                request->task_id(), status); 
    }
    resource_mgr_->Free(request->task_id());
    response->set_status(status);
    response->set_gc_path(gc_path);
    done->Run();
}

bool AgentImpl::Stop() {
    DownloaderManager* downloader_mgr = 
        DownloaderManager::GetInstance();
    bool ret = downloader_mgr->Stop();
    if (!ret) {
        LOG(WARNING, "downloader mgr stop failed"); 
        return false;
    }

    DynamicResourceScheduler* dy_scheduler =
        GetDynamicResourceScheduler();
    ret = dy_scheduler->Stop();
    if (!ret) {
        LOG(WARNING, "dynamic scheduler stop failed");
        return false;
    }
    ResourceCollectorEngine* engine = 
        GetResourceCollectorEngine();
    ret = engine->Stop();
    if (!ret) {
        LOG(WARNING, "resource collector stop failed");
        return false;
    }
    delete http_server_;
    http_server_ = NULL;
    return thread_pool_.Stop(false); 
}

bool AgentImpl::DumpPersistenceInfo(
        AgentServicePersistence* service_info) {
    if (service_info == NULL) {
        return false; 
    }

    WorkspaceManagerPersistence* wmgr_persistence = 
        service_info->mutable_workspace_manager();
    if (!ws_mgr_->DumpPersistenceInfo(wmgr_persistence)) {
        LOG(WARNING, "[PERSISTENCE] workspace manager dump failed"); 
        return false;
    }

    TaskManagerPersistence* tmgr_persistence = 
        service_info->mutable_task_manager();
    if (!task_mgr_->DumpPersistenceInfo(tmgr_persistence)) {
        LOG(WARNING, "[PERSISTENCE] task manager dump failed"); 
        return false;
    }

    ResourceManagerPersistence* rmgr_persistence =
        service_info->mutable_resource_manager();
    if (!resource_mgr_->DumpPersistenceInfo(rmgr_persistence)) {
        LOG(WARNING, "[PERSISTENCE] resource manager dump failed"); 
        return false;
    }
    service_info->set_workspace_path(workspace_root_path_);

    DynamicSchedulerPersistence* dsr_persistence = 
        service_info->mutable_dynamic_scheduler();
    DynamicResourceScheduler* dy_scheduler 
        = GetDynamicResourceScheduler();
    if (!dy_scheduler->DumpPersistenceInfo(dsr_persistence)) {
        LOG(WARNING, "[PERSISTENCE] dynamic scheduler dump failed"); 
        return false;
    }

    LOG(INFO, "[PERSISTENCE] dump service info success");
    return true;
}

bool AgentImpl::LoadPersistenceInfo(
        const AgentServicePersistence& service_info) {
    if (!service_info.has_workspace_path()
            || !service_info.has_workspace_manager()
            || !service_info.has_task_manager()
            || !service_info.has_resource_manager()
            || !service_info.has_dynamic_scheduler()) {
        return false; 
    }

    workspace_root_path_ = service_info.workspace_path();
    if (!ws_mgr_->LoadPersistenceInfo(
                service_info.workspace_manager())) {
        LOG(WARNING, "[PERSISTENCE] load workspace manager failed"); 
        return false;
    }

    if (!task_mgr_->LoadPersistenceInfo(
                service_info.task_manager(), ws_mgr_)) {
        LOG(WARNING, "[PERSISTENCE] load task manager failed");
        return false;
    }

    if (!resource_mgr_->LoadPersistenceInfo(
                service_info.resource_manager())) {
        LOG(WARNING, "[PERSISTENCE] load resource manager failed"); 
        return false;
    }
     
    DynamicResourceScheduler* dy_scheduler 
        = GetDynamicResourceScheduler();
    if (!dy_scheduler->LoadPersistenceInfo(
                service_info.dynamic_scheduler())) {
        LOG(WARNING, "[PERSISTENCE] load dynamic scheduler failed"); 
        return false;
    }
    LOG(INFO, "[PERSISTENCE] load service info success");
    return true;
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
