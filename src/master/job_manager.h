// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_GALAXY_JOB_MANGER_H
#define BAIDU_GALAXY_JOB_MANGER_H
#include <string>
#include <map>
#include <vector>
#include "proto/master.pb.h"
#include "proto/galaxy.pb.h"
#include <mutex.h>

namespace baidu {
namespace galaxy {

typedef std::string JobId;
typedef std::string PodId;
typedef google::protobuf::RepeatedPtrField<baidu::galaxy::JobInfo> JobInfoList;

struct Job {
    std::map<PodId, PodStatus*> pods_;
    JobDescriptor desc_;
};

class JobManager {
public:
    void Add(const JobDescriptor& job_desc);
    void GetPendingPods(JobInfoList* pending_pods);
private:
    std::map<JobId, Job*> jobs_;
    std::map<JobId, std::map<PodId, PodStatus*> > pending_pods_;
    Mutex mutex_;   
};

}
}

#endif
