// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#ifndef  GALAXY_GALAXY_H_
#define  GALAXY_GALAXY_H_

#include <string>
#include <vector>
#include <set>
#include <stdint.h>

namespace galaxy {

//
enum PackageTypeEnum{
    FTP_TYPE = 0,
    HTTP_TYPE,
    P2P_TYPE,
    BINARY_TYPE
};

enum TaskStateEnum{
    DEPLOYING_STATE = 0,
    RUNNING_STATE,
    ERROR_STATE
};

struct TaskDescription{
    int64_t task_id;
    int64_t job_id;
    std::string task_name;
    int32_t state;
    std::string addr;
};


struct PackageDescription{
    PackageTypeEnum type;
    std::string source;
    std::string version;
};

struct ResourceDescription{
    std::string name;
    int64_t value;
};

struct JobDescription{
    int64_t job_id;
    std::string job_name;
    std::string cmd_line;
    PackageDescription pkg;
    int32_t replicate_count;
    double cpu_share;
    int64_t mem_share;
    std::vector<ResourceDescription> resource_vector;
    std::string monitor_conf;
    int32_t deploy_step_size;
    double cpu_limit;
    bool one_task_per_host;
    std::string restrict_tag;
    bool is_updating;
    int32_t update_step_size;
};

struct NodeDescription {
    int64_t node_id;
    std::string addr;
    int32_t task_num;
    double cpu_share;
    int64_t mem_share;
    double cpu_used;
    int64_t mem_used;
    std::set<std::string> tags;
};

struct JobInstanceDescription : JobDescription{
    int32_t running_task_num;
};

class Galaxy {
public:
    static Galaxy* ConnectGalaxy(const std::string& master_addr);
    //create a new job
    virtual int64_t NewJob(const JobDescription& job) = 0;
    //update job for example update the replicate_count
    virtual bool UpdateJob(const JobDescription& job) = 0;
    //list all jobs in galaxys
    virtual bool ListJob(std::vector<JobInstanceDescription>* jobs) = 0;
    //termintate job
    virtual bool TerminateJob(int64_t job_id) = 0;
    //list all tasks of job
    virtual bool ListTask(int64_t job_id,
                          int64_t task_id,
                          std::vector<TaskDescription>* tasks) = 0;
    //list all nodes of cluster
    virtual bool ListNode(std::vector<NodeDescription>* nodes) = 0;
    //debug
    virtual bool KillTask(int64_t task_id) = 0;

    virtual bool ListTaskByAgent(const std::string& agent_addr,
                                 std::vector<TaskDescription> * tasks) = 0 ;

    virtual bool TagAgent(const std::string& tag, std::set<std::string>* agents) = 0;
};

} // namespace galaxy

#endif  // GALAXY_GALAXY_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
