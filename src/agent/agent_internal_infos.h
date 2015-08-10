// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _AGENT_INTERNAL_INFOS_H
#define _AGENT_INTERNAL_INFOS_H

#include <string>
#include <vector>

#include "proto/galaxy.pb.h"
#include "proto/initd.pb.h"

namespace baidu {
namespace galaxy {

enum TaskStage {
    kStagePENDING = 0,      // use to check initd ready
    kStageDEPLOYING = 1,       // use to check deploy process ready
    kStageRUNNING = 2,          // use to check main process running
    kStageSTOPPING = 3,         // use to check stop process ready
    kStageENDING = 4          // end stage
};

struct TaskInfo {
    std::string task_id;    
    std::string pod_id;     // which pod belong to 
    TaskDescriptor desc;
    TaskStatus status;
    std::string initd_endpoint;
    TaskStage stage;
    ProcessInfo main_process;
    ProcessInfo deploy_process;
    ProcessInfo stop_process;
    std::string cgroup_path;
    std::string task_workspace;
    int fail_retry_times;
    int stop_timeout_point;
    TaskInfo(const TaskInfo& task) {
        task_id = task.task_id; 
        pod_id = task.pod_id;
        desc.CopyFrom(task.desc);
        status.CopyFrom(task.status);
        initd_endpoint = task.initd_endpoint;
        stage = task.stage;
        main_process.CopyFrom(task.main_process);
        deploy_process.CopyFrom(task.deploy_process);
        stop_process.CopyFrom(task.stop_process);
        cgroup_path = task.cgroup_path;
        task_workspace = task.task_workspace;
        fail_retry_times = task.fail_retry_times;
        stop_timeout_point = task.stop_timeout_point;
    }
};

struct PodInfo {
    std::string pod_id;
    PodDescriptor pod_desc;     
    PodStatus pod_status;
    int initd_port;
    int initd_pid;
    std::map<std::string, TaskInfo> tasks;
};

}   // ending namespace galaxy
}   // ending namespace baidu



#endif  //_AGENT_INTERNAL_INFOS_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
