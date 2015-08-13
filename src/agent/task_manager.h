// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#ifndef AGENT_TASK_MANAGER_H
#define AGENT_TASK_MANAGER_H
#include <map>
#include <stdint.h>
#include "proto/task.pb.h"
#include "proto/agent.pb.h"
#include "common/mutex.h"
#include "agent/task_runner.h"
#include "agent/workspace.h"
#include "agent/workspace_manager.h"

namespace galaxy{

class TaskManager{
public:
    TaskManager(){
        m_mutex = new common::Mutex();
    }
    ~TaskManager(){
        std::map<int64_t,TaskRunner*>::iterator it = m_task_runner_map.begin();
        for(;it!=m_task_runner_map.end();++it){
            it->second->Stop();
            delete it->second;
        }
        delete m_mutex;
    }
    bool Init();
    int Add(const ::galaxy::TaskInfo &task_info,
            DefaultWorkspace* workspace);
    int Start(const int64_t& task_info_id);
    int Remove(const int64_t& task_info_id);
    int Status(std::vector<TaskStatus>& task_status_vector, int64_t id = -1);
    // for recover 
    int SetWorkspaceForTask(const ::galaxy::TaskInfo& task_info,
            DefaultWorkspace* workspace);
    bool LoadPersistenceInfo(const TaskManagerPersistence& info,
            WorkspaceManager* wgr);
    bool DumpPersistenceInfo(TaskManagerPersistence* info);

private:
    common::Mutex * m_mutex;
    std::map<int64_t,TaskRunner*> m_task_runner_map;
    std::string m_task_meta_dir;
};
}
#endif /* !AGENT_TASK_MANAGER_H */
