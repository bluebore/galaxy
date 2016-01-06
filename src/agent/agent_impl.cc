// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/agent_impl.h"

#include "gflags/gflags.h"
#include "boost/bind.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"
#include "proto/master.pb.h"
#include "logging.h"
#include "agent/agent_internal_infos.h"
#include "utils/trace.h"
#include "agent/utils.h"

DECLARE_string(master_host);
DECLARE_string(master_port);
DECLARE_int32(agent_background_threads_num);
DECLARE_int32(agent_heartbeat_interval);
DECLARE_int32(agent_trace_pod_interval);
DECLARE_string(agent_ip);
DECLARE_string(agent_port);
DECLARE_string(agent_work_dir);

DECLARE_int32(agent_millicores_share);
DECLARE_int64(agent_mem_share);

DECLARE_string(agent_persistence_path);
DECLARE_bool(enable_resource_minitor);
DECLARE_int32(stat_check_period);
DECLARE_double(max_cpu_usage);
DECLARE_double(max_mem_usage);
DECLARE_double(max_disk_r_kbps);
DECLARE_double(max_disk_w_kbps);
DECLARE_double(max_disk_r_rate);
DECLARE_double(max_disk_w_rate);
DECLARE_double(max_disk_util);
DECLARE_double(max_net_in_bps);
DECLARE_double(max_net_out_bps);
DECLARE_double(max_net_in_pps);
DECLARE_double(max_net_out_pps);
DECLARE_double(max_intr_rate);
DECLARE_double(max_soft_intr_rate);
DECLARE_int32(agent_recover_threshold);

namespace baidu {
namespace galaxy {
    
AgentImpl::AgentImpl() : 
    master_endpoint_(),
    lock_(),
    background_threads_(FLAGS_agent_background_threads_num),
    trace_pool_(10),
    rpc_client_(NULL),
    endpoint_(),
    master_(NULL),
    resource_capacity_(),
    master_watcher_(NULL),
    mutex_master_endpoint_(),
    state_(kInit) {
    rpc_client_ = new RpcClient();    
    endpoint_ = FLAGS_agent_ip;
    endpoint_.append(":");
    endpoint_.append(FLAGS_agent_port);
    master_watcher_ = new MasterWatcher();
    recover_threshold_ = 0;
}

AgentImpl::~AgentImpl() {
    background_threads_.Stop(false);
    if (rpc_client_ != NULL) {
        delete rpc_client_; 
        rpc_client_ = NULL;
    }
    delete master_watcher_;
}

void AgentImpl::Query(::google::protobuf::RpcController* /*cntl*/,
                      const ::baidu::galaxy::QueryRequest* /*req*/,
                      ::baidu::galaxy::QueryResponse* resp,
                      ::google::protobuf::Closure* done) {

    MutexLock scope_lock(&lock_);
    resp->set_status(kOk);
    AgentInfo agent_info; 
    agent_info.set_endpoint(endpoint_);
    agent_info.mutable_total()->set_millicores(
            FLAGS_agent_millicores_share);
    agent_info.mutable_total()->set_memory(
            FLAGS_agent_mem_share);
    agent_info.mutable_assigned()->set_millicores(
            FLAGS_agent_millicores_share - resource_capacity_.millicores); 
    agent_info.mutable_assigned()->set_memory(
            FLAGS_agent_mem_share - resource_capacity_.memory);

    boost::unordered_set<int32_t>::iterator port_it = resource_capacity_.used_port.begin();
    for (; port_it != resource_capacity_.used_port.end(); ++port_it) {
        agent_info.mutable_assigned()->add_ports(*port_it);
    } 
    
    int32_t millicores = 0;
    int64_t memory_used = 0;
    std::vector<PodInfo> pods;
    pod_manager_.ShowPods(&pods);
    LOG(INFO, "query pods size %u", pods.size());
    std::vector<PodInfo>::iterator it = pods.begin();
    for (; it != pods.end(); ++it) {
        PodStatus* pod_status = agent_info.add_pods();         
        pod_status->CopyFrom(it->pod_status);
        millicores += pod_status->resource_used().millicores();
        memory_used += pod_status->resource_used().memory();
        LOG(DEBUG, "query pod %s job %s state %s", 
                pod_status->podid().c_str(), 
                pod_status->jobid().c_str(),
                PodState_Name(pod_status->state()).c_str());
    }
    agent_info.mutable_used()->set_millicores(millicores);
    agent_info.mutable_used()->set_memory(memory_used);
    resp->mutable_agent()->CopyFrom(agent_info);
    done->Run();
    return;
}

void AgentImpl::CreatePodInfo(
                    const ::baidu::galaxy::RunPodRequest* req,
                    PodInfo* pod_info) {
    if (pod_info == NULL) {
        return; 
    }
    pod_info->pod_id = req->podid();
    pod_info->job_id = req->jobid();
    pod_info->pod_desc.CopyFrom(req->pod());
    pod_info->pod_status.set_podid(req->podid());
    pod_info->pod_status.set_jobid(req->jobid());
    pod_info->pod_status.set_state(kPodPending);
    pod_info->initd_port = -1;
    pod_info->initd_pid = -1;
    
    for (int i = 0; i < pod_info->pod_desc.tasks_size(); i++) {
        TaskInfo task_info;
        task_info.task_id = GenerateTaskId(pod_info->pod_id);
        task_info.pod_id = pod_info->pod_id;
        task_info.job_id = pod_info->job_id;
        task_info.desc.CopyFrom(pod_info->pod_desc.tasks(i));
        task_info.desc.add_env("POD_VERSION=" + pod_info->pod_desc.version());
        task_info.status.set_state(kTaskPending);
        task_info.initd_endpoint = "";
        task_info.stage = kTaskStagePENDING;
        task_info.fail_retry_times = 0;
        task_info.max_retry_times = 10;
        pod_info->tasks[task_info.task_id] = task_info;
    }
    if (req->has_job_name()) {
        pod_info->job_name = req->job_name();
    }
}

void AgentImpl::RunPod(::google::protobuf::RpcController* /*cntl*/,
                       const ::baidu::galaxy::RunPodRequest* req,
                       ::baidu::galaxy::RunPodResponse* resp,
                       ::google::protobuf::Closure* done) {
    do {
        MutexLock scope_lock(&lock_);
        if (state_ == kOffline) {
            resp->set_status(kAgentError);
            lock_.Unlock();
            OfflineAgentRequest request;
            OfflineAgentResponse response;
            MutexLock lock(&mutex_master_endpoint_);
            request.set_endpoint(endpoint_);
            if (!rpc_client_->SendRequest(master_,
                                          &Master_Stub::OfflineAgent,
                                          &request,
                                          &response,
                                          5, 1)) {
                LOG(WARNING, "send offline request fail");
            }
            break;
        }
        if (!req->has_podid()
                || !req->has_pod()) {
            resp->set_status(kInputError);  
            break;
        }
        std::map<std::string, PodDescriptor>::iterator it = 
            pods_descs_.find(req->podid());
        if (it != pods_descs_.end()) {
            resp->set_status(kOk); 
            break;
        }
        if (AllocResource(req->pod().requirement()) != 0) {
            LOG(WARNING, "pod %s alloc resource failed",
                    req->podid().c_str()); 
            resp->set_status(kQuota);
            break;
        } 
        // NOTE alloc should before pods_desc set
        pods_descs_[req->podid()] = req->pod();

        PodInfo info;
        CreatePodInfo(req, &info);
        // if add failed, clean by master?
        pod_manager_.AddPod(info);
        if (0 != pod_manager_.ShowPod(req->podid(), &info) 
                || !persistence_handler_.SavePodInfo(info)) {
            LOG(WARNING, "pod %s persistence failed",
                         req->podid().c_str());
            // NOTE if persistence failed, need delete pod
            pod_manager_.DeletePod(req->podid());
            resp->set_status(kUnknown);
            break;
        }
        LOG(INFO, "run pod %s", req->podid().c_str());
        resp->set_status(kOk);
        trace_pool_.DelayTask(FLAGS_agent_trace_pod_interval,
                boost::bind(&AgentImpl::CollectPodStat, this, req->podid()));
    } while (0);
    done->Run();
    return;
}

void AgentImpl::KillPod(::google::protobuf::RpcController* /*cntl*/,
                        const ::baidu::galaxy::KillPodRequest* req,
                        ::baidu::galaxy::KillPodResponse* resp,
                        ::google::protobuf::Closure* done) {
    if (!req->has_podid()) {
        LOG(WARNING, "master kill request has no podid"); 
        resp->set_status(kInputError);
        done->Run();
        return;
    }
    MutexLock scope_lock(&lock_);
    std::map<std::string, PodDescriptor>::iterator it = 
        pods_descs_.find(req->podid());
    if (it == pods_descs_.end()) {
        LOG(WARNING, "pod %s not exists", req->podid().c_str());
        resp->set_status(kOk); 
        done->Run();
        return;
    }

    std::string pod_id = req->podid();
    resp->set_status(kOk); 
    done->Run();

    pod_manager_.DeletePod(pod_id);
    LOG(INFO, "pod %s add to delete", pod_id.c_str());
    return;
}

void AgentImpl::KillAllPods() {
    lock_.AssertHeld();
    for (std::map<std::string, PodDescriptor>::iterator it = pods_descs_.begin();
            it != pods_descs_.end(); it++) {
        std::string pod_id = it->first;
        pod_manager_.DeletePod(pod_id);
        LOG(WARNING, "fork to kill %s", pod_id.c_str());
    }
    return;
}

void AgentImpl::KeepHeartBeat() {
    MutexLock lock(&mutex_master_endpoint_);
    if (!PingMaster()) {
        LOG(WARNING, "ping master %s failed",master_endpoint_.c_str());
    }
    background_threads_.DelayTask(FLAGS_agent_heartbeat_interval,
            boost::bind(&AgentImpl::KeepHeartBeat, this));
    return;
}

bool AgentImpl::RestorePods() {
    MutexLock scope_lock(&lock_);
    std::vector<PodInfo> pods;
    if (!persistence_handler_.ScanPodInfo(&pods)) {
        LOG(WARNING, "scan pods failed");
        return false;
    }

    std::vector<PodInfo>::iterator it = pods.begin();
    for (; it != pods.end(); ++it) {
        PodInfo& pod = *it; 
        if (AllocResource(pod.pod_desc.requirement()) != 0) {
            LOG(WARNING, "alloc for pod %s failed require %ld %ld",
                    pod.pod_id.c_str(),
                    pod.pod_desc.requirement().millicores(),
                    pod.pod_desc.requirement().memory());
            return false;
        }
        pods_descs_[pod.pod_id] = pod.pod_desc;

        if (pod_manager_.ReloadPod(pod) != 0) {
            LOG(WARNING, "reload pod %s failed",
                    pod.pod_id.c_str()); 
            return false;
        }
        trace_pool_.DelayTask(FLAGS_agent_trace_pod_interval,
                boost::bind(&AgentImpl::CollectPodStat, this, pod.pod_id));

    }
    return true;
}

bool AgentImpl::Init() {

    resource_capacity_.millicores = FLAGS_agent_millicores_share;
    resource_capacity_.memory = FLAGS_agent_mem_share; 
    if (!file::Mkdir(FLAGS_agent_work_dir)) {
        LOG(WARNING, "mkdir workdir %s falied", 
                            FLAGS_agent_work_dir.c_str()); 
        return false;
    }

    if (!persistence_handler_.Init(FLAGS_agent_persistence_path)) {
        LOG(WARNING, "init persistence handler failed");
        return false;  
    }

    if (pod_manager_.Init() != 0) {
        LOG(WARNING, "init pod manager failed");
        return false; 
    }

    if (!RestorePods()) {
        LOG(WARNING, "restore pods failed"); 
        return false;
    }

    if (!RegistToMaster()) {
        return false; 
    }
    background_threads_.DelayTask(
            FLAGS_agent_heartbeat_interval, boost::bind(&AgentImpl::KeepHeartBeat, this));

    background_threads_.DelayTask(
                500, 
                boost::bind(&AgentImpl::LoopCheckPods, this)); 
    if (FLAGS_enable_resource_minitor) {
        background_threads_.AddTask(boost::bind(&AgentImpl::CheckSysHealth, this));
    }
    return true;
}

bool AgentImpl::PingMaster() {
    mutex_master_endpoint_.AssertHeld();
    HeartBeatRequest request;
    HeartBeatResponse response;
    request.set_endpoint(endpoint_);
    LOG(WARNING, "agent %s ping master : %s", endpoint_.c_str(), master_endpoint_.c_str());
    return rpc_client_->SendRequest(master_,
                                    &Master_Stub::HeartBeat,
                                    &request,
                                    &response,
                                    5, 1);    
}

void AgentImpl::LoopCheckPods() {
    MutexLock scope_lock(&lock_);
    std::map<std::string, PodDescriptor>::iterator it = 
        pods_descs_.begin();
    std::vector<std::string> to_del_pod;
    for (; it != pods_descs_.end(); ++it) {
        if (pod_manager_.CheckPod(it->first) != 0) {
            if (!persistence_handler_.DeletePodInfo(it->first)) {
                LOG(WARNING, "delete persistence pod %s failed",
                        it->first.c_str());
                continue;
            }
            to_del_pod.push_back(it->first);
            ReleaseResource(it->second.requirement()); 
        }  
    }
    for (size_t i = 0; i < to_del_pod.size(); i++) {
        pods_descs_.erase(to_del_pod[i]); 
    }
    background_threads_.DelayTask(
                100, 
                boost::bind(&AgentImpl::LoopCheckPods, this)); 
}

void AgentImpl::HandleMasterChange(const std::string& new_master_endpoint) {
    if (new_master_endpoint.empty()) {
        LOG(WARNING, "the master endpoint is deleted from nexus");
    }
    if (new_master_endpoint != master_endpoint_) {
        MutexLock lock(&mutex_master_endpoint_);
        LOG(INFO, "master change to %s", new_master_endpoint.c_str());
        master_endpoint_ = new_master_endpoint; 
        if (master_) {
            delete master_;
        }
        if (!rpc_client_->GetStub(master_endpoint_, &master_)) {
            LOG(WARNING, "connect master %s failed", master_endpoint_.c_str()); 
            return;
        }  
    }
}

bool AgentImpl::RegistToMaster() {
    boost::function<void(const std::string)> handler;
    handler = boost::bind(&AgentImpl::HandleMasterChange, this, _1);
    bool ok = master_watcher_->Init(handler);
    if (!ok) {
        LOG(WARNING, "fail to watch on nexus");
        return false;
    }
    MutexLock lock(&mutex_master_endpoint_);
    
    master_endpoint_ = master_watcher_->GetMasterEndpoint();

    if (!rpc_client_->GetStub(master_endpoint_, &master_)) {
        LOG(WARNING, "connect master %s failed", master_endpoint_.c_str()); 
        return false;
    }

    if (!PingMaster()) {
        LOG(WARNING, "connect master %s failed", master_endpoint_.c_str()); 
    }
    LOG(INFO, "regist to master %s", master_endpoint_.c_str());
    return true;
}

int AgentImpl::AllocResource(const Resource& requirement) {
    lock_.AssertHeld();
    if (resource_capacity_.millicores >= requirement.millicores()
            && resource_capacity_.memory >= requirement.memory()) {
        boost::unordered_set<int32_t> ports;
        for (int i = 0; i < requirement.ports_size(); i++) {
            int32_t port = requirement.ports(i);
            if (resource_capacity_.used_port.find(port) 
                    != resource_capacity_.used_port.end()) {
                return -1; 
            }
            ports.insert(port);
        }
        boost::unordered_set<int32_t>::iterator it = ports.begin();
        for (; it != ports.end(); ++it) {
            resource_capacity_.used_port.insert(*it);
        }
        resource_capacity_.millicores -= requirement.millicores(); 
        resource_capacity_.memory -= requirement.memory();

        return 0;
    }
    return -1;
}

void AgentImpl::ReleaseResource(const Resource& requirement) {
    lock_.AssertHeld();

    resource_capacity_.millicores += requirement.millicores();
    resource_capacity_.memory += requirement.memory();
    for (int i = 0; i < requirement.ports_size(); i++) {
        resource_capacity_.used_port.erase(requirement.ports(i));
    }
}

void AgentImpl::ConvertToPodPropertiy(const PodInfo& show_pod_info, PodPropertiy* pod_propertiy) {
    pod_propertiy->set_pod_id(show_pod_info.pod_id);
    pod_propertiy->set_job_id(show_pod_info.job_id);
    pod_propertiy->mutable_pod_desc()->CopyFrom(
                            show_pod_info.pod_desc);
    pod_propertiy->mutable_pod_status()->CopyFrom(
                            show_pod_info.pod_status);
    std::string initd_endpoint = "127.0.0.1:";
    initd_endpoint.append(
            boost::lexical_cast<std::string>(
                    show_pod_info.initd_port));
    pod_propertiy->set_initd_endpoint(initd_endpoint);
    pod_propertiy->set_pod_path(show_pod_info.pod_path);
    pod_propertiy->set_job_name(show_pod_info.job_name);
    return;
}

void AgentImpl::CollectPodStat(const std::string& podid) {
    MutexLock scope_lock(&lock_);
    PodInfo pod_info;
    int ok = pod_manager_.ShowPod(podid, &pod_info);
    if (ok != 0) {
        LOG(WARNING, "pod %s does not exist", podid.c_str());
        return;
    }
    LOG(INFO, "trace pod %s", podid.c_str());
    Trace::TracePodStat(&pod_info.pod_status,
                        pod_info.pod_desc.requirement().millicores(),
                        pod_info.pod_desc.requirement().memory());
    trace_pool_.DelayTask(FLAGS_agent_trace_pod_interval,
            boost::bind(&AgentImpl::CollectPodStat, this, podid));
}

void AgentImpl::ShowPods(::google::protobuf::RpcController* /*cntl*/,
                         const ::baidu::galaxy::ShowPodsRequest* req,
                         ::baidu::galaxy::ShowPodsResponse* resp,
                         ::google::protobuf::Closure* done) {
    MutexLock scope_lock(&lock_);
    resp->set_status(kOk);
    do {
        if (req->has_podid()) {
            PodInfo show_pod_info;          
            if (0 != 
                    pod_manager_.ShowPod(req->podid(), 
                                         &show_pod_info)) { 
                LOG(WARNING, "pod %s not exists", 
                             req->podid().c_str());
                resp->set_status(kUnknown);
                break;
            }
            PodPropertiy* pod_propertiy = resp->add_pods();
            ConvertToPodPropertiy(show_pod_info, 
                                  pod_propertiy);
            break;
        }     
        std::vector<PodInfo> pods; 
        pod_manager_.ShowPods(&pods);
        std::vector<PodInfo>::iterator it = pods.begin();
        for (; it != pods.end(); ++it) {
            ConvertToPodPropertiy(*it, resp->add_pods());            
        }
        LOG(INFO, "pods show size %d", resp->pods_size());
    } while (0);
    done->Run();    
    return;
}

void AgentImpl::CheckSysHealth() {
    bool ret = true;
    int coll_rlt = resource_collector_.CollectStatistics();
    if (coll_rlt == -1) {
        LOG(WARNING, "Collect sys stat fail.");
        ret = false;
    } else if (coll_rlt == 1)  {
        background_threads_.DelayTask(FLAGS_stat_check_period, boost::bind(&AgentImpl::CheckSysHealth, this));
        return;
    } else if (fabs(FLAGS_max_cpu_usage) >= 1e-6 
            && resource_collector_.GetStat()->cpu_used_ > FLAGS_max_cpu_usage) {
        LOG(WARNING, "cpu uage %f reach threshold %f",
                resource_collector_.GetStat()->cpu_used_, FLAGS_max_cpu_usage);
        ret = false;
    } else if (fabs(FLAGS_max_mem_usage) >= 1e-6 
            && resource_collector_.GetStat()->mem_used_ > FLAGS_max_mem_usage) {
        LOG(WARNING, "mem usage %f reach threshold %f",
                resource_collector_.GetStat()->mem_used_, FLAGS_max_mem_usage);
        ret = false;
    } else if (fabs(FLAGS_max_disk_r_kbps) >= 1e-6
            && resource_collector_.GetStat()->disk_read_Bps_ > FLAGS_max_disk_r_kbps) {
        LOG(WARNING, "disk read Bps %f reach threshold %f",
                resource_collector_.GetStat()->disk_read_Bps_, FLAGS_max_disk_r_kbps);
        ret = false;
    } else if (fabs(FLAGS_max_disk_w_kbps) >= 1e-6 
            && resource_collector_.GetStat()->disk_write_Bps_ > FLAGS_max_disk_w_kbps) {
        LOG(WARNING, "disk write Bps %f reach threshold %f",
                resource_collector_.GetStat()->disk_write_Bps_, FLAGS_max_disk_w_kbps);
        ret = false;
    } else if (fabs(FLAGS_max_disk_r_rate) >= 1e-6
            && resource_collector_.GetStat()->disk_read_times_ > FLAGS_max_disk_r_rate) {
        LOG(WARNING, "disk write rate %f reach threshold %f",
                resource_collector_.GetStat()->disk_read_times_, FLAGS_max_disk_r_rate);
        ret = false;
    } else if (fabs(FLAGS_max_disk_w_rate) >= 1e-6 &&
            resource_collector_.GetStat()->disk_write_times_ > FLAGS_max_disk_w_rate) {
        LOG(WARNING, "disk write rate %f reach threshold %f", 
                resource_collector_.GetStat()->disk_write_times_, FLAGS_max_disk_w_rate);
        ret = false;
    } else if (fabs(FLAGS_max_disk_util) >= 1e-6
            && resource_collector_.GetStat()->disk_io_util_ > FLAGS_max_disk_util) {
        LOG(WARNING, "disk io util %f reach threshold %f", 
                resource_collector_.GetStat()->disk_io_util_, FLAGS_max_disk_util);
        ret = false;
    } else if (fabs(FLAGS_max_net_in_bps) >= 1e-6
            && resource_collector_.GetStat()->net_in_bps_ > FLAGS_max_net_in_bps) {
        LOG(WARNING, "net in bps %f reach threshold %f",
                resource_collector_.GetStat()->net_in_bps_, FLAGS_max_net_in_bps);
        ret = false;
    } else if (fabs(FLAGS_max_net_out_bps) >= 1e-6
            && resource_collector_.GetStat()->net_out_bps_ > FLAGS_max_net_out_bps) {
        LOG(WARNING, "net out bps %f reach threshold %f", 
                resource_collector_.GetStat()->net_out_bps_, FLAGS_max_net_out_bps);
        ret = false;
    } else if (fabs(FLAGS_max_net_in_pps) >= 1e-6 
            && resource_collector_.GetStat()->net_in_pps_ > FLAGS_max_net_in_pps) {
        LOG(WARNING, "net in pps %f reach threshold %f", 
                resource_collector_.GetStat()->net_in_bps_, FLAGS_max_net_in_pps);
        ret = false;
    } else if (fabs(FLAGS_max_net_out_pps) >= 1e-6 &&
            resource_collector_.GetStat()->net_out_pps_ > FLAGS_max_net_out_pps) {
        LOG(WARNING, "net out pps %f reach threshold %f",
                resource_collector_.GetStat()->net_out_pps_, FLAGS_max_net_out_pps);
        ret = false;
    } else if (fabs(FLAGS_max_intr_rate) >= 1e-6  && 
            resource_collector_.GetStat()->intr_rate_ > FLAGS_max_intr_rate) {
        LOG(WARNING, "interupt rate %f reach threshold %f", 
                resource_collector_.GetStat()->intr_rate_, FLAGS_max_intr_rate);
        ret = false;
    } else if (fabs(FLAGS_max_soft_intr_rate) >= 1e-6 &&
            resource_collector_.GetStat()->soft_intr_rate_ > FLAGS_max_soft_intr_rate) {
        LOG(WARNING, "soft interupt rate %f reach threshold %f",
                resource_collector_.GetStat()->soft_intr_rate_, FLAGS_max_soft_intr_rate);
        ret = false;
    }
    recover_threshold_++;
    if (ret) {
        if ((recover_threshold_ > FLAGS_agent_recover_threshold 
                && state_ == kOffline) || state_ == kInit) {
            OnlineAgentRequest request;
            OnlineAgentResponse response;
            MutexLock lock(&mutex_master_endpoint_);
            request.set_endpoint(endpoint_);
            if (!rpc_client_->SendRequest(master_,
                                          &Master_Stub::OnlineAgent,
                                          &request,
                                          &response,
                                          5, 1)) {
                LOG(WARNING, "send online request fail");
            } else {
                mutex_master_endpoint_.Unlock();
                MutexLock scope_lock(&lock_);
                state_ = kAlive;
                LOG(WARNING, "agent state Alive.");
            }
        }
    } else {
        recover_threshold_ = 0;
        if (state_ != kOffline) {
            MutexLock scope_lock(&lock_);
            state_ = kOffline;
            KillAllPods();
            LOG(WARNING, "agent state offline, sys health checker kill all pods");
            lock_.Unlock();
            OfflineAgentRequest request;
            OfflineAgentResponse response;
            MutexLock lock(&mutex_master_endpoint_);
            request.set_endpoint(endpoint_);
            if (!rpc_client_->SendRequest(master_,
                                         &Master_Stub::OfflineAgent,
                                         &request,
                                         &response,
                                         5, 1)) {
                LOG(WARNING, "send offline request fail");
            }
        }
    }
    background_threads_.DelayTask(FLAGS_stat_check_period, boost::bind(&AgentImpl::CheckSysHealth, this));
}

}   // ending namespace galaxy
}   // ending namespace baidu
