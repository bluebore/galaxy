// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "task_manager.h"

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <timer.h>
#include "utils.h"

DECLARE_int32(task_manager_background_thread_pool_size);
DECLARE_int32(task_manager_task_stop_command_timeout);
DECLARE_int32(task_manager_task_check_command_timeout);
DECLARE_int32(task_manager_loop_wait_interval);
DECLARE_int32(task_manager_task_max_fail_retry_times);
DECLARE_int32(process_manager_download_retry_times);
DECLARE_int32(process_manager_process_retry_delay);

namespace baidu {
namespace galaxy {

int MakeProcessEnv(Task* task, ProcessEnv& env);

TaskManager::TaskManager() :
        mutex_(),
        background_pool_(FLAGS_task_manager_background_thread_pool_size) {
}

TaskManager::~TaskManager() {
    background_pool_.Stop(false);
}

int TaskManager::CreateTask(const TaskEnv& task_env,
                            const TaskDescription& task_desc) {
    MutexLock lock(&mutex_);
    Task* task = new Task();
    task->env = task_env;
    task->task_id = task_env.task_id;
    task->desc.CopyFrom(task_desc);
    task->status = proto::kTaskPending;
    task->prev_status = proto::kTaskPending;
    task->reload_status = proto::kTaskPending;
    tasks_.insert(std::make_pair(task_env.task_id, task));
    return 0;
}

// create all task deploy processes,
// for downloading exe package and data packages
int TaskManager::DeployTask(const std::string& task_id) {
    MutexLock lock(&mutex_);
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    LOG(INFO) << "deplay task start, task: " << task_id;
    Task* task = it->second;
    task->packages_size = 0;

    if (task->desc.has_exe_package()
            && task->desc.exe_package().has_package()) {
        DownloadProcessContext context;
        context.process_id = task->task_id + "_deploy_0";
        context.src_path = task->desc.exe_package().package().source_path();
        context.dst_path = task->desc.exe_package().package().dest_path();
        context.version = boost::replace_all_copy(task->desc.exe_package().package().version(), " ", "");
        context.work_dir = task->env.workspace_path;
        context.package = context.work_dir + "/" + context.process_id
                          + "." + context.version + ".tar.gz";

        // process env
        ProcessEnv env;
        MakeProcessEnv(task, env);

        if (0 != process_manager_.CreateProcess(env, &context)) {
            LOG(WARNING) << "command execute fail, command: " << context.cmd;
            return -1;
        }

        task->packages_size++;
    }

    if (task->desc.has_data_package()) {
        for (int i = 0; i < task->desc.data_package().packages_size(); i++) {
            DownloadProcessContext context;
            context.process_id = task->task_id + "_deploy_"\
                                 + boost::lexical_cast<std::string>(i + 1);
            context.src_path = task->desc.data_package().packages(i).source_path();
            context.dst_path = task->desc.data_package().packages(i).dest_path();
            context.version = boost::replace_all_copy(task->desc.data_package().packages(i).version(), " ", "");
            context.work_dir = task->env.workspace_path;
            context.package = context.work_dir + "/" + context.process_id
                              + "." + context.version + ".tar.gz";

            ProcessEnv env;
            MakeProcessEnv(task, env);

            if (0 != process_manager_.CreateProcess(env, &context)) {
                LOG(WARNING) << "command execute fail, command: " << context.cmd;
                return -1;
            }

            task->packages_size++;
        }
    }

    task->status = proto::kTaskDeploying;
    return 0;
}

// create task main process
int TaskManager::DoStartTask(const std::string& task_id) {
    LOG(INFO) << "start task: " << task_id;
    mutex_.AssertHeld();
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    LOG(INFO) << "start create main process for task: " << task_id;
    Task* task = it->second;

    if (task->desc.has_exe_package()) {
        ProcessContext context;
        context.process_id = task->task_id + "_main";
        context.cmd = task->desc.exe_package().start_cmd();
        context.work_dir = task->env.workspace_path;

        ProcessEnv env;
        MakeProcessEnv(task, env);

        if (0 != process_manager_.CreateProcess(env, &context)) {
            LOG(WARNING) << "command execute fail, command: " << context.cmd;
            return -1;
        }
    }

    task->status = proto::kTaskRunning;
    return 0;
}

int TaskManager::StartTask(const std::string& task_id) {
    MutexLock loc(&mutex_);
    return DoStartTask(task_id);
}

int TaskManager::StopTask(const std::string& task_id) {
    MutexLock lock(&mutex_);
    LOG(INFO) << "stop task: " << task_id;
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    LOG(INFO) << "start create stop process for task: " << task_id;
    Task* task = it->second;
    // save current status, for cleaning
    task->prev_status = task->status;

    if (!task->desc.has_exe_package()
            || !task->desc.exe_package().has_stop_cmd()) {
        return -1;
    }

    task->timeout_point = common::timer::now_time()\
                          + FLAGS_task_manager_task_stop_command_timeout;
    ProcessContext context;
    context.process_id = task->task_id + "_stop";
    context.cmd = task->desc.exe_package().stop_cmd();
    context.work_dir = task->env.workspace_path;

    ProcessEnv env;
    MakeProcessEnv(task, env);

    if (0 != process_manager_.CreateProcess(env, &context)) {
        LOG(WARNING) << "command execute fail, command: " << context.cmd;
        return -1;
    }

    task->status = proto::kTaskStopping;
    return 0;
}

int TaskManager::CheckTask(const std::string& task_id, Task& task) {
    MutexLock lock(&mutex_);
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    // task status switch
    std::string process_id;
    Process process;

    switch (it->second->status) {
    case proto::kTaskDeploying: {
        ProcessStatus process_status = proto::kProcessFinished;

        for (int i = 0; i < it->second->packages_size; i++) {
            process_id = task_id + "_deploy_" + boost::lexical_cast<std::string>(i);

            if (0 != process_manager_.QueryProcess(process_id, process)) {
                LOG(WARNING) << "query deploy process: " << process_id << " fail";
                it->second->status = proto::kTaskFailed;
                return -1;
            }

            if (process.status > proto::kProcessFinished) {
                LOG(WARNING) << "deploy process " << process_id << " failed";
                if (process.fail_retry_times < FLAGS_process_manager_download_retry_times) {
                    // process env
                    ProcessEnv env;
                    MakeProcessEnv(it->second, env);
                    DownloadProcessContext context;
                    context.delay_time = FLAGS_process_manager_process_retry_delay;

                    if (0 == i) {
                        context.process_id = process_id;
                        context.src_path = it->second->desc.exe_package().package().source_path();
                        context.dst_path = it->second->desc.exe_package().package().dest_path();
                        context.version = boost::replace_all_copy(it->second->desc.exe_package().package().version(), " ", "");
                        context.work_dir = it->second->env.workspace_path;
                        context.package = context.work_dir + "/" + context.process_id
                                          + "." + context.version +  ".tar.gz";
                    } else {
                        context.process_id = process_id;
                        context.src_path = it->second->desc.data_package().packages(i - 1).source_path();
                        context.dst_path = it->second->desc.data_package().packages(i - 1).dest_path();
                        context.version = boost::replace_all_copy(it->second->desc.data_package().packages(i - 1).version(), " ", "");
                        context.work_dir = it->second->env.workspace_path;
                        context.package = context.work_dir + "/" + context.process_id
                                          + "." + context.version +  ".tar.gz";
                    }

                    process_manager_.RecreateProcess(env, &context);
                    process_status = proto::kProcessRunning;
                } else {
                    it->second->status = proto::kTaskFailed;
                    process_status = process.status;
                }
                break;
            }

            if (process.status < process_status) {
                process_status = process.status;
            }
        }

        if (process_status == proto::kProcessFinished) {
            LOG(INFO) << "deploy all processes successed";
            it->second->status = proto::kTaskStarting;
        }

        break;
    }

    case proto::kTaskRunning: {
        process_id = task_id + "_main";

        if (0 != process_manager_.QueryProcess(process_id, process)) {
            LOG(WARNING) << "query main process: " << process_id << " fail";
            return -1;
        }

        do {
            if (process.status == proto::kProcessRunning) {
                break;
            }

            if (0 == process.exit_code) {
                it->second->status = proto::kTaskFinished;
                LOG(INFO) << "main process finished successful";
                break;
            }

            if (it->second->fail_retry_times < FLAGS_task_manager_task_max_fail_retry_times) {
                LOG(INFO) << "task: " << task_id << " failed and retry";
                it->second->fail_retry_times++;
                if (0 == DoStartTask(task_id)) {
                    break;
                }
            }

            LOG(INFO) << "task: " << task_id << " failed no retry";
            it->second->status = proto::kTaskFailed;
        } while (0);

        break;
    }

    case proto::kTaskStopping: {
        process_id = task_id + "_stop";

        if (0 != process_manager_.QueryProcess(process_id, process)) {
            LOG(WARNING) << "query stop process: " << process_id << " fail";
            return -1;
        }

        // stop process has over
        if (process.status != proto::kProcessRunning) {
            // when task has no stop_cmd, then return -1
            if (!it->second->desc.exe_package().has_stop_cmd()
                    || it->second->desc.exe_package().stop_cmd() == "") {
                LOG(INFO) << "task has no stop cmd, ignore main process";
                return -1;
            }

            // if main_process run over, return -1
            std::string main_process_id = task_id + "_main";
            Process main_process;
            if (0 != process_manager_.QueryProcess(main_process_id, main_process)) {
                LOG(WARNING) << "query main process: " << main_process_id << " fail";
                return -1;
            }
            if (main_process.status != proto::kProcessRunning) {
                LOG(INFO) << "stop main process ok";
                return -1;
            }
            // wait main_process for timeout after stop_process has already run over
            int32_t now_time = common::timer::now_time();
            if (it->second->timeout_point < now_time) {
                LOG(WARNING) << "stop main process timeout";
                return -1;
            }
        } else {
            int32_t now_time = common::timer::now_time();

            if (it->second->timeout_point < now_time) {
                LOG(WARNING) << "stop process timeout";
                return -1;
            }
        }

        break;
    }

    default:
        break;
    }

    task.task_id = it->second->task_id;
    task.desc.CopyFrom(it->second->desc);
    task.status = it->second->status;
    task.fail_retry_times = it->second->fail_retry_times;

    return 0;
}

int TaskManager::CleanTask(const std::string& task_id) {
    MutexLock lock(&mutex_);
    LOG(INFO) << "clean task: " << task_id;
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    std::string process_id;

    switch (it->second->prev_status) {
    case proto::kTaskDeploying:
        LOG(INFO) << "clean deploying task";

        for (int i = 0; i < it->second->packages_size; i++) {
            process_id = task_id + "_deploy_"\
                         + boost::lexical_cast<std::string>(i);
            process_manager_.KillProcess(process_id);
        }

        break;

    case proto::kTaskRunning:
        LOG(INFO) << "clean running task";
        process_id = task_id + "_main";
        process_manager_.KillProcess(process_id);
        break;

    default:
        break;
    }

    it->second->status = proto::kTaskTerminated;

    return 0;
}

int TaskManager::ClearTasks() {
    MutexLock lock(&mutex_);
    LOG(INFO) << "clear all tasks";
    tasks_.clear();

    process_manager_.ClearProcesses();

    return 0;
}

void TaskManager::StartLoops() {
    MutexLock lock(&mutex_);
    process_manager_.StartLoops();
}

void TaskManager::PauseLoops() {
    MutexLock lock(&mutex_);
    process_manager_.PauseLoops();
}

int TaskManager::DumpTasks(proto::TaskManager* task_manager) {
    MutexLock lock(&mutex_);
    // dump tasks
    std::map<std::string, Task*>::iterator it = tasks_.begin();
    for (; it != tasks_.end(); ++it) {
        Task* t = it->second;
        proto::Task* task = task_manager->add_tasks();
        task->set_task_id(t->task_id);
        task->mutable_desc()->CopyFrom(t->desc);
        task->set_status(t->status);
        task->set_prev_status(t->prev_status);
        task->set_reload_status(t->reload_status);
        task->set_packages_size(t->packages_size);
        task->set_fail_retry_times(it->second->fail_retry_times);
        task->set_check_timeout_point(it->second->check_timeout_point);

        // env
        proto::TaskEnv* env = task->mutable_env();
        TaskEnv e = t->env;
        env->set_user(e.user);
        env->set_job_id(e.job_id);
        env->set_pod_id(e.pod_id);
        env->set_task_id(e.task_id);
        // env cgroup_subsystems
        std::vector<std::string>::iterator cs_it = e.cgroup_subsystems.begin();
        for (; cs_it != e.cgroup_subsystems.end(); ++cs_it) {
            std::string* cgroup_subsystem = env->add_cgroup_subsystems();
            *cgroup_subsystem = *cs_it;
        }
        // env cgroup_paths
        std::map<std::string, std::string>::iterator cp_it = \
            e.cgroup_paths.begin();
        for (; cp_it != e.cgroup_paths.end(); ++cp_it) {
            proto::CgroupPath* cgroup_path = env->add_cgroup_paths();
            cgroup_path->set_cgroup(cp_it->first);
            cgroup_path->set_path(cp_it->second);
        }
        // env ports
        std::map<std::string, std::string>::iterator pit = e.ports.begin();
        for (; pit != e.ports.end(); ++pit) {
            proto::Port* port = env->add_ports();
            port->set_name(pit->first);
            port->set_port(pit->second);
        }
        // workspace_path
        env->set_workspace_path(e.workspace_path);
        env->set_workspace_abspath(e.workspace_abspath);
    }

    // dump process
    process_manager_.DumpProcesses(task_manager->mutable_process_manager());

    return 0;
}

int TaskManager::LoadTasks(const proto::TaskManager& task_manager) {
    MutexLock lock(&mutex_);

    for (int i = 0; i < task_manager.tasks().size(); ++i) {
        const proto::Task t = task_manager.tasks(i);
        Task* task = new Task();
        if (t.has_task_id()) {
            task->task_id = t.task_id();
        }
        if (t.has_desc()) {
            task->desc.CopyFrom(t.desc());
        }
        if (t.has_status()) {
            task->status = t.status();
        }
        if (t.has_prev_status()) {
            task->prev_status = t.prev_status();
        }
        if (t.has_reload_status()) {
            task->reload_status = t.reload_status();
        }
        if (t.has_packages_size()) {
            task->packages_size = t.packages_size();
        }
        if (t.has_fail_retry_times()) {
            task->fail_retry_times = t.fail_retry_times();
        }
        if (t.has_timeout_point()) {
            task->timeout_point = t.timeout_point();
        }
        if (t.has_check_timeout_point()) {
            task->check_timeout_point = t.check_timeout_point();
        }
        TaskEnv& env = task->env;
        if (t.has_env()) {
            const proto::TaskEnv& e = t.env();
            if (e.has_user()) {
                env.user = e.user();
            }
            if (e.has_job_id()) {
                env.job_id = e.job_id();
            }
            if (e.has_pod_id()) {
                env.pod_id = e.pod_id();
            }
            if (e.has_task_id()) {
                env.task_id = e.task_id();
            }
            if (e.has_workspace_path()) {
                env.workspace_path = e.workspace_path();
            }
            if (e.has_workspace_abspath()) {
                env.workspace_abspath = e.workspace_abspath();
            }
            // env cgroup_subsystems
            for (int j = 0; j < e.cgroup_subsystems().size(); ++j) {
                env.cgroup_subsystems.push_back(e.cgroup_subsystems(i));
            }
            // env cgroup_paths
            for (int j = 0; j < e.cgroup_paths().size(); ++j) {
                const proto::CgroupPath& cgroup_path = e.cgroup_paths(j);
                env.cgroup_paths.insert(std::make_pair(
                            cgroup_path.cgroup(),
                            cgroup_path.path()));
            }
            // env ports
            for (int j = 0; j < e.ports().size(); ++j) {
                const proto::Port& port = e.ports(j);
                env.ports.insert(std::make_pair(port.name(), port.port()));
            }
        }

        tasks_.insert(std::make_pair(task->task_id, task));
    }

    // dump process manager
    if (task_manager.has_process_manager()) {
        process_manager_.LoadProcesses(task_manager.process_manager());
    }

    return 0;
}

int TaskManager::QueryTaskStatus(const std::string& task_id, TaskStatus& task_status) {
    MutexLock lock(&mutex_);
    LOG(INFO) << "query task status: " << task_id;
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    task_status = it->second->status;

    return 0;
}

int TaskManager::DeployReloadTask(const std::string& task_id,
                                  const TaskDescription& task_desc) {
    MutexLock lock(&mutex_);
    LOG(INFO) << "reload deploy task: " << task_id;
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    // 1.replace desc
    Task* task = it->second;
    task->desc.CopyFrom(task_desc);
    LOG(INFO) << "task desc replaced ok, task: " << task_id;

    // 2.deploy data packages
    LOG(INFO) << "reload deplay task start, task: " << task_id;
    task->packages_size = 0;

    if (task->desc.has_data_package()) {
        for (int i = 0; i < task->desc.data_package().packages_size(); i++) {
            DownloadProcessContext context;
            context.process_id = task->task_id + "_reload_deploy_"\
                                 + boost::lexical_cast<std::string>(i + 1);
            context.src_path = task->desc.data_package().packages(i).source_path();
            context.dst_path = task->desc.data_package().packages(i).dest_path();
            context.version = boost::replace_all_copy(task->desc.data_package().packages(i).version(), " ", "");
            context.work_dir = task->env.workspace_path;
            context.package = context.work_dir + "/" + context.process_id
                                  + "." + context.version +  ".tar.gz";

            // process env
            ProcessEnv env;
            MakeProcessEnv(task, env);

            if (0 != process_manager_.CreateProcess(env, &context)) {
                LOG(WARNING)
                        << "command execute fail, "
                        << "command: " << context.cmd << ", "
                        << "dir: " << context.work_dir;
                return -1;
            }
        }
    }

    task->reload_status = proto::kTaskDeploying;

    return 0;
}

int TaskManager::StartReloadTask(const std::string& task_id) {
    MutexLock lock(&mutex_);
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    LOG(INFO) << "start create reload main process for task: " << task_id;
    Task* task = it->second;

    if (task->desc.has_data_package()) {
        ProcessContext context;
        context.process_id = task->task_id + "_reload_main";
        context.cmd = task->desc.data_package().reload_cmd();
        context.work_dir = task->env.workspace_path;

        ProcessEnv env;
        MakeProcessEnv(task, env);

        if (0 != process_manager_.CreateProcess(env, &context)) {
            LOG(WARNING)
                    << "command execute fail, "
                    << "command: " << context.cmd << ", "
                    << "dir: " << context.work_dir;
            return -1;
        }

        task->reload_status = proto::kTaskRunning;
    } else {
        task->reload_status = proto::kTaskFinished;
    }

    return 0;
}

// check reload task status
int TaskManager::CheckReloadTask(const std::string& task_id,
                                 Task& task) {
    MutexLock lock(&mutex_);
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    std::string process_id;
    Process process;

    switch (it->second->reload_status) {
    case proto::kTaskDeploying: {
        ProcessStatus process_status = proto::kProcessFinished;

        for (int i = 0; i < it->second->packages_size; i++) {
            process_id = task_id + "_reload_deploy_" + boost::lexical_cast<std::string>(i + 1);

            if (0 != process_manager_.QueryProcess(process_id, process)) {
                LOG(WARNING) << "query deploy process: " << process_id << " fail";
                it->second->reload_status = proto::kTaskFailed;
                return -1;
            }

            if (process.status > proto::kProcessFinished) {
                LOG(WARNING) << "deploy process " << process_id << " failed";
                it->second->reload_status = proto::kTaskFailed;
                process_status = process.status;
                break;
            }

            if (process.status < process_status) {
                process_status = process.status;
            }
        }

        if (process_status == proto::kProcessFinished) {
            LOG(WARNING) << "deploy all processes successed";
            it->second->reload_status = proto::kTaskStarting;
        }

        break;
    }

    case proto::kTaskRunning: {
        process_id = task_id + "_reload_main";

        if (0 != process_manager_.QueryProcess(process_id, process)) {
            LOG(WARNING) << "query reload main process: " << process_id << " fail";
            return -1;
        }

        do {
            if (process.status == proto::kProcessRunning) {
                break;
            }

            if (0 == process.exit_code) {
                it->second->reload_status = proto::kTaskFinished;
                LOG(INFO) << "main process finished successful";
                break;
            }

            it->second->reload_status = proto::kTaskFailed;
        } while (0);

        break;
    }

    default:
        break;
    }

    task.reload_status = it->second->reload_status;

    return 0;
}

int TaskManager::StartTaskHealthCheck(const std::string& task_id) {
    MutexLock lock(&mutex_);
    LOG(INFO) << "start task health check, task: " << task_id;
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    LOG(INFO) << "start create health check process for task: " << task_id;
    Task* task = it->second;

    if (task->desc.has_exe_package()
            && task->desc.exe_package().has_health_cmd()
            && task->desc.exe_package().health_cmd() != "") {
        ProcessContext context;
        context.process_id = task->task_id + "_check";
        context.cmd = task->desc.exe_package().health_cmd();
        context.work_dir = task->env.workspace_path;

        ProcessEnv env;
        MakeProcessEnv(task, env);

        if (0 != process_manager_.CreateProcess(env, &context)) {
            LOG(WARNING) << "command execute fail, command: " << context.cmd;
            return -1;
        }

        task->check_timeout_point = common::timer::now_time()\
                                    + FLAGS_task_manager_task_check_command_timeout;
    }

    return 0;
}

int TaskManager::CheckTaskHealth(const std::string& task_id, Task& task) {
    MutexLock lock(&mutex_);
    std::map<std::string, Task*>::iterator it = tasks_.find(task_id);

    if (it == tasks_.end()) {
        LOG(WARNING) << "task: " << task_id << " not exist";
        return -1;
    }

    LOG(INFO) << "check health task, task: " << task_id;

    if (!it->second->desc.has_exe_package()
            || !it->second->desc.exe_package().has_health_cmd()
            || it->second->desc.exe_package().health_cmd() == "") {
        task.status = proto::kTaskFinished;
        return 0;
    }

    std::string process_id = task_id + "_check";
    Process process;

    if (0 != process_manager_.QueryProcess(process_id, process)) {
        LOG(WARNING) << "query health process: " << process_id << " fail";
        return -1;
    }

    if (process.status == proto::kProcessRunning) {
        int32_t now_time = common::timer::now_time();
        if (it->second->check_timeout_point < now_time) {
            LOG(INFO) << "check command timeout";
            process_manager_.KillProcess(process_id);
            task.status = proto::kTaskFailed;
        } else {
            task.status = proto::kTaskRunning;
        }
    } else {
        if (0 == process.exit_code) {
            task.status = proto::kTaskFinished;
            LOG(INFO) << "check process finished";
        } else {
            task.status = proto::kTaskFailed;
            LOG(INFO) << "check process failed";
        }
    }

    return 0;
}

int TaskManager::ClearTaskHealthCheck(const std::string& task_id) {
    MutexLock lock(&mutex_);

    LOG(INFO) << "clear task health check, task: " << task_id;
    std::string process_id = task_id + "_check";
    process_manager_.KillProcess(process_id);

    return 0;
}

int MakeProcessEnv(Task* task, ProcessEnv& env) {
    env.user = task->env.user;
    // set env PATH
    std::string path = "/usr/local/bin:/bin:/usr/bin";
    char* c_path = getenv("PATH");
    if (NULL != c_path) {
        path = std::string(c_path);
    }
    path += ":.";
    env.envs.push_back("PATH=" + path);

    env.envs.push_back("GALAXY_JOB_ID=" + task->env.job_id);
    env.envs.push_back("GALAXY_POD_ID=" + task->env.pod_id);
    env.envs.push_back("GALAXY_TASK_ID=" + task->env.task_id);
    env.envs.push_back("GALAXY_ABS_PATH=" + task->env.workspace_abspath);
    std::map<std::string, std::string>::iterator p_it = task->env.ports.begin();

    for (; p_it != task->env.ports.end(); ++p_it) {
        env.envs.push_back("GALAXY_PORT_" + p_it->first + "=" + p_it->second);
    }

    std::map<std::string, std::string>::iterator c_it = \
            task->env.cgroup_paths.begin();

    for (; c_it != task->env.cgroup_paths.end(); ++c_it) {
        env.cgroup_paths.push_back(c_it->second);
    }

    return 0;
}

}   // ending namespace galaxy
}   // ending namespace baidu
