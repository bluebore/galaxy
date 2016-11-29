// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "job_manager.h"
#include "job_tracker.h"
#include "thread_pool.h"
#include "agent/util/error_code.h"
#include "protocol/appmaster.pb.h"

#include "glog/logging.h"

namespace baidu {
namespace galaxy {
namespace am {

JobManager::JobManager() :
    job_tracker_threadpool_(new baidu::common::ThreadPool(10)) {
}

JobManager::~JobManager() {
}

baidu::galaxy::util::ErrorCode JobManager::Submit(
        const JobId& id,
        const baidu::galaxy::proto::JobDescription& desc,
        const baidu::galaxy::proto::User& user) {
    // create job tracker
    boost::shared_ptr<RuntimeJob> jobinfo(new RuntimeJob(id, desc));
    jobinfo->update_modify_time()
        ->update_owner(user)
        ->update_create_time();

    boost::mutex::scoped_lock lock(mutex_);
    rt_jobs_[id] = jobinfo;
    jobinfo->job_tracker()->Run(job_tracker_threadpool_);
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode JobManager::HandleFetch(const baidu::galaxy::proto::FetchTaskRequest& req,
        baidu::galaxy::proto::FetchTaskResponse& response) {
    boost::mutex::scoped_lock lock(mutex_);
    std::map<JobId, boost::shared_ptr<RuntimeJob> >::const_iterator iter
        = rt_jobs_.find(req.jobid());

    if (iter == rt_jobs_.end()) {
        return ERRORCODE(proto::kJobNotFound, "job donot exist: %s", req.jobid().c_str());
    }

    boost::shared_ptr<RuntimeJob> rt_job = iter->second;
    assert(NULL != rt_job);
    return rt_job->job_tracker()->HandleFetch(req, response);
}



baidu::galaxy::util::ErrorCode UpdateContinue(const JobId& id, int breakpoint) {
    assert(breakpoint >= 0);
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode PauseUpdating(const JobId& id) {
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode RollbackUpdating(const JobId& id) {
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode CancelUpdating(const JobId& id) {
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode JobManager::UpdateJob(const JobId& id,
        const baidu::galaxy::proto::JobDescription& desc,
        int breakpoint) {
    // check if job exists or not
    boost::shared_ptr<RuntimeJob> rt_job;
    {
        boost::mutex::scoped_lock lock(mutex_);
        std::map<JobId, boost::shared_ptr<RuntimeJob> >::const_iterator iter
            = rt_jobs_.find(id);

        if (iter == rt_jobs_.end()) {
            return ERRORCODE(-1, "job %s donot exist", id.c_str());
        }

        rt_job = iter->second;
    }
    return rt_job->job_tracker()->Update(desc, breakpoint);
}

baidu::galaxy::util::ErrorCode JobManager::RemoveJob(const JobId& id, 
            proto::RemoveContainerGroupResponse* response) {
    assert(NULL != response);
    boost::mutex::scoped_lock lock(mutex_);
    std::map<JobId, boost::shared_ptr<RuntimeJob> >::const_iterator iter
        = rt_jobs_.find(id);

    if (iter == rt_jobs_.end()) {
        response->mutable_error_code()->set_status(proto::kJobNotFound);
        response->mutable_error_code()->set_reason("job is not found");
        return ERRORCODE(0, "job %s donot exist", id.c_str());
    }

    // FIXME: write to nexus when job'status changed
    //baidu::galaxy::nexus::NexusProxy::GetInstance()->Put();

    return ERRORCODE_OK;
}


void JobManager::ListJobs(proto::ListJobsResponse* response) {
    assert(NULL != response);
    boost::mutex::scoped_lock lock(mutex_);
    std::map<JobId, boost::shared_ptr<RuntimeJob> >::iterator iter = rt_jobs_.begin();
    while (iter != rt_jobs_.end()) {

        proto::JobOverview* jo = response->mutable_jobs()->Add();
        boost::shared_ptr<JobTracker> jt = iter->second->job_tracker();
        JobTracker::Counter cnt;
        jt->GetCounter(cnt);

        jo->set_jobid(jt->Id());
        jo->mutable_desc()->CopyFrom(jt->Description());
        jo->set_status(jt->Status());
        jo->set_running_num(cnt.running_num);
        jo->set_pending_num(cnt.pending_num);
        jo->set_death_num(cnt.death_num);
        jo->set_fail_count(cnt.failed_num);
        jo->set_create_time(0);
        jo->set_update_time(0);
        jo->mutable_user()->CopyFrom(iter->second->owner());
        iter++;
    }

    response->mutable_error_code()->set_status(proto::kOk);
}


baidu::galaxy::util::ErrorCode JobManager::ShowJob(const JobId& id, proto::ShowJobResponse* response) {
    assert(NULL != response);
    boost::mutex::scoped_lock lock(mutex_);

    std::map<JobId, boost::shared_ptr<RuntimeJob> >::iterator iter
        = rt_jobs_.find(id);

    if (iter == rt_jobs_.end()) {
        response->mutable_error_code()->set_status(proto::kJobNotFound);
        response->mutable_error_code()->set_reason("job is not found");
        return ERRORCODE(-1, "job donot exist: %s", id.c_str());
    }


    boost::shared_ptr<JobTracker> jt = iter->second->job_tracker();

    proto::JobInfo* job = response->mutable_job();
    job->set_jobid(jt->Id());
    job->mutable_desc()->CopyFrom(jt->Description());
    job->set_status(jt->Status());
    //job->set_version();
    job->set_create_time(iter->second->create_time());
    job->set_update_time(jt->Version());
    //job->mutable_last_desc()->CopyFrom();
    job->mutable_user()->CopyFrom(iter->second->owner());
    //job->set_action();
    //job->set_last_version();
    job->set_rollback_time(jt->LastVersion());

    jt->PodInfo(job->mutable_pods());
    response->mutable_error_code()->set_status(proto::kOk);

    return ERRORCODE_OK;
}

void JobManager::FinishedJobCheckLoop(int interval) {
    boost::mutex::scoped_lock lock(mutex_);
    std::map<JobId, boost::shared_ptr<RuntimeJob> >::iterator iter = rt_jobs_.begin();

    while (iter != rt_jobs_.end()) {
        boost::shared_ptr<JobTracker> jt = iter->second->job_tracker();
        if (jt->Status() == proto::kJobDestroying
                    && jt->PodSize() == 0) {
            LOG(INFO) << "job " << iter->first << " is finished";
            rt_jobs_.erase(iter++);
            continue;
        }
        iter++;
    }
}

}
}
}
