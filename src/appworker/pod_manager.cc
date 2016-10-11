// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pod_manager.h"

#include <stdlib.h>

#include <algorithm>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <glog/logging.h>
#include <gflags/gflags.h>

#include "protocol/appmaster.pb.h"
#include "utils.h"

DECLARE_int32(pod_manager_change_pod_status_interval);

namespace baidu {
namespace galaxy {

PodManager::PodManager() :
        mutex_(),
        background_pool_(10) {
    pod_.status = proto::kPodPending;
    pod_.reload_status = proto::kPodPending;
    pod_.stage = proto::kPodStageCreating;
    pod_.fail_count = 0;
    pod_.health = proto::kOk;
}

PodManager::~PodManager() {
    background_pool_.Stop(false);
}

int PodManager::SetPodEnv(const PodEnv& pod_env) {
    MutexLock lock(&mutex_);
    pod_.env = pod_env;
    pod_.pod_id = pod_env.pod_id;

    return 0;
}

int PodManager::SetPodDescription(const PodDescription& pod_desc) {
    MutexLock lock(&mutex_);
    pod_.desc.CopyFrom(pod_desc);

    return 0;
}

int PodManager::TerminatePod() {
    MutexLock lock(&mutex_);

    if (proto::kPodPending == pod_.status) {
        pod_.status = proto::kPodTerminated;
        return 0;
    }

    if (pod_.status == proto::kPodStopping
            || pod_.status == proto::kPodTerminated) {
        LOG(WARNING) << "pod is stopping";
        return 0;
    }

    pod_.stage = proto::kPodStageTerminating;
    LOG(INFO)
            << "terminate pod, "
            << "current pod status: " << PodStatus_Name(pod_.status);

    if (0 == DoStopPod()) {
        pod_.status = proto::kPodStopping;
    }

    return 0;
}

int PodManager::RebuildPod() {
    MutexLock lock(&mutex_);
    int tasks_size = pod_.desc.tasks().size();

    if (tasks_size != (int)(pod_.env.task_ids.size())) {
        LOG(WARNING)
                << "cgroup size mismatch task size, "
                << "cgroup size: " << pod_.env.task_ids.size() << ", "
                << "task size: " << tasks_size;
        pod_.status = proto::kPodFailed;
        return -1;
    }

    if (proto::kPodPending != pod_.status) {
        pod_.stage = proto::kPodStageRebuilding;

        if (0 == DoStopPod()) {
            pod_.status = proto::kPodStopping;
        }
    }

    return 0;
}

int PodManager::ReloadPod() {
    MutexLock lock(&mutex_);

    int tasks_size = pod_.desc.tasks().size();

    if (tasks_size != (int)(pod_.env.task_ids.size())) {
        LOG(WARNING)
                << "cgroup size mismatch task size, "
                << "cgroup size: " << pod_.env.task_ids.size() << ", "
                << "task size: " << tasks_size;
        pod_.reload_status = proto::kPodFailed;
        return -1;
    }

    if (0 != DoDeployReloadPod()) {
        pod_.reload_status = proto::kPodFailed;
        return -1;
    }

    // TODO replace task desc
    pod_.reload_status = proto::kPodDeploying;

    // add change pod reload status loop
    background_pool_.DelayTask(
        FLAGS_pod_manager_change_pod_status_interval,
        boost::bind(&PodManager::LoopChangePodReloadStatus, this)
    );

    return 0;
}

int PodManager::QueryPod(Pod& pod) {
    MutexLock lock(&mutex_);
    pod.pod_id = pod_.pod_id;
    pod.env = pod_.env;
    pod.desc.CopyFrom(pod_.desc);
    pod.status = pod_.status;
    pod.reload_status = pod_.reload_status;
    pod.stage = pod_.stage;
    pod.fail_count = pod_.fail_count;
    pod.services.assign(pod_.services.begin(), pod_.services.end());
    pod.health = pod_.health;

    return 0;
}

void PodManager::StartLoops() {
    MutexLock lock(&mutex_);
    background_pool_.DelayTask(
        FLAGS_pod_manager_change_pod_status_interval,
        boost::bind(&PodManager::LoopChangePodStatus, this)
    );
    background_pool_.DelayTask(
        FLAGS_pod_manager_change_pod_status_interval,
        boost::bind(&PodManager::LoopCheckPodService, this)
    );
    background_pool_.DelayTask(
        FLAGS_pod_manager_change_pod_status_interval,
        boost::bind(&PodManager::LoopCheckPodHealth, this)
    );
    task_manager_.StartLoops();
}

void PodManager::PauseLoops() {
    MutexLock lock(&mutex_);
    background_pool_.Stop(true);
    task_manager_.PauseLoops();
}

int PodManager::DumpPod(proto::PodManager* pod_manager) {
    MutexLock lock(&mutex_);
    // dump states
    proto::Pod* pod = pod_manager->mutable_pod();
    pod->set_pod_id(pod_.pod_id);
    pod->set_status(pod_.status);
    pod->set_reload_status(pod_.reload_status);
    pod->set_stage(pod_.stage);
    pod->set_fail_count(pod_.fail_count);
    pod->mutable_desc()->CopyFrom(pod_.desc);
    pod->set_health(pod_.health);
    // services
    std::vector<ServiceInfo>::iterator sit = pod_.services.begin();
    for (; sit != pod_.services.end(); ++sit) {
        proto::ServiceInfo* s = pod->add_services();
        s->CopyFrom(*sit);
    }
    // env
    proto::PodEnv* env = pod->mutable_env();
    env->set_user(pod_.env.user);
    env->set_workspace_path(pod_.env.workspace_path);
    env->set_workspace_abspath(pod_.env.workspace_abspath);
    env->set_job_id(pod_.env.job_id);
    env->set_pod_id(pod_.env.pod_id);
    env->set_ip(pod_.env.ip);
    env->set_hostname(pod_.env.hostname);
    // task_ids
    std::vector<std::string>::iterator it;
    it = pod_.env.task_ids.begin();
    for (; it != pod_.env.task_ids.end(); ++it) {
        std::string* task_id = env->add_task_ids();
        *task_id = *it;
    }
    // cgroup_subsystems
    it = pod_.env.cgroup_subsystems.begin();
    for (; it != pod_.env.cgroup_subsystems.end(); ++it) {
        std::string* cgroup_subsystem = env->add_cgroup_subsystems();
        *cgroup_subsystem = *it;
    }
    // task_cgroup_paths
    std::vector<std::map<std::string, std::string> >::iterator tit = \
            pod_.env.task_cgroup_paths.begin();
    for (; tit != pod_.env.task_cgroup_paths.end(); ++tit) {
        std::map<std::string, std::string>::iterator tmit = tit->begin();
        proto::CgroupPathMap* cgroup_path_map = env->add_task_cgroup_paths();

        for (; tmit != tit->end(); ++tmit) {
            proto::CgroupPath* cgroup_path = cgroup_path_map->add_cgroup_paths();
            cgroup_path->set_cgroup(tmit->first);
            cgroup_path->set_path(tmit->second);
        }
    }
    // task_ports
    std::vector<std::map<std::string, std::string> >::iterator pit = \
            pod_.env.task_ports.begin();
    for (; pit != pod_.env.task_ports.end(); ++pit) {
        std::map<std::string, std::string>::iterator pmit = \
            pit->begin();
        proto::PortMap* port_map = env->add_task_ports();
        for (; pmit != pit->end(); ++pmit) {
            proto::Port* port = port_map->add_ports();
            port->set_name(pmit->first);
            port->set_port(pmit->second);
        }
    }

    // dump task state
    task_manager_.PauseLoops();
    task_manager_.DumpTasks(pod_manager->mutable_task_manager());

    return 0;
}

int PodManager::LoadPod(const proto::PodManager& pod_manager) {
    MutexLock lock(&mutex_);
    if (!pod_manager.has_pod()) {
        return -1;
    }

    const proto::Pod& p = pod_manager.pod();
    if (p.has_pod_id()) {
        pod_.pod_id = p.pod_id();
    }
    if (p.has_status()) {
        pod_.status = p.status();
    }
    if (p.has_reload_status()) {
        pod_.reload_status = p.reload_status();
    }
    if (p.has_stage()) {
        pod_.stage = p.stage();
    }
    if (p.has_fail_count()) {
        pod_.fail_count = p.fail_count();
    }
    if (p.has_health()) {
        pod_.health = p.health();
    }
    if (p.has_desc()) {
        pod_.desc.CopyFrom(p.desc());
    }
    // services
    for (int i = 0; i < p.services().size(); ++i) {
        pod_.services.push_back(p.services(i));
    }
    // env
    if (p.has_env()) {
        const proto::PodEnv& e = p.env();
        if (e.has_user()) {
            pod_.env.user = e.user();
        }
        if (e.has_workspace_path()) {
            pod_.env.workspace_path = e.workspace_path();
        }
        if (e.has_workspace_abspath()) {
            pod_.env.workspace_abspath = e.workspace_abspath();
        }
        if (e.has_job_id()) {
            pod_.env.job_id = e.job_id();
        }
        if (e.has_pod_id()) {
            pod_.env.pod_id = e.pod_id();
        }
        if (e.has_ip()) {
            pod_.env.ip = e.ip();
        }
        if (e.has_hostname()) {
            pod_.env.hostname = e.hostname();
        }
        // env task_ids
        for (int i = 0; i < e.task_ids().size(); ++i) {
            pod_.env.task_ids.push_back(e.task_ids(i));
        }
        // env cgroup_subsystems
        for (int i = 0; i < e.cgroup_subsystems().size(); ++i) {
            pod_.env.cgroup_subsystems.push_back(e.cgroup_subsystems(i));
        }
        // env task_cgroup_paths
        for (int i = 0; i < e.task_cgroup_paths().size(); ++i) {
            const proto::CgroupPathMap& cgroup_path_map = e.task_cgroup_paths(i);
            std::map<std::string, std::string> cgroup_paths;
            for (int j = 0; j < cgroup_path_map.cgroup_paths().size(); ++j) {
                cgroup_paths.insert(std::make_pair(
                        cgroup_path_map.cgroup_paths(i).cgroup(),
                        cgroup_path_map.cgroup_paths(i).path()));
            }
            pod_.env.task_cgroup_paths.push_back(cgroup_paths);
        }
        // env task_ports
        for (int i = 0; i < e.task_ports().size(); ++i) {
            const proto::PortMap& port_map = e.task_ports(i);
            std::map<std::string, std::string> ports;
            for (int j = 0; j < port_map.ports().size(); ++j) {
                ports.insert(std::make_pair(
                       port_map.ports(j).name(),
                       port_map.ports(j).port()));
            }
            pod_.env.task_ports.push_back(ports);
        }
    }

    // load task_manager
    if (pod_manager.has_task_manager()) {
        task_manager_.LoadTasks(pod_manager.task_manager());
    }

    return 0;
}

int PodManager::DoCreatePod() {
    mutex_.AssertHeld();

    if (proto::kPodPending != pod_.status) {
        return -1;
    }

    int tasks_size = pod_.desc.tasks().size();

    if (tasks_size != (int)(pod_.env.task_ids.size())) {
        LOG(WARNING)
                << "cgroup size mismatch task size, "
                << "cgroup size: " << pod_.env.task_ids.size() << ", "
                << "task size: " << tasks_size;
        pod_.status = proto::kPodFailed;
        return -1;
    }

    pod_.pod_id = pod_.env.pod_id;
    pod_.stage = proto::kPodStageCreating;
    pod_.fail_count = 0;
    LOG(INFO) << "create pod, task size: " << tasks_size;

    for (int i = 0; i < tasks_size; ++i) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        TaskEnv env;
        env.user = pod_.env.user;
        env.job_id = pod_.env.job_id;
        env.pod_id = pod_.env.pod_id;
        env.task_id = task_id;
        env.cgroup_subsystems = pod_.env.cgroup_subsystems;
        env.cgroup_paths = pod_.env.task_cgroup_paths[i];
        env.ports = pod_.env.task_ports[i];
        env.workspace_path = pod_.env.workspace_path;
        env.workspace_abspath = pod_.env.workspace_abspath;

        int ret = task_manager_.CreateTask(env, pod_.desc.tasks(i));
        if (0 != ret) {
            LOG(WARNING) << "create task fail, task: " << task_id;
            DoClearPod();
            return ret;
        }

        LOG(INFO) << "create task ok, task " << task_id;
    }

    // fill services
    pod_.services.clear();
    std::vector<ServiceInfo>(pod_.services).swap(pod_.services);

    for (int i = 0; i < tasks_size; ++i) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        int32_t services_size = pod_.desc.tasks(i).services().size();
        std::map<std::string, std::string>::iterator p_it;

        for (int j = 0; j < services_size; ++j) {
            const proto::Service& service = pod_.desc.tasks(i).services(j);
            std::string port = "-1";
            if (service.has_port_name() && service.port_name() != "") {
                std::string port_name = service.port_name();
                transform(port_name.begin(), port_name.end(), port_name.begin(), toupper);
                p_it = pod_.env.task_ports[i].find(port_name);

                if (pod_.env.task_ports[i].end() == p_it) {
                    LOG(WARNING) << "### port not found: " << port_name;
                    continue;
                }
                port = p_it->second;
            }

            ServiceInfo service_info;
            service_info.set_name(service.service_name());
            service_info.set_hostname(pod_.env.hostname);
            service_info.set_port(port);
            service_info.set_ip(pod_.env.ip);
            service_info.set_status(proto::kError);
            service_info.set_deploy_path(pod_.env.workspace_abspath);
            service_info.set_task_id(task_id);
            pod_.services.push_back(service_info);
            LOG(INFO)
                    << "create task: " << i << ", "
                    << "service: " << service_info.name() << ", "
                    << "port: " << service_info.port() << ", "
                    << "hostname: " << service_info.hostname() << ", "
                    << "deploy_path: " << service_info.deploy_path();
        }
    }

    return 0;
}

int PodManager::DoClearPod() {
    mutex_.AssertHeld();
    task_manager_.ClearTasks();

    return 0;
}

// do task deploy processes creating
int PodManager::DoDeployPod() {
    mutex_.AssertHeld();
    int tasks_size = pod_.desc.tasks().size();
    LOG(INFO) << "deploy pod, task size: " << tasks_size;

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);

        if (0 != task_manager_.DeployTask(task_id)) {
            LOG(WARNING) << "create task deploy process fail, task: " << task_id;
            DoClearPod();
            pod_.status = proto::kPodPending;
            return -1;
        }

        LOG(INFO) << "create task deploy process ok, task: " << task_id;
    }

    return 0;
}

// do task main process creating
int PodManager::DoStartPod() {
    mutex_.AssertHeld();
    int tasks_size = pod_.desc.tasks().size();
    LOG(INFO) << "start pod, task size: " << tasks_size;

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        int ret = task_manager_.StartTask(task_id);

        if (0 != ret) {
            LOG(WARNING) << "create task main process fail, task:  " << task_id;
            return ret;
        }

        LOG(INFO) << "create task main process ok, task:  " << task_id;
    }

    return 0;
}

// do task stop process creating
int PodManager::DoStopPod() {
    mutex_.AssertHeld();
    int tasks_size = pod_.desc.tasks().size();
    LOG(INFO) << "stop pod, task size: " << tasks_size;

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);

        if (0 != task_manager_.StopTask(task_id)) {
            LOG(WARNING) << "create task stop process fail, task:  " << task_id;
            task_manager_.CleanTask(task_id);
        }
    }

    return 0;
}

int PodManager::DoDeployReloadPod() {
    mutex_.AssertHeld();
    int tasks_size = pod_.desc.tasks().size();
    LOG(INFO) << "deploy reload pod, task_size: " << tasks_size;

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);

        if (0 != task_manager_.DeployReloadTask(task_id, pod_.desc.tasks(i))) {
            LOG(WARNING) << "create reload task deploy process fail, task:  " << task_id;
            return -1;
        }

        LOG(INFO) << "create reload task deploy process ok, task: " << task_id;
    }

    return 0;
}

int PodManager::DoStartReloadPod() {
    mutex_.AssertHeld();
    int tasks_size = pod_.desc.tasks().size();
    LOG(INFO) << "start reload pod, task size: " << tasks_size;

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        int ret = task_manager_.StartReloadTask(task_id);

        if (0 != ret) {
            LOG(WARNING) << "create reload task main process fail, task:  " << task_id;
            return ret;
        }

        LOG(INFO) << "create reload task main process ok, task:  " << task_id;
    }

    return 0;
}

void PodManager::LoopChangePodStatus() {
    MutexLock lock(&mutex_);
    LOG(INFO)
            << "loop change pod status, "
            << "pod status: " << proto::PodStatus_Name(pod_.status);

    switch (pod_.status) {
    case proto::kPodPending:
        PendingPodCheck();
        break;

    case proto::kPodReady:
        ReadyPodCheck();
        break;

    case proto::kPodDeploying:
        DeployingPodCheck();
        break;

    case proto::kPodStarting:
        StartingPodCheck();
        break;

    case proto::kPodRunning:
        RunningPodCheck();
        break;

    case proto::kPodStopping:
        StoppingPodCheck();
        break;

    default:
        break;
    }

    background_pool_.DelayTask(
        FLAGS_pod_manager_change_pod_status_interval,
        boost::bind(&PodManager::LoopChangePodStatus, this)
    );

    return;
}

void PodManager::PendingPodCheck() {
    mutex_.AssertHeld();

    if (0 != pod_.desc.tasks().size()
            && 0 == DoCreatePod()) {
        pod_.status = proto::kPodReady;
    }

    return;
}

// create all pod deploy processes,
// if created ok, pod status change to deploying
void PodManager::ReadyPodCheck() {
    mutex_.AssertHeld();

    if (0 == DoDeployPod()) {
        pod_.status = proto::kPodDeploying;
        LOG(INFO) << "pod status change to kPodDeploying";
    }

    return;
}

// check all deploy processes' status,
// if all deployed ok, pod status change to starting
// if one deployed error, pod status change to failed
void PodManager::DeployingPodCheck() {
    mutex_.AssertHeld();
    int tasks_size = pod_.desc.tasks().size();
    TaskStatus task_status = proto::kTaskStarting;

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        Task task;

        if (0 != task_manager_.CheckTask(task_id, task)) {
            return;
        }

        if (task.status == proto::kTaskFailed) {
            task_status = proto::kTaskFailed;
            break;
        }

        if (task.status < task_status) {
            task_status = task.status;
        }
    }

    if (proto::kTaskStarting == task_status) {
        LOG(INFO) << "pod status change to kPodStarting";
        pod_.status = proto::kPodStarting;
    }

    if (proto::kTaskFailed == task_status) {
        LOG(INFO) << "pod status change to kPodFailed";
        pod_.status = proto::kPodFailed;
    }

    return;
}

// create all pod main processes,
// if all created ok, pod status changed to running
void PodManager::StartingPodCheck() {
    mutex_.AssertHeld();

    if (0 == DoStartPod()) {
        LOG(INFO) << "pod status change to kPodRunning";
        pod_.status = proto::kPodRunning;
    } else {
        LOG(INFO) << "pod status change to kPodFailed";
        pod_.status = proto::kPodFailed;
    }

    return;
}

// check all running processes' status,
// if all exit ok, pod status change to terminated,
// if one exit error, pod status change to failed
void PodManager::RunningPodCheck() {
    mutex_.AssertHeld();
    int tasks_size = pod_.desc.tasks().size();
    TaskStatus task_status = proto::kTaskFinished;

    int32_t fail_count = 0;

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        Task task;

        if (0 != task_manager_.CheckTask(task_id, task)) {
            return;
        }

        fail_count += task.fail_retry_times;

        if (proto::kTaskFailed == task.status) {
            task_status = proto::kTaskFailed;
            break;
        }

        if (task.status < task_status) {
            task_status = task.status;
        }
    }

    if (proto::kTaskRunning != task_status) {
        if (proto::kTaskFinished == task_status) {
            LOG(INFO) << "pod status change to kPodFinished";
            pod_.status = proto::kPodFinished;
        } else {
            LOG(INFO) << "pod status change to kPodFailed";
            pod_.status = proto::kPodFailed;
        }
    }

    pod_.fail_count = fail_count;
    return;
}

void PodManager::StoppingPodCheck() {
    mutex_.AssertHeld();
    int tasks_size = pod_.desc.tasks().size();
    TaskStatus task_status = proto::kTaskTerminated;

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        Task task;

        if (0 != task_manager_.CheckTask(task_id, task)) {
            task_manager_.CleanTask(task_id);
            task.status = proto::kTaskTerminated;
        }

        if (proto::kTaskStopping == task.status) {
            task_status = proto::kTaskStopping;
            break;
        }
    }

    if (proto::kTaskTerminated == task_status) {
        LOG(WARNING) << "pod status change to kPodTerminated";
        pod_.status = proto::kPodTerminated;

        if (proto::kPodStageRebuilding == pod_.stage) {
            LOG(WARNING) << "pod in rebuiding stage, pod status change to kPodPending";
            DoClearPod();
            pod_.stage = proto::kPodStageCreating;
            pod_.status = proto::kPodPending;
        }
    }

    return;
}

void PodManager::LoopChangePodReloadStatus() {
    MutexLock lock(&mutex_);

    switch (pod_.reload_status) {
    case proto::kPodDeploying: {
        int tasks_size = pod_.desc.tasks().size();
        TaskStatus task_status = proto::kTaskStarting;

        for (int i = 0; i < tasks_size; i++) {
            std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
            Task task;

            if (0 != task_manager_.CheckReloadTask(task_id, task)) {
                break;
            }

            if (task.reload_status == proto::kTaskFailed) {
                task_status = proto::kTaskFailed;
                break;
            }

            if (task.reload_status < task_status) {
                task_status = task.reload_status;
            }
        }

        if (proto::kTaskStarting == task_status) {
            LOG(INFO) << "pod reload status change to kPodStarting";
            pod_.reload_status = proto::kPodStarting;
        }

        if (proto::kTaskFailed == task_status) {
            LOG(INFO) << "pod reload status change to kPodFailed";
            pod_.reload_status = proto::kPodFailed;
        }

        break;
    }

    case proto::kPodStarting: {
        if (0 == DoStartReloadPod()) {
            LOG(INFO) << "pod reload status change to kPodRunning";
            pod_.reload_status = proto::kPodRunning;
        } else {
            LOG(WARNING) << "pod reload status change to kPodFailed";
            pod_.reload_status = proto::kPodFailed;
        }

        break;
    }

    case proto::kPodRunning: {
        // query reload running process status
        int tasks_size = pod_.desc.tasks().size();
        TaskStatus task_status = proto::kTaskFinished;

        for (int i = 0; i < tasks_size; i++) {
            std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
            Task task;

            if (0 != task_manager_.CheckReloadTask(task_id, task)) {
                return;
            }

            if (proto::kTaskFailed == task.reload_status) {
                task_status = proto::kTaskFailed;
                break;
            }

            if (task.reload_status < task_status) {
                task_status = task.reload_status;
            }
        }

        if (proto::kTaskRunning != task_status) {
            if (proto::kTaskFinished == task_status) {
                LOG(INFO) << "pod reload status change to kPodFinished";
                pod_.reload_status = proto::kPodFinished;
            } else {
                LOG(INFO) << "pod reload status change to kPodFailed";
                pod_.reload_status = proto::kPodFailed;
            }
        }

        break;
    }

    case proto::kPodFinished:
        LOG(INFO) << "pod reload ok";
        pod_.stage = proto::kPodStageCreating;
        return;

    case proto::kPodFailed:
        LOG(INFO) << "pod reload failed";
        return;

    default:
        break;
    }

    background_pool_.DelayTask(
        FLAGS_pod_manager_change_pod_status_interval,
        boost::bind(&PodManager::LoopChangePodReloadStatus, this)
    );

    return;
}

void PodManager::LoopCheckPodService() {
    MutexLock lock(&mutex_);
    LOG(INFO) << "loop check pod service";
    std::vector<ServiceInfo>::iterator it = pod_.services.begin();

    for (; it != pod_.services.end(); ++it) {
        int32_t port = boost::lexical_cast<int32_t>(it->port());
        // ignore port, service status follow with task.status
        if (port < 0) {
            it->set_status(proto::kError);
            TaskStatus task_status = proto::kTaskTerminated;
            int ret = task_manager_.QueryTaskStatus(it->task_id(), task_status);
            if (0 == ret && proto::kTaskRunning == task_status) {
                it->set_status(proto::kOk);
            }
        } else {
            if (net::IsPortOpen(port)) {
                it->set_status(proto::kOk);
            } else {
                it->set_status(proto::kError);
            }
        }
    }

    background_pool_.DelayTask(
        FLAGS_pod_manager_change_pod_status_interval,
        boost::bind(&PodManager::LoopCheckPodService, this)
    );

    return;
}

// check health check process status
void PodManager::PodHealthCheck() {
    MutexLock lock(&mutex_);
    proto::TaskStatus task_status = proto::kTaskFinished;
    int tasks_size = pod_.desc.tasks().size();

    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        Task task;
        int ret = task_manager_.CheckTaskHealth(task_id, task);

        if (0 != ret) {
            LOG(WARNING) << "check health task process fail, task:  " << task_id;
            task_status = proto::kTaskFailed;
            break;
        }

        if (task.status == proto::kTaskFailed) {
            task_status = proto::kTaskFailed;
            break;
        }

        if (task.status < task_status) {
            task_status = task.status;
        }
    }

    LOG(INFO) << "# check pod health, task status: " << proto::TaskStatus_Name(task_status);

    if (proto::kTaskRunning == task_status) {
        background_pool_.DelayTask(
            FLAGS_pod_manager_change_pod_status_interval,
            boost::bind(&PodManager::PodHealthCheck, this)
        );
        return;
    }

    if (proto::kTaskFailed == task_status) {
        pod_.health = proto::kError;
    }

    if (proto::kTaskFinished == task_status) {
        pod_.health = proto::kOk;
    }

    DoClearPodHealthCheck();
    background_pool_.DelayTask(
        FLAGS_pod_manager_change_pod_status_interval,
        boost::bind(&PodManager::LoopCheckPodHealth, this)
    );

    return;
}

int PodManager::DoClearPodHealthCheck() {
    mutex_.AssertHeld();
    LOG(INFO) << "clear pod health check";
    int tasks_size = pod_.desc.tasks().size();
    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        task_manager_.ClearTaskHealthCheck(task_id);
    }

    return 0;
}

int PodManager::DoStartPodHealthCheck() {
    mutex_.AssertHeld();

    int tasks_size = pod_.desc.tasks().size();
    for (int i = 0; i < tasks_size; i++) {
        std::string task_id = pod_.pod_id + "_" + boost::lexical_cast<std::string>(i);
        int ret = task_manager_.StartTaskHealthCheck(task_id);

        if (0 != ret) {
            LOG(WARNING) << "start task health check fail, task:  " << task_id;
            return ret;
        }
    }

    return 0;
}

void PodManager::LoopCheckPodHealth() {
    MutexLock lock(&mutex_);
    int tasks_size = pod_.desc.tasks().size();
    // no task, pod.health is kError
    if (tasks_size == 0) {
        pod_.health = proto::kError;
        background_pool_.DelayTask(
            FLAGS_pod_manager_change_pod_status_interval,
            boost::bind(&PodManager::LoopCheckPodHealth, this)
        );
        return;
    }

    if (0 != DoStartPodHealthCheck()) {
        LOG(WARNING) << "start pod health check failed";
        pod_.health = proto::kError;
        DoClearPodHealthCheck();
        background_pool_.DelayTask(
            FLAGS_pod_manager_change_pod_status_interval,
            boost::bind(&PodManager::LoopCheckPodHealth, this)
        );
    } else {
        LOG(INFO) << "start pod health check ok";
        background_pool_.DelayTask(
            FLAGS_pod_manager_change_pod_status_interval,
            boost::bind(&PodManager::PodHealthCheck, this)
        );
    }

    return;
}

} // ending namespace galaxy
} // ending namespace baidu
