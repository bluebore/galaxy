// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "job_tracker.h"
#include "timer.h"
#include "protocol/appmaster.pb.h"
#include "glog/logging.h"
#include "gflags/gflags.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>

#include <iostream>

DECLARE_int32(appmaster_check_deadpod_interval);
DECLARE_int32(appmaster_check_deploy_interval);
DECLARE_int32(master_pod_dead_threshhold);
DECLARE_int32(deploying_pod_timeout);

namespace baidu {
namespace galaxy {
namespace am {

JobTracker::JobTracker(const JobId& jobid, const baidu::galaxy::proto::JobDescription& desc) :
    id_(jobid),
    desc_(new baidu::galaxy::proto::JobDescription()),
    version_(baidu::common::timer::get_micros()),
    last_version_(0L),
    status_(baidu::galaxy::proto::kJobPending),
    running_(false),
    check_dead_thread_(0L),
    check_deploy_thread_(0L) {

    desc_->CopyFrom(desc);
}

JobTracker::~JobTracker() {
    running_ = false;
}

void JobTracker::CheckDeadPodLoop(int32_t interval) {
    // check heartbeat, remove dead pods
    boost::mutex::scoped_lock lock(mutex_);
    if (!running_) {
        LOG(INFO) << id_ << "donot run, check dead pod loop will exit";
        return;
    }

    assert(NULL != running_pool_.get());
    int64_t now = baidu::common::timer::get_micros();
    std::list<boost::shared_ptr<RuntimePod> >::iterator iter = runtime_pods_.begin();

    while (iter != runtime_pods_.end()) {
        if (now - (*iter)->heartbeat_time() > FLAGS_master_pod_dead_threshhold * 1000000L) {
            // remove pod from queue and index
            std::map<PodId, boost::shared_ptr<RuntimePod> >::iterator it
                = runtime_pods_index_.find((*iter)->id());
            LOG(WARNING) << (*iter)->id()
                         << " is dead, and will be removed from queue: detail msg: "
                         << (*iter)->ToString();
            assert(it != runtime_pods_index_.end());
            runtime_pods_index_.erase(it);
            runtime_pods_.erase(iter++);
            continue;
        }

        iter++;
    }

    check_dead_thread_ = running_pool_->DelayTask(interval * 1000,
        boost::bind(&JobTracker::CheckDeadPodLoop, this, interval));
}


void JobTracker::CheckDeployLoop(int32_t interval) {
    // check deploy step
    boost::mutex::scoped_lock lock(mutex_);

    if (!running_) {
        LOG(INFO) << id_ << "donot run, check deploy pod loop will exit";
        return;
    }

    assert(NULL != running_pool_.get());

    if (proto::kJobUpdating == status_
            || proto::kJobRunning == status_
            || proto::kJobPending == status_) {
        std::list<boost::shared_ptr<RuntimePod> >::iterator iter = runtime_pods_.begin();
        int32_t deploying_cnt = 0;
        int32_t need_deploying_cnt = 0;

        while (iter != runtime_pods_.end()) {
            if ((*iter)->PodInDeploying(version_, FLAGS_deploying_pod_timeout)) {
                deploying_cnt++;
            } else if ((*iter)->PodNeedUpdate(version_)) {
                need_deploying_cnt++;
            }

            iter++;
        }

        if (need_deploying_cnt > 0
                && deploying_cnt < (int)desc_->deploy().step()) {
            iter = runtime_pods_.begin();
            int32_t to_deploy_cnt = (int)desc_->deploy().step() - deploying_cnt;
            int real_add_cnt = 0;

            while (iter != runtime_pods_.end() && to_deploy_cnt) {
                if ((*iter)->PodNeedUpdate(version_)) {
                    (*iter)->set_expect_version(version_);
                    to_deploy_cnt--;
                    real_add_cnt++;
                }

                iter++;
            }

            LOG(INFO) << "deploying:" << id_ << "deploying_cnt : " << deploying_cnt
                      << " | need_deploying_cnt: " << need_deploying_cnt
                      << " | deploy_step: " << desc_->deploy().step()
                      << " | deploying_cnt: " << deploying_cnt + real_add_cnt
                      << " | deploying_deta: " << real_add_cnt;
        }
    }

    check_deploy_thread_ = running_pool_->DelayTask(interval * 1000,
        boost::bind(&JobTracker::CheckDeployLoop, this, interval));
}


void JobTracker::Run(boost::shared_ptr<baidu::common::ThreadPool> running_pool) {
    assert(NULL == running_pool_.get());
    running_ = true;
    running_pool_ = running_pool;
    CheckDeadPodLoop(FLAGS_appmaster_check_deadpod_interval);
    CheckDeployLoop(FLAGS_appmaster_check_deploy_interval);
}


baidu::galaxy::util::ErrorCode JobTracker::HandleFetch(
        const baidu::galaxy::proto::FetchTaskRequest& request,
        baidu::galaxy::proto::FetchTaskResponse& response) {
    boost::mutex::scoped_lock lock(mutex_);
    std::map<PodId, boost::shared_ptr<RuntimePod> >::iterator iter = this->runtime_pods_index_.find(request.podid());
    boost::shared_ptr<RuntimePod> rt_pod;

    if (iter == this->runtime_pods_index_.end()) {
        rt_pod.reset(new RuntimePod(request.podid()));
        rt_pod->set_expect_version(version_)
        ->update_updating_time();
        runtime_pods_index_[request.podid()] = rt_pod;
        runtime_pods_.push_back(rt_pod);
    } else {
        // process redundant pod
        if (iter->second->PodInfo().start_time() != request.start_time()) {
            if (iter->second->PodInfo().start_time() > request.start_time()) {
                response.mutable_error_code()->set_status(baidu::galaxy::proto::kOk);
                LOG(WARNING) << request.podid() << ":" << request.start_time()
                             << " is confict with :" << iter->second->PodInfo().start_time()
                             << " and will quit";
                return ERRORCODE_OK;
            }
        }

        rt_pod = iter->second;
    }

    rt_pod->update_heartbeat_time()
    ->update_pod_info(request);

    if (baidu::galaxy::proto::kJobPending == status_) {
        // process pending pod
        response.mutable_error_code()->set_status(baidu::galaxy::proto::kOk);
        response.mutable_pod()->CopyFrom(desc_->pod());
        status_ = baidu::galaxy::proto::kJobRunning;
        response.set_update_time(rt_pod->expect_version());
        LOG(INFO) << id_ << " transfer to kJobRunning status";
        status_ = baidu::galaxy::proto::kJobRunning;
    } else if (baidu::galaxy::proto::kJobRunning == status_) {
        if (!rt_pod->PeasNeedUpdate()) {
            switch (request.status()) {
                case proto::kPodFailed:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kRebuild);
                    break;

                default:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kSuspend);
                    break;
            }
        } else {
            switch (request.status()) {
                case proto::kPodPending:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kOk);
                    response.mutable_pod()->CopyFrom(desc_->pod());
                    response.set_update_time(rt_pod->expect_version());
                    break;

                case proto::kPodFailed:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kRebuild);
                    break;

                default:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kSuspend);
                    // suspend
            }
        }
    } else if (baidu::galaxy::proto::kJobDestroying == status_
               || baidu::galaxy::proto::kJobFinished == status_) {
        response.mutable_error_code()->set_status(baidu::galaxy::proto::kQuit);
    } else if (baidu::galaxy::proto::kJobUpdatePause == status_) {
    } else if (baidu::galaxy::proto::kJobUpdating == status_) {
        if (!rt_pod->PeasNeedUpdate()) {
            switch (request.status()) {
                case proto::kPodFailed:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kRebuild);
                    break;

                default:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kSuspend);
                    break;
            }
        } else {
            switch (request.status()) {
                case proto::kPodPending:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kOk);
                    response.mutable_pod()->CopyFrom(desc_->pod());
                    response.set_update_time(rt_pod->expect_version());
                    break;

                case proto::kPodFailed:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kRebuild);
                    break;

                default:
                    response.mutable_error_code()->set_status(baidu::galaxy::proto::kSuspend);
                    // suspend
            }
        }
    }

    return ERRORCODE(proto::kOk, "ok");
}

void JobTracker::ExportPodDescription(const baidu::galaxy::proto::JobDescription& job,
        baidu::galaxy::proto::PodDescription& pod) {
}

baidu::galaxy::util::ErrorCode JobTracker::Update(const baidu::galaxy::proto::JobDescription& desc,
        int breakpoint) {
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode JobTracker::ContinueUpdating(int breakpoint) {
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode JobTracker::PauseUpdating() {
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode JobTracker::RollbackUpdating() {
    return ERRORCODE_OK;
}

// donot realize
baidu::galaxy::util::ErrorCode JobTracker::CancelUpdating() {
    return ERRORCODE_OK;
}

// set for action
baidu::galaxy::util::ErrorCode JobTracker::ForceAction(const PodId& id,
        baidu::galaxy::proto::UpdateAction action) {
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode JobTracker::ForceCommand(const PodId& id,
        const std::string& action) {
    return ERRORCODE_OK;
}

const JobId& JobTracker::Id() {
    return id_;
}

const proto::JobDescription& JobTracker::Description() {
    boost::mutex::scoped_lock lock(mutex_);
    return *desc_; 

}

const proto::JobStatus JobTracker::Status() {
    boost::mutex::scoped_lock lock(mutex_);
    return status_;
}

void JobTracker::GetCounter(JobTracker::Counter& cnt) {
    boost::mutex::scoped_lock lock(mutex_);
    std::list<boost::shared_ptr<RuntimePod> >::iterator iter = runtime_pods_.begin();
    int pending_num = 0;
    int deploying_num = 0;
    int64_t failed_num = 0L;
    int finish_updating_num = 0;
    int running_num = 0;

    while (iter != runtime_pods_.end()) {
        if ((*iter)->PodInfo().status() == proto::kPodPending) {
            pending_num++;
        }

        if ((*iter)->PodInDeploying(version_, FLAGS_deploying_pod_timeout)) {
            deploying_num++;
        }

        if ((*iter)->PodInfo().status() == proto::kPodFailed) {
            failed_num++;
        } else if ((*iter)->PodInfo().status() == proto::kPodRunning) {
            running_num++;
        }

        if (!(*iter)->PodNeedUpdate(version_)) {
            finish_updating_num++;
        }

        cnt.pending_num = pending_num 
            + (desc_->deploy().replica() - runtime_pods_.size());
        cnt.failed_num = failed_num;
        cnt.running_num = running_num;
        cnt.deploying_num = deploying_num;
        cnt.finished_updating_num = finish_updating_num;
        iter++;
    }
}

JobVersion JobTracker::Version() {
    boost::mutex::scoped_lock lock(mutex_);
    return version_;
}

JobVersion JobTracker::LastVersion() {
    boost::mutex::scoped_lock lock(mutex_);
    return last_version_;
}


void JobTracker::PodInfo(::google::protobuf::RepeatedPtrField<proto::PodInfo >* pods) {
    boost::mutex::scoped_lock lock(mutex_);
    std::list<boost::shared_ptr<RuntimePod> >::iterator iter = runtime_pods_.begin();
    while(iter != runtime_pods_.end()) {
        proto::PodInfo* pi = pods->Add();
        pi->set_podid((*iter)->PodInfo().podid());
        pi->set_jobid((*iter)->PodInfo().jobid());
        pi->set_endpoint((*iter)->PodInfo().endpoint());
        pi->set_status((*iter)->PodInfo().status());
        pi->set_fail_count((*iter)->PodInfo().fail_count());
        pi->set_update_time((*iter)->PodInfo().update_time());
        pi->set_heartbeat_time((*iter)->heartbeat_time());
        pi->set_reload_status((*iter)->PodInfo().reload_status());
        pi->set_start_time((*iter)->PodInfo().start_time());
        for (int i = 0; i < (*iter)->PodInfo().services_size(); i++) {
            pi->mutable_services()->Add()->CopyFrom((*iter)->PodInfo().services(i));
        }
        iter++;
    }
}


size_t JobTracker::PodSize() {
    boost::mutex::scoped_lock lock(mutex_);
    return runtime_pods_.size();
}

void JobTracker::Destroy() {
    // FIXME: check status is confict or not
    boost::mutex::scoped_lock lock(mutex_);
    status_ = proto::kJobDestroying;
    running_pool_->CancelTask(check_dead_thread_);
    running_pool_->CancelTask(check_deploy_thread_);
}





}
}
}
