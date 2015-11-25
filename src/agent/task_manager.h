// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _SRC_AGENT_TASK_MANAGER_H
#define _SRC_AGENT_TASK_MANAGER_H

#include <deque>
#include "agent/agent_internal_infos.h"
#include "proto/initd.pb.h"
#include "rpc/rpc_client.h"
#include "thread_pool.h"

namespace baidu {
namespace galaxy {

class TaskManager {
public:
    TaskManager();
    virtual ~TaskManager(); 
    int Init();
    int CreateTask(const TaskInfo& task);
    int DeleteTask(const std::string& task_id);
    int QueryTask(TaskInfo* task);
    int ReloadTask(const TaskInfo& task);
protected:
    int QueryProcessInfo(Initd_Stub* stub,
                         ProcessInfo* process_info);

    // env prepare and clean
    int PrepareWorkspace(TaskInfo* task_info);
    int PrepareCgroupEnv(TaskInfo* task_info); 
    int PrepareResourceCollector(TaskInfo* task_info);
    int PrepareVolumeEnv(TaskInfo* task_info);
    int PrepareCpuScheduler(TaskInfo* task_info);

    int ResetCpuScheduler(TaskInfo* task_info);

    int CleanWorkspace(TaskInfo* task_info);
    int CleanCgroupEnv(TaskInfo* task_info);
    int CleanResourceCollector(TaskInfo* task_info);
    int CleanVolumeEnv(TaskInfo* task_info);
    int CleanProcess(TaskInfo* task_info);
    int CleanCpuScheduler(TaskInfo* task_info);

    // simple strategy to clear retry_times
    void RefreshRetryTimes(TaskInfo* task_info);
    void SetResourceUsage(TaskInfo* task_info);
    // task stage run
    int DeployTask(TaskInfo* task_info);
    int RunTask(TaskInfo* task_info);
    int TerminateTask(TaskInfo* task_info);
    int CleanTask(TaskInfo* task_info);

    // process check. ret -1 == error
    //                     0 == running
    //                     1 == finish
    int DeployProcessCheck(TaskInfo* task_info);
    int RunProcessCheck(TaskInfo* task_info);
    int TerminateProcessCheck(TaskInfo* task_info);
    int InitdProcessCheck(TaskInfo* task_info);

    void DelayCheckTaskStageChange(const std::string& task_id);

    inline void SetupDeployProcessKey(TaskInfo* task_info) {
        task_info->deploy_process.set_key(task_info->task_id 
                                          + "_deploy"); 
    }

    inline void SetupRunProcessKey(TaskInfo* task_info) {
        task_info->main_process.set_key(task_info->task_id);
    }

    inline void SetupTerminateProcessKey(TaskInfo* task_info) {
        task_info->stop_process.set_key(task_info->task_id  
                                        + "_stop"); 
    }
protected:
    Mutex tasks_mutex_;
    std::map<std::string, TaskInfo*> tasks_;
    ThreadPool background_thread_;
    std::string cgroup_root_;
    std::map<std::string, std::string> hierarchies_;
    RpcClient* rpc_client_;
};

}   // ending namespace galaxy
}   // ending namespace baidu


#endif  //_SRC_AGENT_TASK_MANAGER_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
