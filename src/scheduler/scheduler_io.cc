// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scheduler/scheduler_io.h"

#include "logging.h"

namespace baidu {
namespace galaxy {

void SchedulerIO::HandleMasterChange(const std::string& new_master_endpoint) {
    if (new_master_endpoint.empty()) {
        LOG(WARNING, "the master endpoint is deleted from nexus");
    }
    if (new_master_endpoint != master_addr_) {
        MutexLock lock(&master_mutex_);
        LOG(INFO, "master change to %s", new_master_endpoint.c_str());
        master_addr_ = new_master_endpoint; 
        if (master_stub_) {
            delete master_stub_;
        }
        if (!rpc_client_.GetStub(master_addr_, &master_stub_)) {
            LOG(WARNING, "connect master %s failed", master_addr_.c_str()); 
            return;
        }  
    }
}

bool SchedulerIO::Init() {
    bool ok = master_watcher_.Init(
                    boost::bind(&SchedulerIO::HandleMasterChange, this, _1) );
    assert(ok);
    master_addr_ = master_watcher_.GetMasterEndpoint();
    LOG(INFO, "create scheduler io from master %s", master_addr_.c_str());
    bool ret = rpc_client_.GetStub(master_addr_, &master_stub_);
    assert(ret == true);
    return ok;
}

void SchedulerIO::Loop() {
    MutexLock lock(&master_mutex_);
    GetResourceSnapshotRequest sync_request;
    scheduler_.BuildSyncRequest(&sync_request);
    GetResourceSnapshotResponse sync_response;
    bool ret = rpc_client_.SendRequest(master_stub_,
                                      &Master_Stub::GetResourceSnapshot,
                                      &sync_request, &sync_response, 5, 1);
    if (!ret) {
        LOG(WARNING, "fail to get master resource snapshot");
        return;
    }
    int32_t agent_count = scheduler_.SyncResources(&sync_response);
    LOG(INFO, "sync resource from master successfully, agent count %d",
             agent_count);
    GetPendingJobsRequest get_jobs_request;
    GetPendingJobsResponse get_jobs_response;

    ListJobsRequest list_jobs_request;
    ListJobsResponse list_jobs_response;

    ProposeRequest pro_request;
    ProposeResponse pro_response;

    ret = rpc_client_.SendRequest(master_stub_,
                                 &Master_Stub::GetPendingJobs,
                                 &get_jobs_request,
                                 &get_jobs_response,
                                 5, 1);
    if (!ret) {
        LOG(WARNING, "fail to get pending jobs from master");
    }
    LOG(INFO, "get pending jobs from master , pending_size %d, scale_down_size %d",
            get_jobs_response.scale_up_jobs_size(),
            get_jobs_response.scale_down_jobs_size());

    ret = rpc_client_.SendRequest(master_stub_,
                            &Master_Stub::ListJobs,
                            &list_jobs_request,
                            &list_jobs_response,
                            5, 1);
    if (!ret) {
        LOG(WARNING, "fail to get list jobs from master");
    }
    int32_t jobs_size = scheduler_.SyncJobOverview(&list_jobs_response);
    LOG(INFO, "list jobs from master , #job %d", jobs_size);
    std::vector<JobInfo*> pending_jobs;
    for (int i = 0; i < get_jobs_response.scale_up_jobs_size(); i++) {
        pending_jobs.push_back(get_jobs_response.mutable_scale_up_jobs(i));
    }
    std::vector<JobInfo*> reducing_jobs;
    for (int i = 0; i < get_jobs_response.scale_down_jobs_size(); i++) {
        reducing_jobs.push_back(get_jobs_response.mutable_scale_down_jobs(i));
    }
    std::vector<ScheduleInfo*> propose;
    int32_t status = scheduler_.ScheduleScaleUp(pending_jobs, &propose);
    if (status < 0) {
        LOG(INFO, "fail to schedule scale up ");
        goto END;
    }
    status = scheduler_.ScheduleScaleDown(reducing_jobs, &propose);
    if (status < 0) {
        LOG(INFO, "fail to schedule scale down ");
        goto END;
    }
    status = scheduler_.ScheduleAgentOverLoad(&propose);
    if (status < 0) {
        LOG(INFO, "fail to schedule agent overload");
        goto END;
    }

    for (std::vector<ScheduleInfo*>::iterator it = propose.begin();
            it != propose.end(); ++it) {
        ScheduleInfo* sched = pro_request.add_schedule();
        sched->CopyFrom(*(*it));
    }
    ret = rpc_client_.SendRequest(master_stub_,
                                      &Master_Stub::Propose,
                                  &pro_request,
                                  &pro_response,
                                  5, 1);
    if (!ret) {
        LOG(INFO, "fail to propose");
    }
END:
    for (std::vector<ScheduleInfo*>::iterator it = propose.begin();
                it != propose.end(); ++it) {
        delete *it;
    }
    propose.clear();
}

} // galaxy
} // baidu
