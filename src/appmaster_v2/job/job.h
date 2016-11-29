// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "protocol/galaxy.pb.h"
#include "protocol/appmaster.pb.h"
#include "boost/thread/mutex.hpp"
#include "timer.h"

#include <assert.h>
#include <string>
#include <sstream>

namespace baidu {
namespace galaxy {
namespace am {

typedef std::string JobId;
typedef std::string PodId;
typedef int64_t PodVersion;
typedef int64_t JobVersion;

const static PodVersion NULL_POD_VERSION = 0L;
const static JobVersion NULL_JOB_VERSION = 0L;

// format: jobxxxxxxxxxxxxx.pod_123
/*class PodId {
public:
    explicit PodId(const std::string& id) :
    id_(id) {
    }

    explicit PodId(const PodId& id) :
    id_(id.ToString()) {
    }

    const std::string& ToString() const {
        return id_;
    }

    JobId job_id() {
        assert(id_.empty());
        size_t index = id_.find_first_of(".");
        if (std::string::npos == index
                || index <= 1) {
            return "";
        }

        return id_.substr(0, index);
    }

    std::string pod_index() {
        assert(id_.empty());
        size_t index = id_.find_first_of(".");
        if (std::string::npos == index
                || index <= 1
                || index == id_.length() - 1) {
            return "";
        }

        return id_.substr(index + 1, id_.length());
    }

    bool operator <(const PodId& id) {
        return id_ < id.ToString();
    }

    bool operator ==(const PodId& id) {
        return id_ == id.ToString();
    }
private:
    std::string id_;
};

class RuntimeJob {
public:
    RuntimeJob() {}
    ~RuntimeJob() {}

private:
    JobId id_;
    int64_t last_modify_time_;
    std::string modify_host_;   // host

    JobVersion version_;
    JobVersion last_version_;
    baidu::galaxy::proto::JobStatus status_;

};*/

class RuntimePod {
public:
    RuntimePod(const PodId& id) :
        id_(id),
        expect_version_(0L),
        last_version_(0L),
        updating_time_(0L),
        heartbeat_time_(0L) {
    }

    ~RuntimePod() {
    }

    const PodId& id() {
        boost::mutex::scoped_lock lock(mutex_);
        return id_;
    }

    PodVersion expect_version() {
        boost::mutex::scoped_lock lock(mutex_);
        return expect_version_;
    }

    int64_t updating_time() {
        boost::mutex::scoped_lock lock(mutex_);
        return updating_time_;
    }

    int64_t heartbeat_time() {
        boost::mutex::scoped_lock lock(mutex_);
        return heartbeat_time_;
    }

    RuntimePod* set_expect_version(PodVersion vs) {
        boost::mutex::scoped_lock lock(mutex_);
        expect_version_ = vs;
        return this;
    }

    RuntimePod* update_updating_time() {
        boost::mutex::scoped_lock lock(mutex_);
        updating_time_ = baidu::common::timer::get_micros();
        return this;
    }

    RuntimePod* update_heartbeat_time() {
        boost::mutex::scoped_lock lock(mutex_);
        heartbeat_time_ = baidu::common::timer::get_micros();
        return this;
    }

    RuntimePod* update_pod_info(const baidu::galaxy::proto::FetchTaskRequest& podinfo) {
        podinfo_.CopyFrom(podinfo);
        return this;
    }

    const baidu::galaxy::proto::FetchTaskRequest& PodInfo() {
        boost::mutex::scoped_lock lock(mutex_);
        return podinfo_;
    }

    bool PodInDeploying(JobVersion job_version, int deploy_pod_timeout) {
        boost::mutex::scoped_lock lock(mutex_);

        if (expect_version_ == job_version) {
            if (expect_version_ != podinfo_.update_time()) {
                return true;
            } else {
                // deploy timeout
                int64_t now = baidu::common::timer::get_micros();

                if (podinfo_.status() != proto::kPodRunning
                        && now - updating_time_ > deploy_pod_timeout * 1000000L) {
                    return true;
                }
            }
        }

        return false;
    }

    bool PodNeedUpdate(JobVersion job_version) {
        boost::mutex::scoped_lock lock(mutex_);
        return (expect_version_ != job_version);
    }

    bool PeasNeedUpdate() {
        boost::mutex::scoped_lock lock(mutex_);
        return expect_version_ != podinfo_.update_time();
    }

    std::string ToString() {
        boost::mutex::scoped_lock lock(mutex_);
        std::stringstream ss;
        ss << "podid: " << id_
           << "| expect_version: " << expect_version_
           << "| last_version:" << last_version_
           << "| curversion: " << podinfo_.update_time()
           << "| updating_time:" << updating_time_
           << "| heartbeat_time: " << heartbeat_time_
           << "endpoint: " << podinfo_.endpoint();
        return ss.str();
    }


private:
    boost::mutex mutex_;
    PodId id_;
    baidu::galaxy::proto::PodStatus expect_status_;
    baidu::galaxy::proto::FetchTaskRequest podinfo_;

    PodVersion expect_version_;
    PodVersion last_version_;

    int64_t updating_time_; // the first time expect_version_ equal version
    int64_t heartbeat_time_; // update when receive heart beat request
};
}
}
}
