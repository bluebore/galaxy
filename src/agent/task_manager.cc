// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#include "agent/task_manager.h"

#include "common/logging.h"
#include "agent/cgroup.h"

extern std::string FLAGS_container;
namespace galaxy {
int TaskManager::Add(const ::galaxy::TaskInfo& task_info,
                     DefaultWorkspace *  workspace) {
    MutexLock lock(m_mutex);
    LOG(INFO, "add task with id %d", task_info.task_id());
    if (m_task_runner_map.find(task_info.task_id()) != m_task_runner_map.end()) {
        LOG(WARNING, "task with id %d has exist", task_info.task_id());
        return 0;
    }
    // do download

    TaskRunner* runner = NULL;
    if(FLAGS_container.compare("cgroup") == 0){
        LOG(INFO,"use cgroup task runner for task %d",task_info.task_id());
        runner = new ContainerTaskRunner(task_info,"/cgroup", workspace);
    }else{
        LOG(INFO,"use command task runner for task %d",task_info.task_id());
        runner = new CommandTaskRunner(task_info,workspace);
    }
    int ret = runner->Prepare();
    if(ret != 0 ){
        LOG(INFO,"fail to prepare runner ,ret is %d",ret);
        return ret;
    }
    m_task_runner_map[task_info.task_id()] = runner;
    //ret = runner->Start();
    //if (ret == 0) {
    //    LOG(INFO, "add task with id %d successfully", task_info.task_id());
    //    m_task_runner_map[task_info.task_id()] = runner;
    //} else {
    //    LOG(FATAL, "fail to add task with %d", task_info.task_id());
    //}
    return ret;
}

int TaskManager::Remove(const int64_t& task_info_id) {
    MutexLock lock(m_mutex);
    if (m_task_runner_map.find(task_info_id) == m_task_runner_map.end()) {
        LOG(WARNING, "task with id %d does not exist", task_info_id);
        return 0;
    }
    TaskRunner* runner = m_task_runner_map[task_info_id];
    if(NULL == runner){
        return 0;
    }
    int status = runner->Stop();
    if(status == 0){
        LOG(INFO,"stop task %d successfully",task_info_id);
    }
    m_task_runner_map.erase(task_info_id);
    delete runner;
    return status;
}

int TaskManager::Status(std::vector< TaskStatus >& task_status_vector) {
    MutexLock lock(m_mutex);
    std::map<int64_t, TaskRunner*>::iterator it = m_task_runner_map.begin();
    for (; it != m_task_runner_map.end(); ++it) {
        TaskStatus status;
        status.set_task_id(it->first);
        int ret = it->second->IsRunning();
        if(ret == 0){
            status.set_status(RUNNING);
        }else if(ret == 1){
            status.set_status(COMPLETE);
        }else{
            if (it->second->ReStart() == 0) {
                status.set_status(RESTART);
            } else {
                // if restart failed,
                // 1. retry times more than limit, no need retry any more.
                // 2. stop failed
                // 3. start failed
                status.set_status(ERROR);
            }
        }

        task_status_vector.push_back(status);
    }
    return 0;
}
}



