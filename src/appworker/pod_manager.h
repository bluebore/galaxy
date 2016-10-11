// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  BAIDU_GALAXY_POD_MANAGER_H
#define  BAIDU_GALAXY_POD_MANAGER_H

#include <map>
#include <string>

#include <thread_pool.h>
#include <mutex.h>

#include "protocol/appmaster.pb.h"
#include "protocol/appworker.pb.h"
#include "task_manager.h"

namespace baidu {
namespace galaxy {

typedef proto::PodDescription PodDescription;
typedef proto::PodStatus PodStatus;
typedef proto::ServiceInfo ServiceInfo;
typedef proto::Status Status;
typedef proto::PodStage PodStage;

struct PodEnv {
    std::string user;
    std::string workspace_path;
    std::string workspace_abspath;
    std::string job_id;
    std::string pod_id;
    std::string ip;
    std::string hostname;
    std::vector<std::string> task_ids;
    std::vector<std::string> cgroup_subsystems;
    std::vector<std::map<std::string, std::string> > task_cgroup_paths;
    std::vector<std::map<std::string, std::string> > task_ports;
};

// when pod.stage == kPodRebuilding and pod.status == kPodTerminated,
// set pod.status = kPodPending

struct Pod {
    std::string pod_id;
    PodStatus status;
    PodStatus reload_status;
    PodStage stage;
    PodEnv env;
    PodDescription desc;
    int32_t fail_count;
    std::vector<ServiceInfo> services;
    Status health;
};

class PodManager {
public:
    PodManager();
    ~PodManager();
    int SetPodEnv(const PodEnv& pod_env);
    int SetPodDescription(const PodDescription& pod_desc);
    int TerminatePod();
    int RebuildPod();
    int ReloadPod();
    int QueryPod(Pod& pod);
    // upgrade
    int DumpPod(proto::PodManager* pod_manager);
    int LoadPod(const proto::PodManager& pod_manager);
    void StartLoops();
    void PauseLoops();

private:
    int DoCreatePod();
    int DoDeployPod();
    int DoStartPod();
    int DoStopPod();
    int DoClearPod();
    // reload
    int DoDeployReloadPod();
    int DoStartReloadPod();
    // loop
    void LoopChangePodStatus();
    void LoopChangePodReloadStatus();
    void LoopCheckPodService();
    void LoopCheckPodHealth();
    // health check
    int DoStartPodHealthCheck();
    int DoClearPodHealthCheck();
    // status check
    void PendingPodCheck();
    void ReadyPodCheck();
    void DeployingPodCheck();
    void StartingPodCheck();
    void StoppingPodCheck();
    void RunningPodCheck();
    void PodHealthCheck();

private:
    Mutex mutex_;
    Pod pod_;
    TaskManager task_manager_;
    ThreadPool background_pool_;
};

} // ending namespace galaxy
} // ending namespace baidu

#endif // BAIDU_GALAXY_POD_MANAGER_H
