/*
 * workspace_manager.h
 * Copyright (C) 2015 wangtaize <wangtaize@baidu.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef AGENT_WORKSPACE_MANAGER_H
#define AGENT_WORKSPACE_MANAGER_H
#include <map>
#include <string>
#include <stdint.h>
#include "agent/workspace.h"
#include "common/mutex.h"
#include "proto/task.pb.h"
namespace galaxy{

class WorkspaceManager{
public:
    WorkspaceManager(const std::string & root_path):m_root_path(root_path){
        m_mutex = new common::Mutex();
    }
    ~WorkspaceManager(){
        if(m_mutex!=NULL){
            delete m_mutex;
        }
        Workspace * tmp = NULL;
        std::map<int64_t, DefaultWorkspace*>::iterator map_it = m_workspace_map.begin();
        for(;map_it != m_workspace_map.end();++map_it){
            tmp = map_it->second;
            tmp->Clean();
            m_workspace_map.erase(map_it);
            if(tmp != NULL){
                delete tmp;
            }
        }
    }
    //根据task info 创建一个workspace
    int Add(const ::galaxy::TaskInfo &task_info);
    int Remove(const ::galaxy::TaskInfo &task_info);
    DefaultWorkspace* GetWorkspace(const ::galaxy::TaskInfo &task_info);
private:
    std::map<int64_t, DefaultWorkspace*>  m_workspace_map;
    common::Mutex * m_mutex;
    std::string m_root_path;
};

}
#endif /* !AGENT_WORKSPACE_MANAGER_H */
