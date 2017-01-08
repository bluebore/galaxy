// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "master/minion_ctrl.h"

#include <boost/algorithm/string/join.hpp>
#include "rapidjson/writer.h"
#include <gflags/gflags.h>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include <boost/uuid/uuid.hpp>
#include <boost/bind.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include "logging.h"

DECLARE_int32(exec_job_check_interval);
DECLARE_string(rms_token);
DECLARE_string(rms_app_key);
DECLARE_string(rms_auth_user);
DECLARE_string(rms_api_check_job);

namespace baidu {
namespace lumia {

MinionCtrl::MinionCtrl(const std::string& ccs_http_server,
                       const std::string& rms_http_server):ccs_http_server_(ccs_http_server),
    rms_http_server_(rms_http_server),
    http_client_(),
    http_workers_(4),
    call_back_workers_(4){

}

MinionCtrl::~MinionCtrl(){}

bool MinionCtrl::ExecOnNoah(const std::string& script,
                      const std::vector<std::string>& hosts,
                      const std::string& account,
                      int32_t concurrency,
                      std::string* sessionid,
                      const CallBack& callback) {
    MutexLock lock(&mutex_);
    LOG(INFO, "exec %s with account %s on hosts %s",
            script.c_str(), 
            account.c_str(),
            boost::algorithm::join(hosts, ",").c_str());
    std::string ticket;
    std::string service;
    bool ok = GenerateTicket(&ticket, &service);
    if (!ok) {
        return false;
    }
    std::string job;
    ok = BuildJob(script, hosts, account, service, concurrency, &job);
    if (!ok) {
        return false;
    }
    std::vector<std::pair<std::string, std::string> > data;
        data.push_back(std::make_pair("job", job));
    data.push_back(std::make_pair("credential", ticket));
    HttpPostRequest request;
    request.data = data;
    request.url = ccs_http_server_ + "/job/simplyAdd";
    HttpResponse response;
    ok = http_client_.Post(&request, &response);
    if (!ok) {
        LOG(WARNING, "post request to %s fails %s",ccs_http_server_.c_str(), response.error.c_str());
        return false;
    }
    rapidjson::Document doc;
    doc.Parse(response.body.c_str());
    if (!doc.IsObject()) {
        LOG(WARNING, "fail to parse response %s", response.body.c_str());
        return false;
    }
    if (doc.HasMember("code")
       && doc["code"].GetInt() == 0) {
        LOG(INFO, "submit cmd with job %s successfully",doc["data"]["jobId"].GetString());
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        std::string id = boost::lexical_cast<std::string>(uuid); 
        *sessionid = id;
        ExecContext& context = exec_sessions_[id];
        context.hosts = hosts;
        context.jobid = doc["data"]["jobId"].GetString();
        context.sessionid = id;
        context.script = script;
        context.callback = callback;
        context.try_count = 0;
        context.concurrency = concurrency;
        http_workers_.DelayTask(FLAGS_exec_job_check_interval, boost::bind(&MinionCtrl::CheckExecJob, this, id));
        return true;
    }else{
        LOG(WARNING, "submit cmd fails for %s", doc["msg"].GetString());
    }
    return false;
}

void MinionCtrl::CheckExecJob(const std::string& sessionid) {
    MutexLock lock(&mutex_);
    std::map<std::string, ExecContext>::iterator it = exec_sessions_.find(sessionid);
    if (it == exec_sessions_.end()) {
        LOG(WARNING, "sessionid %s does not exist ", sessionid.c_str());
        return;
    }
    ExecContext& context = it->second;
    LOG(INFO, "check job %s", context.jobid.c_str());
    HttpGetRequest request;
    request.url = ccs_http_server_ + "/job/query?jobId=" + context.jobid.c_str();
    HttpResponse response;
    bool ok = http_client_.Get(&request, &response);
    if (!ok) {
        LOG(WARNING, "fail get job %s task status from %s",it->second.jobid.c_str(), request.url.c_str());
        http_workers_.DelayTask(FLAGS_exec_job_check_interval, boost::bind(&MinionCtrl::CheckExecJob, this, sessionid));
        return;
    }
    rapidjson::Document doc;
    doc.Parse(response.body.c_str());
    if (!doc.IsObject()) {
        LOG(WARNING, "fail to parse response %s for request %s jobid %s", 
            response.body.c_str(),
            request.url.c_str(),
            context.jobid.c_str());
        // TODO error count
        http_workers_.DelayTask(FLAGS_exec_job_check_interval, boost::bind(&MinionCtrl::CheckExecJob, this, sessionid));
        return;
    }
    if (!doc.HasMember("code")
        || doc["code"].GetInt() != 0) {
        LOG(WARNING, "fail get job %s task status %s", context.jobid.c_str(), response.body.c_str());
        http_workers_.DelayTask(FLAGS_exec_job_check_interval, boost::bind(&MinionCtrl::CheckExecJob, this, sessionid));
        return;
    }
    if (!doc.HasMember("data")) {
        std::vector<std::string> place_holder;
        call_back_workers_.AddTask(boost::bind(context.callback, sessionid, place_holder, place_holder));
        exec_sessions_.erase(sessionid);
    }
    if (doc["data"]["jobStatus"] == "FINISH"
       || doc["data"]["jobStatus"] == "CANCELED") {
        LOG(INFO, "job %s is completed with status %s", context.jobid.c_str(), doc["data"]["jobStatus"].GetString());
        http_workers_.AddTask(boost::bind(&MinionCtrl::HandleJobFinished,
              this, sessionid));
    } else {
        LOG(INFO, "job %s is running with status %s", context.jobid.c_str(), doc["data"]["jobStatus"].GetString());
        http_workers_.DelayTask(FLAGS_exec_job_check_interval, boost::bind(&MinionCtrl::CheckExecJob, this, sessionid));
    }

}

void MinionCtrl::HandleJobFinished(const std::string& sessionid) {
    MutexLock lock(&mutex_);
    std::map<std::string, ExecContext>::iterator it = exec_sessions_.find(sessionid);
    if (it == exec_sessions_.end()) {
        LOG(WARNING, "sessionid %s does not exist ", sessionid.c_str());
        return;
    }
    ExecContext& context = it->second;
    LOG(INFO, "handle job %s completed", context.jobid.c_str());
    HttpGetRequest request;
    request.url = ccs_http_server_ + "/job/queryTaskList?jobId=" + context.jobid.c_str();
    HttpResponse response;
    bool ok = http_client_.Get(&request, &response);
    if (!ok) {
        LOG(WARNING, "fail get job %s task status from %s",it->second.jobid.c_str(), request.url.c_str());
        if (context.try_count <= 5) {
            http_workers_.DelayTask(FLAGS_exec_job_check_interval, boost::bind(&MinionCtrl::HandleJobFinished, this, sessionid));
            context.try_count += 1;
        }else {
            std::vector<std::string> succ;
            call_back_workers_.AddTask(boost::bind(context.callback, sessionid, succ, context.hosts));
            exec_sessions_.erase(sessionid);
        }
        return;
    }
    rapidjson::Document doc;
    doc.Parse(response.body.c_str());
    if (!doc.IsObject()) {
        LOG(WARNING, "fail to parse response %s for request %s jobid %s", 
            response.body.c_str(),
            request.url.c_str(),
            context.jobid.c_str());
        return;
    }
    if (!doc.HasMember("code")
        || doc["code"].GetInt() != 0) {
        LOG(WARNING, "fail get job %s task status %s", context.jobid.c_str(), response.body.c_str());
        return;
    }
    if (doc.HasMember("data")) {
        const rapidjson::Value& data = doc["data"];
        std::vector<std::string> succ;
        std::vector<std::string> fails;
        for (rapidjson::SizeType i = 0; i < data.Size(); i++) {
            std::string host = data[i]["host"].GetString();
            std::string task_status = data[i]["taskStatus"].GetString();
            int32_t exit_code = data[i]["exitCode"].GetInt();
            if (task_status == "EXITED" && exit_code == 0) {
                LOG(INFO, "exec job %s on host %s successfully", context.jobid.c_str(), host.c_str());
                succ.push_back(host);
            }else {
                LOG(WARNING, "exec job %s on host %s fails with task %s exit code %d", context.jobid.c_str(), host.c_str(), task_status.c_str(), exit_code);
                fails.push_back(host);
            }
        }
        call_back_workers_.AddTask(boost::bind(context.callback, sessionid, succ, fails));
        exec_sessions_.erase(sessionid);
    }
}


bool MinionCtrl::GenerateTicket(std::string* ticket, 
                                 std::string* service) {
    if (ticket == NULL || service == NULL) {
        return false;
    } 
    return true;
}

bool MinionCtrl::BuildJob(const std::string& script,
                           const std::vector<std::string>& hosts,
                           const std::string& account,
                           const std::string& service,
                           int32_t concurrency,
                           std::string* job) {
    if (job == NULL) {
        LOG(WARNING, "fail build job for job is NULL");
        return false;
    }
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    writer.String("serviceType");
    writer.String(service.c_str());
    writer.String("creator");
    writer.String(service.c_str());
    writer.String("account");
    writer.String(account.c_str());
    writer.String("command");
    writer.String(script.c_str());
    writer.String("concurrency");
    writer.Int(concurrency);
    writer.String("hosts");
    writer.StartArray();
    for (size_t i = 0; i < hosts.size(); i++) {
        writer.String(hosts[i].c_str());
    }
    writer.EndArray();
    writer.String("taskTimeout");
    //30 minutes
    writer.Int(1800000);
    writer.EndObject();
    *job = sb.GetString();
    return true;
}

bool MinionCtrl::Reboot(const std::vector<std::string>& hosts,
                        const CallBack& callback,
                        std::string* sessionid) {
    MutexLock lock(&mutex_);
    std::string access_token;
    bool ok = GetRmsAccessToken(&access_token);
    if (!ok) {
        LOG(WARNING, "fail to get rms access token");
        return false;
    }
    LOG(INFO, "get access_token %s", access_token.c_str());
    HttpGetRequest request;
    request.headers.push_back("ROP-Authorization: RopAuth " + access_token);
    std::string query_str;
    ok = BuildRebootJob(hosts, &query_str);
    if (!ok) {
        LOG(WARNING, "fail build job form");
        return false;
    }
    LOG(INFO, "reboot hosts %s",  boost::algorithm::join(hosts, ",").c_str());
    request.url = rms_http_server_ + "/v1/unified_list/selfReboot?" + query_str;
    HttpResponse response;
    ok = http_client_.Get(&request, &response);
    if (!ok) {
        LOG(WARNING, "fail to reboot for query_str %s", query_str.c_str());
        return false;
    }
    rapidjson::Document doc;
    doc.Parse(response.body.c_str());
    LOG(INFO, "reboot response %s", response.body.c_str());
    if (!doc.IsObject()) {
        LOG(WARNING, "fail to parse response %s", response.body.c_str());
        return false;
    }
    if (doc.HasMember("status") && doc["status"].GetInt() ==0) {
        LOG(INFO, "reboot %s successfully with id %s ", query_str.c_str(), doc["data"]["list_id"].GetString());
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        std::string id = boost::lexical_cast<std::string>(uuid); 
        *sessionid = id;
        ExecContext& context = exec_sessions_[id];
        context.hosts = hosts;
        context.jobid = doc["data"]["list_id"].GetString();
        context.sessionid = id;
        context.try_count = 0;
        context.callback = callback;
        http_workers_.DelayTask(FLAGS_exec_job_check_interval,
                               boost::bind(&MinionCtrl::CheckRebootJob, this, id));
        return true;
    }
    return false;
}

void MinionCtrl::CheckRebootJob(const std::string& sessionid) {
    MutexLock lock(&mutex_);
    std::map<std::string, ExecContext>::iterator c_it = exec_sessions_.find(sessionid);
    if (c_it == exec_sessions_.end()) {
        LOG(WARNING, "session with %s has been expired", sessionid.c_str());
        return;
    }
    ExecContext& context = c_it->second;
    HttpGetRequest request;
    request.url = FLAGS_rms_api_check_job + context.jobid;
    HttpResponse response;
    bool ok = http_client_.Get(&request, &response);
    if (!ok) {
        LOG(WARNING, "fail to get rms job %s status", context.jobid.c_str());
        if (context.try_count <= 0) {
            context.try_count += 1;
            http_workers_.DelayTask(FLAGS_exec_job_check_interval,
                          boost::bind(&MinionCtrl::CheckRebootJob, this, sessionid));
        } else {
            exec_sessions_.erase(sessionid);
        }
        return;
    }
    // TODO handle multi host
    std::size_t found = response.body.find("已交付");
    if (found !=std::string::npos) {
        LOG(INFO, "reboot with sessionid %s complete", sessionid.c_str());
        std::vector<std::string> place_holder;
        call_back_workers_.AddTask(boost::bind(context.callback, sessionid, context.hosts, place_holder));
        exec_sessions_.erase(sessionid);
    } else {
        http_workers_.DelayTask(FLAGS_exec_job_check_interval,
                          boost::bind(&MinionCtrl::CheckRebootJob, this, sessionid));

    }
}




bool MinionCtrl::BuildRebootJob(const std::vector<std::string>& hosts,
                                 std::string* query_str) {
    std::string machines;
    for (size_t i = 0; i < hosts.size(); i++) {
        if (i > 0) {
            machines.append(",");
        }
        machines.append(hosts[i]);
    }
    query_str->append("machines=");
    query_str->append(machines); 
    query_str->append("&skip_audit=1");
    return true;
}

bool MinionCtrl::GetRmsAccessToken(std::string* access_token) {
    HttpPostRequest request;
    request.url = rms_http_server_ + "/auth/accessToken";
    request.data.push_back(std::make_pair("app_key", FLAGS_rms_app_key));
    request.data.push_back(std::make_pair("secret_token", FLAGS_rms_token));
    request.data.push_back(std::make_pair("generate_type", "special_auth"));
    request.data.push_back(std::make_pair("user", FLAGS_rms_auth_user));
    HttpResponse response;
    bool ok = http_client_.Post(&request, &response);
    if (!ok) {
        LOG(WARNING, "fail to get rms access token");
        return false;
    }
    rapidjson::Document doc;
    doc.Parse(response.body.c_str());
    if (!doc.IsObject()) {
        LOG(WARNING, "fail to parse response %s", response.body.c_str());
        return false;
    }
    if (!doc.HasMember("access_token")) {
        LOG(WARNING, "fail get access token for response %s", response.body.c_str());
        return false;
    }
    *access_token = doc["access_token"].GetString();
    return true;
}

} // lumia
} // galaxy
