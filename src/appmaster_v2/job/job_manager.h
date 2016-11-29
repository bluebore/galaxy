// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "job.h"
#include "job_tracker.h"
#include "boost/thread/mutex.hpp"
#include "protocol/resman.pb.h"

#include <stdint.h>

#include <map>

namespace baidu {
namespace galaxy {
namespace am {

class RuntimeJob {
public:
    RuntimeJob(const JobId& id, const baidu::galaxy::proto::JobDescription& desc) :
        job_tracker_(new JobTracker(id, desc)),
        last_modify_time_(0),
        last_modify_host_("") {
    }

    int64_t modify_time() {
        return last_modify_time_;
    }

    std::string modify_host() {
        return last_modify_host_;
    }

    boost::shared_ptr<JobTracker> job_tracker() {
        return job_tracker_;
    }

    RuntimeJob* update_modify_time() {
        last_modify_time_ = baidu::common::timer::get_micros();
        return this;
    }

    RuntimeJob* update_modify_host(const std::string& host) {
        last_modify_host_ = host;
        return this;
    }

    RuntimeJob* update_create_time() {
        create_time_ = baidu::common::timer::get_micros();
        return this;
    }

    int64_t create_time() {
        return create_time_;
    }

    RuntimeJob* update_owner(const baidu::galaxy::proto::User& owner) {
        owner_.CopyFrom(owner);
        return this;
    }

    const baidu::galaxy::proto::User& owner() {
        return owner_;
    }


private:
    boost::shared_ptr<JobTracker> job_tracker_;
    int64_t last_modify_time_;
    int64_t create_time_;
    std::string last_modify_host_;
    baidu::galaxy::proto::User owner_;

};

class JobManager {
public:
    JobManager();
    ~JobManager();


    baidu::galaxy::util::ErrorCode Submit(
            const JobId& id,
            const baidu::galaxy::proto::JobDescription& desc,
            const baidu::galaxy::proto::User& user);

    baidu::galaxy::util::ErrorCode UpdateJob(const JobId& id,
            const baidu::galaxy::proto::JobDescription& desc,
            int breakpoint);

    baidu::galaxy::util::ErrorCode UpdateContinue(const JobId& id, int breakpoint);
    baidu::galaxy::util::ErrorCode PauseUpdating(const JobId& id);
    baidu::galaxy::util::ErrorCode RollbackUpdating(const JobId& id);
    baidu::galaxy::util::ErrorCode CancelUpdating(const JobId& id);
    baidu::galaxy::util::ErrorCode RemoveJob(const JobId& id, 
            proto::RemoveContainerGroupResponse* response);
 
    baidu::galaxy::util::ErrorCode HandleFetch(const baidu::galaxy::proto::FetchTaskRequest& req,
            baidu::galaxy::proto::FetchTaskResponse& response);

    void ListJobs(proto::ListJobsResponse* response);
    baidu::galaxy::util::ErrorCode ShowJob(const JobId& id, proto::ShowJobResponse* response);
private:
    void FinishedJobCheckLoop(int interval);

    std::map<JobId, boost::shared_ptr<RuntimeJob> > rt_jobs_;
    boost::mutex mutex_;
    boost::shared_ptr<baidu::common::ThreadPool> job_tracker_threadpool_;
};
}
}
}
