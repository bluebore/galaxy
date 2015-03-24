// Copyright (C) 2015, Copyright (C)


#include "agent/workspace_manager.h"

namespace galaxy{

int WorkspaceManager::Add(const ::galaxy::TaskInfo &task_info){
    m_mutex->Lock();
    if (m_workspace_map.find(task_info.task_id())!= m_workspace_map.end()){
        m_mutex->Unlock();
        return 0;
    }
    DefaultWorkspace * ws = new DefaultWorkspace(task_info,m_root_path);
    int status = ws->Create();
    if(status==0){
        m_workspace_map[task_info.task_id()] = ws;
    }
    m_mutex->Unlock();
    return status;
}

int WorkspaceManager::Remove(const ::galaxy::TaskInfo &task_info){
    m_mutex->Lock();
    if (m_workspace_map.find(task_info.task_id()) == m_workspace_map.end()){
        m_mutex->Unlock();
        return 0;
    }
    Workspace * ws = m_workspace_map[task_info.task_id()];
    if(ws != NULL){
        int status =  ws->Clean();
        if(status != 0 ){
            m_mutex->Unlock();
            return status;
        }
        m_workspace_map.erase(task_info.task_id());
        delete ws;
        m_mutex->Unlock();
        return 0;
    }
    m_mutex->Unlock();
    return -1;

}

DefaultWorkspace* WorkspaceManager::GetWorkspace(const TaskInfo &task_info){
    if (m_workspace_map.find(task_info.task_id()) == m_workspace_map.end()){
        return NULL;
    }
    return m_workspace_map[task_info.task_id()];

}
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
