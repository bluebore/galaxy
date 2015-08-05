// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#include "agent/task_manager.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include "common/logging.h"
#include "agent/cgroup.h"

#include "agent/utils.h"
#include <gflags/gflags.h>

DECLARE_string(container);
DECLARE_string(cgroup_root);
DECLARE_string(agent_work_dir);

namespace galaxy {

const std::string META_PATH = "/meta/";
const std::string META_FILE_PREFIX = "meta_";

bool TaskManager::Init() {
    const int MKDIR_MODE = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

    if (access(FLAGS_agent_work_dir.c_str(), F_OK) != 0) {
        if (mkdir(FLAGS_agent_work_dir.c_str(), MKDIR_MODE) != 0) {
            LOG(WARNING, "mkdir data failed %s err[%d: %s]",
                    FLAGS_agent_work_dir.c_str(),
                    errno,
                    strerror(errno)); 
            return false;
        }
    }
    m_task_meta_dir = FLAGS_agent_work_dir + "/" + META_PATH;
    if (access(m_task_meta_dir.c_str(), F_OK) != 0) {
        if (mkdir(m_task_meta_dir.c_str(), MKDIR_MODE) != 0) {
            LOG(WARNING, "mkdir data failed %s err[%d: %s]",
                    m_task_meta_dir.c_str(),
                    errno,
                    strerror(errno)); 
            return false;
        }         
        return true;
    }

    return true;
}

int TaskManager::Add(const ::galaxy::TaskInfo& task_info,
                     DefaultWorkspace *  workspace, bool download) {
    MutexLock lock(m_mutex);
    LOG(INFO, "add task with id %d", task_info.task_id());
    if (m_task_runner_map.find(task_info.task_id()) != m_task_runner_map.end()) {
        LOG(WARNING, "task with id %d has exist", task_info.task_id());
        return 0;
    }
    // do download
    TaskInfo my_task_info(task_info);
    TaskRunner* runner = NULL;
    std::string persistence_path = FLAGS_agent_work_dir 
        + "/" + META_PATH 
        + "/" + META_FILE_PREFIX + boost::lexical_cast<std::string>(task_info.task_id());

    const int MKDIR_MODE = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    if (mkdir(persistence_path.c_str(), MKDIR_MODE) != 0
            && errno != EEXIST) {
        LOG(WARNING, "mkdir data failed %s err[%d: %s]",
                persistence_path.c_str(),
                errno,
                strerror(errno));     
        return -1;
    }  
    if(FLAGS_container.compare("cgroup") == 0){
        LOG(INFO,"use cgroup task runner for task %d",task_info.task_id());
        runner = new ContainerTaskRunner(my_task_info,FLAGS_cgroup_root, workspace, rpc_client_);
    } else {
        LOG(INFO,"use command task runner for task %d",task_info.task_id());
        runner = new CommandTaskRunner(my_task_info,workspace);
    }
    // NOTE persistence path should be set before init 
    runner->PersistenceAble(persistence_path);
    if (runner->Init() != 0) {
        LOG(WARNING, "runner init failed");
        return -1;
    }
    // not run directly dead lock
    if (download) {
        runner->AsyncDownload(
                boost::bind(&TaskManager::Start, 
                    this, task_info.task_id()));
    }
    m_task_runner_map[task_info.task_id()] = runner;
    return 0;
}

int TaskManager::Start(const int64_t& task_info_id) {
    MutexLock lock(m_mutex);
    if (m_task_runner_map.find(task_info_id) == m_task_runner_map.end()) {
        LOG(WARNING, "task with id %ld does not exist", task_info_id); 
        return 0;
    }

    TaskRunner* runner = m_task_runner_map[task_info_id];
    if (NULL == runner) {
        return 0; 
    }
    int status = runner->Prepare(); 
    if (status == 0) {
        LOG(INFO, "start task %ld successfully", task_info_id); 
    }
    else {
        LOG(WARNING, "start task %ld failed", task_info_id); 
    }
    return status;
}

int TaskManager::Remove(const int64_t& task_info_id) {
    MutexLock lock(m_mutex);
    if (m_task_runner_map.find(task_info_id) == m_task_runner_map.end()) {
        LOG(WARNING, "task with id %ld does not exist", task_info_id); 
        return 0;
    }
    TaskRunner* runner = m_task_runner_map[task_info_id];
    if(NULL == runner){
        return 0;
    }
    runner->Killed();
    int status = runner->Stop();
    if(status == 0){
        LOG(INFO,"stop task %d successfully", task_info_id);
        status = runner->Clean();
        if (status != 0) {
            LOG(WARNING, "clean task %ld failed", task_info_id); 
            return status;
        }
        LOG(INFO, "clean task %ld successfully", task_info_id);
        m_task_runner_map.erase(task_info_id);

        std::string persistence_path = FLAGS_agent_work_dir 
            + "/" + META_PATH 
            + "/" + META_FILE_PREFIX 
            + boost::lexical_cast<std::string>(task_info_id);
        if (!file::Remove(persistence_path)) {
            LOG(WARNING, "task with id %ld meta dir rm failed rm %s",
                    task_info_id,
                    persistence_path.c_str());
        }
        delete runner;
    }    
    else {
        LOG(WARNING, "stop task %d failed maybe delay stoped",
                task_info_id); 
    }
    return status;
}

int TaskManager::Status(std::vector< TaskStatus >& task_status_vector, int64_t id) {
    MutexLock lock(m_mutex);
    std::map<int64_t, TaskRunner*>::iterator it;
    if (id >= 0) {
        it = m_task_runner_map.find(id);
        if (it != m_task_runner_map.end()) {
            TaskStatus status;
            status.set_task_id(id);
            it->second->Status(&status); 
            task_status_vector.push_back(status);
        }
        return 0;
    }
    it = m_task_runner_map.begin();
    for (; it != m_task_runner_map.end(); ++it) {
        TaskStatus status;
        status.set_task_id(it->first);
        it->second->Status(&status);
        task_status_vector.push_back(status);
    }
    return 0;
}

}



