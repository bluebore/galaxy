// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "job_manager.h"

#include "proto/master.pb.h"
#include "proto/galaxy.pb.h"
#include "master_util.h"

namespace baidu {
namespace galaxy {

void JobManager::Add(const JobDescriptor& job_desc) {
    Job* job = new Job();
    job->desc_.CopyFrom(job_desc);
    std::string job_id = MasterUtil::GenerateJobId(job_desc);
    MutexLock lock(&mutex_);
    for(int i = 0; i < job_desc.replica(); i++) {
        std::string pod_id = MasterUtil::GeneratePodId(job_desc);
        PodStatus* pod_status = new PodStatus();
        pod_status->set_podid(pod_id);
        pod_status->set_jobid(job_id);
        job->pods_[pod_id] = pod_status;
        pending_pods_[job_id][pod_id] = pod_status;
    }   
    jobs_[job_id] = job;
}

void JobManager::GetPendingPods(JobInfoList* pending_pods) {
    MutexLock lock(&mutex_);
    std::map<JobId, std::map<PodId, PodStatus*> >::iterator it;
    for (it = pending_pods_.begin(); it != pending_pods_.end(); ++it) {
        JobInfo* job_info = pending_pods->Add();
        JobId job_id = it->first;
        job_info->set_jobid(job_id);
        const JobDescriptor& job_desc = jobs_[job_id]->desc_;
        job_info->mutable_job()->CopyFrom(job_desc);

        const std::map<PodId, PodStatus*> & job_pending_pods = it->second;
        std::map<PodId, PodStatus*>::const_iterator jt;
        for (jt = job_pending_pods.begin(); jt != job_pending_pods.end(); ++jt) {
            PodStatus* pod_status = jt->second;
            PodStatus* new_pod_status = job_info->add_tasks();
            new_pod_status->CopyFrom(*pod_status);
        }
    }
}

}
}
