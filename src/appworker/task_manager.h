// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_TASK_MANAGER_H
#define BAIDU_GALAXY_TASK_MANAGER_H

#include <string>
#include <map>
#include <vector>

#include <mutex.h>
#include <thread_pool.h>

#include "protocol/galaxy.pb.h"
#include "protocol/appworker.pb.h"
#include "process_manager.h"

namespace baidu {
namespace galaxy {

typedef proto::TaskDescription TaskDescription;
typedef proto::TaskStatus TaskStatus;
typedef proto::Package Package;
typedef proto::ProcessStatus ProcessStatus;

struct TaskEnv {
    std::string user;
    std::string job_id;
    std::string pod_id;
    std::string task_id;
    std::vector<std::string> cgroup_subsystems;
    std::map<std::string, std::string> cgroup_paths;
    std::map<std::string, std::string> ports;
    std::string workspace_path;
    std::string workspace_abspath;
};

struct Task {
    std::string task_id;
    TaskDescription desc;
    TaskStatus status;
    TaskStatus prev_status;
    TaskStatus reload_status;
    int32_t packages_size;
    int32_t fail_retry_times;
    TaskEnv env;
    int32_t timeout_point;
    int32_t check_timeout_point;
};

class TaskManager {
public:
    TaskManager();
    ~TaskManager();
    int CreateTask(const TaskEnv& task_env,
                   const TaskDescription& task_desc);
    int DeployTask(const std::string& task_id);
    int StartTask(const std::string& task_id);
    int StopTask(const std::string& task_id);
    int CleanTask(const std::string& task_id);
    int CheckTask(const std::string& task_id, Task& task);
    int QueryTaskStatus(const std::string& task_id, TaskStatus& task_status);
    int ClearTasks();
    // upgrade
    void StartLoops();
    void PauseLoops();
    int DumpTasks(proto::TaskManager* task_manager);
    int LoadTasks(const proto::TaskManager& task_manager);
    // reload
    int DeployReloadTask(const std::string& task_id, const TaskDescription& task_desc);
    int StartReloadTask(const std::string& task_id);
    int CheckReloadTask(const std::string& task_id, Task& task);
    // health
    int StartTaskHealthCheck(const std::string& task_id);
    int CheckTaskHealth(const std::string& task_id, Task& task);
    int ClearTaskHealthCheck(const std::string& task_id);

private:
    int DoStartTask(const std::string& task_id);

private:
    Mutex mutex_;
    std::map<std::string, Task*> tasks_;
    ProcessManager process_manager_;
    ThreadPool background_pool_;
};

}   // ending namespace galaxy
}   // ending namespace baidu

#endif  // BAIDU_GALAXY_TASK_MANAGER_H
