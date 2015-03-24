/*
 * task_manager.h
 * Copyright (C) 2015 wangtaize <wangtaize@baidu.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef AGENT_TASK_MANAGER_H
#define AGENT_TASK_MANAGER_H
#include <map>
#include <stdint.h>
#include "proto/task.pb.h"
#include "common/mutex.h"
#include "agent/task_runner.h"
#include "agent/workspace.h"
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
    int Add(const ::galaxy::TaskInfo &task_info,
            const DefaultWorkspace &workspace);
    int Remove(const ::galaxy::TaskInfo &task_info);
    int Status(std::vector<TaskStatus> task_status_vector);
private:
    common::Mutex * m_mutex;
    std::map<int64_t,TaskRunner*> m_task_runner_map;
};
}
#endif /* !AGENT_TASK_MANAGER_H */
