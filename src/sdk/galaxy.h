// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  GALAXY_GALAXY_H_
#define  GALAXY_GALAXY_H_

#include <string>
#include <vector>
#include <set>
#include <stdint.h>

namespace baidu {
namespace galaxy {

struct JobDescription {
    std::string job_name;
    std::string cmd_line;
    std::string binary;
    bool is_batch;
    int32_t replica;
    int32_t cpu_required;
    int64_t mem_required;
    int32_t deploy_step;
};

struct JobInformation {
    std::string job_name;
    std::string job_id;
    bool is_batch;
    int32_t replica;
    int32_t priority;
    int32_t running_num;
    int32_t cpu_used;
    int32_t mem_used;
    int32_t pending_num;
    int32_t deploying_num;
};

struct NodeDescription {
    std::string addr;
    int32_t task_num;
    int64_t cpu_share;
    int64_t mem_share;
    int64_t cpu_used;
    int64_t mem_used;
    int64_t cpu_assigned;
    int64_t mem_assigned;
    std::string labels;
};

class Galaxy {
public:
    static Galaxy* ConnectGalaxy(const std::string& master_addr);
    //create a new job
    virtual std::string SubmitJob(const JobDescription& job) = 0;
    //update job for example update the replicate_count
    virtual bool UpdateJob(const std::string& jobid, const JobDescription& job) = 0;
    //list all jobs in galaxys
    virtual bool ListJobs(std::vector<JobInformation>* jobs) = 0;
    //termintate job
    virtual bool TerminateJob(const std::string& job_id) = 0;
    //list all nodes of cluster
    virtual bool ListAgents(std::vector<NodeDescription>* nodes) = 0;
    virtual bool LabelAgents(const std::string& label, 
                             const std::vector<std::string>& agents) = 0;
};

} // namespace galaxy
} // namespace baidu

#endif  // GALAXY_GALAXY_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
