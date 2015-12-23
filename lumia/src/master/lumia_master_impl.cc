// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "master/lumia_master_impl.h"

#include "master/lumia_master_util.h"
#include "proto/agent.pb.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <unistd.h>
#include <gflags/gflags.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include "logging.h"
#include <fstream>
#include <sstream>
#include <dirent.h>

DECLARE_string(rms_api_http_host);
DECLARE_string(ccs_api_http_host);
DECLARE_string(nexus_servers);
DECLARE_string(lumia_lock);
DECLARE_string(lumia_ctrl_port);
DECLARE_string(lumia_main);
DECLARE_string(lumia_minion);
DECLARE_string(lumia_script);
DECLARE_string(minion_dict);
DECLARE_string(scripts_dir);
DECLARE_string(lumia_root_path);

namespace baidu {
namespace lumia {

const static int64_t MEM_RESERVED = 6442450944;
const static int32_t CPU_IDLE = 700;

static void OnLumiaSessionTimeout(void* ctx) {
    LumiaMasterImpl* lumia = static_cast<LumiaMasterImpl*>(ctx);
    lumia->OnSessionTimeout();
}

static void OnLumiaLockChange(const ::galaxy::ins::sdk::WatchParam& param,
  ::galaxy::ins::sdk::SDKError) {
    LumiaMasterImpl* lumia = static_cast<LumiaMasterImpl*>(param.context);
    lumia->OnLockChange(param.value);
}

LumiaMasterImpl::LumiaMasterImpl():workers_(4){
    minion_ctrl_ = new MinionCtrl(FLAGS_ccs_api_http_host,
                                  FLAGS_rms_api_http_host);
    nexus_ = new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_servers);
    rpc_client_ = new ::baidu::galaxy::RpcClient();
    query_node_count_ = 0;
    job_under_working_ = new std::set<std::string>();
    job_completed_ = new std::set<std::string>();
}

void LumiaMasterImpl::OnLockChange(const std::string& sessionid) {
    std::string self_sessionid = nexus_->GetSessionID();
    if (self_sessionid != sessionid) {
        LOG(WARNING, "lumia lost lock");
        abort();
    }
}


LumiaMasterImpl::~LumiaMasterImpl() {
    delete minion_ctrl_;
    delete nexus_;
}

void LumiaMasterImpl::Init() {
    AcquireLumiaLock(); 
}
void LumiaMasterImpl::OnSessionTimeout() {
    LOG(WARNING, "time out with nexus");
    abort();
}

void LumiaMasterImpl::AcquireLumiaLock() {
    std::string lock = FLAGS_lumia_root_path + FLAGS_lumia_lock;
    ::galaxy::ins::sdk::SDKError err;
    nexus_->RegisterSessionTimeout(&OnLumiaSessionTimeout, this);
    bool ret = nexus_->Lock(lock, &err); //whould block until accquired
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    std::string lumia_endpoint = LumiaMasterUtil::GetHostName() + ":" + FLAGS_lumia_ctrl_port;
    std::string lumia_key = FLAGS_lumia_root_path + FLAGS_lumia_main;
    ret = nexus_->Put(lumia_key, lumia_endpoint, &err);
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    ret = nexus_->Watch(lock, &OnLumiaLockChange, this, &err);
    assert(ret && err == ::galaxy::ins::sdk::kOK);
    LOG(INFO, "master lock [ok].  %s -> %s", 
        lumia_key.c_str(), lumia_endpoint.c_str());

    std::string minion_start_key = FLAGS_lumia_root_path + FLAGS_lumia_minion +"/0";
    std::string minion_end_key = FLAGS_lumia_root_path + FLAGS_lumia_minion +"/~~~~~~~~~";
    // TODO delete minions
    ::galaxy::ins::sdk::ScanResult* minions = nexus_->Scan(minion_start_key, minion_end_key);
    if (minions->Error() != ::galaxy::ins::sdk::kOK) {
        LOG(FATAL, "fail to load minions from nexus");
    }
    while (!minions->Done()) {
        Minion* m = new Minion();
        ret = m->ParseFromString(minions->Value());
        if (!ret) {
            LOG(WARNING, "fail to parse %s", minions->Key().c_str());
            minions->Next();
        } 
        MinionIndex m_index(m->id(), m->hostname(), m->ip(), m);
        minion_set_.insert(m_index);
        LOG(INFO, "load minion %s", m->hostname().c_str());
        minions->Next();
    }
    std::string script_start_key = FLAGS_lumia_root_path + FLAGS_lumia_script  + "/";
    std::string script_end_key = FLAGS_lumia_root_path + FLAGS_lumia_script + "/~~~~~~~~~";
    ::galaxy::ins::sdk::ScanResult* scripts = nexus_->Scan(script_start_key, script_end_key);
    if (scripts->Error() != ::galaxy::ins::sdk::kOK) {
        LOG(FATAL, "fail to load scripts from nexus");
    }
    while (!scripts->Done()) {
        SystemScript script;
        script.ParseFromString(scripts->Value());
        scripts_[script.name()] = script.content();
        LOG(INFO, "load script %s", script.name().c_str());
        scripts->Next();
    }
    ScheduleNextQuery();

}

void LumiaMasterImpl::GetOverview(::google::protobuf::RpcController*,
                    const ::baidu::lumia::GetOverviewRequest*,
                    ::baidu::lumia::GetOverviewResponse* response,
                    ::google::protobuf::Closure* done) {
    MutexLock lock(&mutex_);
    const minion_set_id_index_t& id_index = boost::multi_index::get<id_tag>(minion_set_);
    minion_set_id_index_t::const_iterator it = id_index.begin();
    int32_t count = 0;
    for (; it != id_index.end(); ++it) {
        const MinionStatus& status = it->minion_->status();
        if (status.devices_size() <= 0) {
            continue;
        }
        MinionOverview* view = response->add_minions();
        view->set_hostname(it->minion_->hostname());
        view->set_ip(it->minion_->ip());
        bool mount_ok = true;
        bool device_ok = true;
        for (int i = 0; i < status.devices_size(); i++) {
            if (status.devices(i).healthy()) {
                continue;
            }
            device_ok = false;
            break;
        }
        for (int i = 0;  i < status.mounts_size(); i++) {
            if (status.mounts(i).mounted()) {
                continue;
            }
            if (status.mounts(i).mount_point().find("/noah") != std::string::npos) {
                continue;
            }
            mount_ok = false;
            break;
        }
        view->set_mount_ok(mount_ok);
        view->set_device_ok(device_ok);
        view->set_datetime(status.datetime());
        count++;
    }
    done->Run();
    LOG(INFO, "get overview  count %d", count);
}

void LumiaMasterImpl::ScheduleNextQuery() {
    workers_.DelayTask(30000, boost::bind(&LumiaMasterImpl::LaunchQuery, this));
}

void LumiaMasterImpl::LaunchQuery() {
    MutexLock lock(&mutex_);
    if (query_node_count_ != 0) {
        LOG(WARNING, "invalide query node count number %d", query_node_count_);
        return;
    }
    std::set<std::string>::iterator it = live_nodes_.begin();
    for (; it != live_nodes_.end(); ++it) {
        QueryNode(*it);
    }
    if (query_node_count_ == 0) {
        ScheduleNextQuery();
    }
}

void LumiaMasterImpl::QueryNode(const std::string& node_addr) {
    mutex_.AssertHeld();
    LOG(INFO, "start a query on node %s", node_addr.c_str());
    QueryRequest* request = new QueryRequest();
    QueryResponse* response = new QueryResponse();
    LumiaLet_Stub* agent = NULL;
    rpc_client_->GetStub(node_addr, &agent);
    boost::function<void (const QueryRequest*, QueryResponse*, bool, int)> query_callback; 
    query_callback = boost::bind(&LumiaMasterImpl::QueryCallBack, this, _1, _2, _3, _4, node_addr);
    rpc_client_->AsyncRequest(agent, &LumiaLet_Stub::Query,
                              request, response, query_callback, 5, 0);
    query_node_count_++;
    delete agent;
}

void LumiaMasterImpl::QueryCallBack(const QueryRequest* request,
                                  QueryResponse* response,
                                  bool fails, int , 
                                  const std::string& node_addr) {
    boost::scoped_ptr<const QueryRequest> request_ptr(request);
    boost::scoped_ptr<QueryResponse> response_ptr(response);
   
    MutexLock lock(&mutex_);
    if (--query_node_count_ == 0) {
        ScheduleNextQuery();
    }
    if (fails || response->status() != 0) {
        LOG(WARNING, "fail to query node %s", node_addr.c_str());
        return;
    }
    const minion_set_ip_index_t& ip_index = boost::multi_index::get<ip_tag>(minion_set_);
    minion_set_ip_index_t::const_iterator it = ip_index.find(response->ip());
    if (it == ip_index.end()) {
        LOG(WARNING, "host %s meta does not in lumia", response->ip().c_str());
        return;
    }
    it->minion_->mutable_status()->CopyFrom(response->minion_status());
    LOG(INFO, "update minion %s status successfully statu %d", response->ip().c_str(),
       response->minion_status().all_is_well());
}

void LumiaMasterImpl::ImportData(::google::protobuf::RpcController* ,
                    const ::baidu::lumia::ImportDataRequest* request,
                    ::baidu::lumia::ImportDataResponse* response,
                    ::google::protobuf::Closure* done) {
    MutexLock lock(&mutex_);
    ::galaxy::ins::sdk::SDKError err;
    const minion_set_id_index_t& index = boost::multi_index::get<id_tag>(minion_set_);
    for (int i = 0; i < request->minions().minions_size(); i++) {
        const Minion& minion = request->minions().minions(i);
        std::string minion_value;
        bool ok = minion.SerializeToString(&minion_value);
        if (!ok) {
           LOG(WARNING, "fail to serialize minion %s to string", minion.hostname().c_str());
           continue;
        }
        std::string minion_key = FLAGS_lumia_root_path + FLAGS_lumia_minion + "/" + minion.id();
        ok = nexus_->Put(minion_key, minion_value, &err);
        if (!ok || err != ::galaxy::ins::sdk::kOK) {
           LOG(WARNING, "fail to put minion to nexus %s", minion.hostname().c_str());
           continue;
        }
        minion_set_id_index_t::const_iterator it = index.find(minion.id());
        if (it == index.end()) {
            Minion* m = new Minion();
            m->CopyFrom(minion);
            m->set_state(kMinionAlive);
            MinionIndex m_index(m->id(), m->hostname(), m->ip(), m);
            minion_set_.insert(m_index);
            LOG(INFO, "insert new minion %s", m->hostname().c_str());
        }else {
            MinionState state = it->minion_->state();
            it->minion_->CopyFrom(minion);
            it->minion_->set_state(state);
            LOG(INFO, "update minion %s", minion.hostname().c_str());
        }
    }

    for (int i = 0; i < request->scripts_size(); i++) {
        LOG(INFO, "load script with name %s", request->scripts(i).name().c_str());
        std::string script_key = FLAGS_lumia_root_path + FLAGS_lumia_script + "/" + request->scripts(i).name();
        std::string script_value;
        bool ok = request->scripts(i).SerializeToString(&script_value);
        if (!ok) {
            LOG(WARNING, "fail to serialize script %s", script_key.c_str());
            continue;
        }
        ok = nexus_->Put(script_key, script_value, &err);
        if (!ok || err != ::galaxy::ins::sdk::kOK ) {
           LOG(WARNING, "fail to save script %s to nexus", request->scripts(i).name().c_str());
           continue;
        }
        scripts_[request->scripts(i).name()] = request->scripts(i).content();
    }
    response->set_status(kLumiaOk);
    done->Run();
}

void LumiaMasterImpl::Ping(::google::protobuf::RpcController*,
                         const ::baidu::lumia::PingRequest* request,
                         ::baidu::lumia::PingResponse* ,
                         ::google::protobuf::Closure* done) {
    {
        MutexLock lock(&mutex_);
        live_nodes_.insert(request->node_addr());
        dead_nodes_.erase(request->node_addr());
    }
    MutexLock lock(&timer_mutex_);
    std::map<std::string, int64_t>::iterator it = node_timers_.find(request->node_addr());
    if (it != node_timers_.end()) {
        bool ok = dead_checkers_.CancelTask(it->second);
        if (!ok) {
            LOG(WARNING, "some stranges happend to %s ", request->node_addr().c_str());
        }
    }else {
        LOG(INFO, "new node %s join", request->node_addr().c_str());
    }
    int64_t id = dead_checkers_.DelayTask(10000, boost::bind(&LumiaMasterImpl::HandleNodeOffline, this, request->node_addr()));
    node_timers_[request->node_addr()] = id;
    done->Run();
}

void LumiaMasterImpl::HandleNodeOffline(const std::string& node_addr) {
    MutexLock lock(&mutex_);
    live_nodes_.erase(node_addr);
    dead_nodes_.insert(node_addr);
}

void LumiaMasterImpl::ReportDeadMinion(::google::protobuf::RpcController* ,
                          const ::baidu::lumia::ReportDeadMinionRequest* request,
                          ::baidu::lumia::ReportDeadMinionResponse* response,
                          ::google::protobuf::Closure* done) {
    MutexLock lock(&mutex_);
    LOG(INFO, "report dead minion %s for %s", request->ip().c_str(), request->reason().c_str());
    const minion_set_ip_index_t& index = boost::multi_index::get<ip_tag>(minion_set_);
    minion_set_ip_index_t::const_iterator i_it = index.find(request->ip());

    if (i_it == index.end()) {
        LOG(WARNING, "minion with ip %s is not found", request->ip().c_str());
        response->set_status(kLumiaMinionNotFound);
        done->Run();
        return;
    }

    if (i_it->minion_->state() != kMinionAlive) {
        LOG(WARNING, "minion with state %s does not accept dead check", MinionState_Name(i_it->minion_->state()).c_str());
        response->set_status(kLumiaMinionInProcess);
        done->Run();
        return;
    }
    std::map<std::string, std::string>::iterator sc_it = scripts_.find("minion-dead-check.sh");
    if (sc_it == scripts_.end()) {
        LOG(WARNING, "minion-dead-check.sh is not found");
        response->set_status(kLumiaScriptNotFound);
        done->Run();
        return;
    }
    i_it->minion_->set_state(kMinionDeadChecking);
    workers_.AddTask(boost::bind(&LumiaMasterImpl::HandleDeadReport, this, request->ip()));
    response->set_status(kLumiaOk);
    done->Run();
}

void LumiaMasterImpl::HandleDeadReport(const std::string& ip) {
    MutexLock lock(&mutex_);
    const minion_set_ip_index_t& index = boost::multi_index::get<ip_tag>(minion_set_);
    minion_set_ip_index_t::const_iterator i_it = index.find(ip);
    std::map<std::string, std::string>::iterator sc_it = scripts_.find("minion-dead-check.sh");
    std::vector<std::string> hosts;
    hosts.push_back(i_it->hostname_);
    std::string sessionid;
    bool ok = minion_ctrl_->ExecOnNoah(sc_it->second, 
                                hosts,
                                "root",
                                1,
                                &sessionid,
                                boost::bind(&LumiaMasterImpl::CheckDeadCallBack, this, _1, _2, _3));
    if (!ok) {
        LOG(WARNING, "fail to run dead check script on minion %s", i_it->hostname_.c_str());
        return;
    }
}


void LumiaMasterImpl::CheckDeadCallBack(const std::string sessionid,
                                      const std::vector<std::string> success,
                                      const std::vector<std::string> fails) {
    LOG(INFO, "dead check with session %s  callback success host %s, fails %s",
             sessionid.c_str(),
             boost::algorithm::join(success, ",").c_str(),
             boost::algorithm::join(fails, ",").c_str());
    const minion_set_hostname_index_t& h_index = boost::multi_index::get<hostname_tag>(minion_set_);
    std::vector<std::string> fail_ids;
    for (size_t i = 0; i < fails.size(); i++) {
        minion_set_hostname_index_t::const_iterator it = h_index.find(fails[i]);
        if (it == h_index.end()) {
            LOG(WARNING, "%s has no rms id", fails[i].c_str());
            continue;
        }
        fail_ids.push_back(it->id_);
        it->minion_->set_state(kMinionRestarting);
    }
    std::string reboot_sessionid;
    if (fail_ids.size() > 0) {
        bool ok = minion_ctrl_->Reboot(fail_ids,
                                   boost::bind(&LumiaMasterImpl::RebootCallBack, this, _1, _2, _3), &reboot_sessionid);
        if (!ok) {
            LOG(WARNING, "fail to submit reboot to rms with dead check session %s", sessionid.c_str());
            return;
        } 
    }  
    if (success.size() > 0) {
        workers_.AddTask(boost::bind(&LumiaMasterImpl::HandleInitAgent, this, success));
    }
    
}

void LumiaMasterImpl::HandleInitAgent(const std::vector<std::string> hosts) {
    MutexLock lock(&mutex_);
    std::map<std::string, std::vector<std::string> > hosts_map;
    const minion_set_hostname_index_t& index = boost::multi_index::get<hostname_tag>(minion_set_);
    std::map<std::string, std::string>::iterator sc_it = scripts_.find("deploy-galaxy-agent.tpl.sh");
    if (sc_it == scripts_.end()) {
        LOG(WARNING, "deploy-galaxy-agent.tpl.sh does not exist in lumia");
        return;
    }
    for (size_t i = 0; i < hosts.size(); i++) {
        minion_set_hostname_index_t::const_iterator it = index.find(hosts[i]);
        if (it == index.end()) {
            LOG(INFO, "host %s does not exit in lumia", it->hostname_.c_str());
            continue;
        }
        // TODO need better strategy 
        int64_t mem_share = it->minion_->mem().count() * it->minion_->mem().size() - MEM_RESERVED;
        int32_t cpu_share = it->minion_->cpu().count() * CPU_IDLE; 
        it->minion_->set_state(kMinionIniting);
        LOG(INFO, "start init minion with cpu share %d, mem share %ld", cpu_share, mem_share);
        std::string deploy_script = sc_it->second;
        std::string cpu_share_sc = "sed -i 's/--agent_millicores_share=.*/--agent_millicores_share=" + boost::lexical_cast<std::string>(cpu_share) + "/' conf/galaxy.flag\n";
        std::string mem_share_sc = "sed -i 's/--agent_mem_share=.*/--agent_mem_share=" + boost::lexical_cast<std::string>(mem_share) + "/' conf/galaxy.flag\n";
        deploy_script.append(cpu_share_sc);
        deploy_script.append(mem_share_sc);
        deploy_script.append("babysitter bin/galaxy-agent.conf stop >/dev/null 2>&1\n");
        deploy_script.append("sleep 2\n");
        deploy_script.append("babysitter bin/galaxy-agent.conf start\n");
        std::vector<std::string>& hosts = hosts_map[deploy_script]; 
        hosts.push_back(it->hostname_);
    }
    std::map<std::string, std::vector<std::string> >::iterator it = hosts_map.begin();
    for (; it != hosts_map.end(); it++) {
        if (it->second.size() <= 0) {
            continue;
        }
        DoInitAgent(it->second, it->first);
    }
}

bool LumiaMasterImpl::DoInitAgent(const std::vector<std::string> hosts,
                                const std::string scripts) {
    std::string sessionid;
    bool ok = minion_ctrl_->ExecOnNoah(scripts, 
                                hosts,
                                "root",
                                10,
                                &sessionid,
                                boost::bind(&LumiaMasterImpl::InitAgentCallBack, this, _1, _2, _3));
    if (!ok) {
        LOG(WARNING, "fail to init agents %s", boost::algorithm::join(hosts, ",").c_str());
        return false;
    }
    LOG(INFO, "submit init cmd to agents %s successfully", boost::algorithm::join(hosts, ",").c_str());
    return true;
}

void LumiaMasterImpl::InitAgentCallBack(const std::string sessionid,
                                      const std::vector<std::string> success,
                                      const std::vector<std::string> fails) {
    MutexLock lock(&mutex_);
    LOG(INFO, "init agent call back succ %s, fails %s", boost::algorithm::join(success, ",").c_str(),
      boost::algorithm::join(fails, ",").c_str()); 
    const minion_set_hostname_index_t& index = boost::multi_index::get<hostname_tag>(minion_set_);
    for (size_t i = 0; i < success.size(); i++) {
        minion_set_hostname_index_t::const_iterator it = index.find(success[i]);
        if (it == index.end()) {
            LOG(WARNING, "agent with hostname %s does not exist in lumia", success[i].c_str());
            continue;
        }
        // galaxy agent alive means agent alive
        it->minion_->set_state(kMinionAlive);
    }
}

void LumiaMasterImpl::GetStatus(::google::protobuf::RpcController* controller,
                    const ::baidu::lumia::GetStatusRequest* request,
                    ::baidu::lumia::GetStatusResponse* response,
                    ::google::protobuf::Closure* done) {
    MutexLock lock(&mutex_);
    std::set<std::string>::iterator live_node_it =  live_nodes_.begin();
    for (; live_node_it != live_nodes_.end(); ++live_node_it) {
        response->add_live_nodes(*live_node_it);
    }
    std::set<std::string>::iterator dead_node_it = dead_nodes_.begin();
    for (; dead_node_it != dead_nodes_.end(); ++dead_node_it) {
        response->add_dead_nodes(*dead_node_it);
    }
    LOG(INFO, "get status live nodes %u, dead nodes %u", live_nodes_.size(), dead_nodes_.size());
    done->Run();
}

void LumiaMasterImpl::RebootCallBack(const std::string sessionid,
                                   const std::vector<std::string> success,
                                   const std::vector<std::string> fails){ 

    MutexLock lock(&mutex_);
    std::vector<std::string> hosts_ok;
    const minion_set_id_index_t& index = boost::multi_index::get<id_tag>(minion_set_);
    for (size_t i = 0; i < success.size(); i++) {
        minion_set_id_index_t::const_iterator it = index.find(success[i]);
        if (it == index.end()) {
            LOG(WARNING, "host with id %s does not exist in lumia", success[i].c_str());
            continue;
        }
        hosts_ok.push_back(it->hostname_);
    } 
    std::vector<std::string> hosts_err;
    for (size_t i = 0; i < fails.size(); i++) {
        minion_set_id_index_t::const_iterator it = index.find(success[i]);
        if (it == index.end()) {
            LOG(WARNING, "host with id %s does not exist in lumia", success[i].c_str());
            continue;
        }
        hosts_err.push_back(it->hostname_);
    }
    LOG(INFO, "reboot call back succ hosts %s, fails hosts %s",
        boost::algorithm::join(hosts_ok, ",").c_str(),
        boost::algorithm::join(hosts_err, ",").c_str());
    if (success.size() > 0) {
        workers_.DelayTask(10000, boost::bind(&LumiaMasterImpl::HandleInitAgent, this, hosts_ok));
    }

}

void LumiaMasterImpl::GetMinion(::google::protobuf::RpcController* /*controller*/,
                   const ::baidu::lumia::GetMinionRequest* request,
                   ::baidu::lumia::GetMinionResponse* response,
                   ::google::protobuf::Closure* done) {
    MutexLock lock(&mutex_);
    const minion_set_ip_index_t& ip_index = boost::multi_index::get<ip_tag>(minion_set_);
    for (int i = 0; i < request->ips_size(); i++) {
        minion_set_ip_index_t::const_iterator it = ip_index.find(request->ips(i));
        if (it != ip_index.end()) {
            Minion* minion = response->add_minions();
            minion->CopyFrom(*(it->minion_));
        }    
    }
    const minion_set_hostname_index_t& ht_index = boost::multi_index::get<hostname_tag>(minion_set_);
    for (int i = 0; i < request->hostnames_size(); i++) {
        minion_set_hostname_index_t::const_iterator it = ht_index.find(request->hostnames(i));
        if (it != ht_index.end()) {
            Minion* minion = response->add_minions();
            minion->CopyFrom(*(it->minion_));
        }
    }
    const minion_set_id_index_t& id_index = boost::multi_index::get<id_tag>(minion_set_);
    for (int i = 0; i < request->ids_size(); i++) {
        minion_set_id_index_t::const_iterator it  = id_index.find(request->ids(i));
        if (it != id_index.end()) {
            Minion* minion = response->add_minions();
            minion->CopyFrom(*(it->minion_));
        }
    }
    done->Run();

}

std::string LumiaMasterImpl::GetUUID() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::lexical_cast<std::string>(uuid); 
}

void LumiaMasterImpl::ScheduleTask() {
    MutexLock lock(&mutex_);
    std::set<std::string>::iterator it = job_under_working_->begin();
    for (; it != job_under_working_->end(); ++it) {
        Job* job = jobs_[*it];
        std::map<int32_t, Task*>::iterator task_it = job->tasks_.begin();
        for (; task_it != job->tasks_.end() && job->running_num_ < job->step_size_;
              ++task_it) {
            if (task_it->second->state_ != kCtrlTaskPending) {
                continue;
            }
            bool ok = RunTask(job, task_it->second);
            if (ok) {
                LOG(INFO, "launch task %d of job %s on agent %s successfully", 
                    task_it->second->offset_,
                    job->id_.c_str(),
                    task_it->second->addr_.c_str());
                job->running_num_ ++;
            }
        }
    }
}

bool LumiaMasterImpl::RunTask(Job* job, Task* task) {
    mutex_.AssertHeld();
    if (task == NULL) {
        LOG(WARNING, "task is null");
        return false;
    }
    ExecRequest* request = new ExecRequest();
    request->set_job_id(task->job_id_);
    request->set_offset(task->offset_);
    request->set_content(job->content_);
    request->set_user(job->user_);
    ExecResponse* response = new ExecResponse();
    boost::function<void (const ExecRequest*, ExecResponse*, bool, int)> run_callback;
    run_callback = boost::bind(&LumiaMasterImpl::RunTaskCallback, this,
                               _1, _2, _3, _4, task->addr_);
    task->state_ = kCtrlTaskRunning;
    LumiaLet_Stub* agent = NULL;
    rpc_client_->GetStub(task->addr_, &agent);
    rpc_client_->AsyncRequest(agent, &LumiaLet_Stub::Exec,
                              request, response, run_callback, 5, 0);
    delete agent;
    return true;
}

void LumiaMasterImpl::RunTaskCallback(const ExecRequest* request,
                                      ExecResponse* response,
                                      bool fails, int , 
                                      const std::string& node_addr) {
    boost::scoped_ptr<const ExecRequest> request_ptr(request);
    boost::scoped_ptr<ExecResponse> response_ptr(response);
    MutexLock lock(&mutex_);
    if (fails || response->status() != 0) {
        LOG(WARNING, "the job %s task %d exec fails on agent %s",
                request->job_id().c_str(),
                request->offset(),
                node_addr.c_str());
        std::map<std::string, Job*>::iterator it = jobs_.find(request->job_id());
        if (it != jobs_.end()) {
            it->second->tasks_[request->offset()]->state_ = kCtrlTaskFails;
        }
        it->second->running_num_--;
    }
}

void LumiaMasterImpl::Exec(::google::protobuf::RpcController* controller,
                         const ::baidu::lumia::ExecTaskRequest* request,
                         ::baidu::lumia::ExecTaskResponse* response,
                         ::google::protobuf::Closure* done) {
    MutexLock lock(&mutex_);
    Job* job = new Job();
    job->id_ = GetUUID();
    job->content_ = request->content();
    job->user_ = request->user();
    LOG(INFO, "submit a job with conten %s and user %s", job->content_.c_str(),
        job->user_.c_str());
    const minion_set_hostname_index_t& ht_index = boost::multi_index::get<hostname_tag>(minion_set_);
    for (int i = 0; i < request->hosts_size(); i++) {
        std::string addr = request->hosts(i);
        minion_set_hostname_index_t::const_iterator it = ht_index.find(addr);
        if (it == ht_index.end()) {
            LOG(WARNING, "host %d does not no lumia", addr.c_str());
            response->add_not_exist_hosts(addr);
            continue;
        }
        Task* task = new Task();
        task->job_id_ = job->id_;
        task->state_ = kCtrlTaskPending;
        task->offset_ = i;
        task->addr_ = request->hosts(i);
        job->tasks_.insert(std::make_pair(i, task));
    }
    job->running_num_ = 0;
    job_under_working_->insert(job->id_);
    response->set_status(kLumiaOk);
    done->Run();
    LOG(INFO, "submit task with job %s successfully", job->id_.c_str());
}


}
}
