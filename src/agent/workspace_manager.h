// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#ifndef AGENT_WORKSPACE_MANAGER_H
#define AGENT_WORKSPACE_MANAGER_H
#include <map>
#include <string>
#include <stdint.h>
#include "agent/workspace.h"
#include "common/mutex.h"
#include "common/thread_pool.h"
#include "proto/task.pb.h"
#include "proto/agent.pb.h"
namespace galaxy{

class WorkspaceManager{
public:
    WorkspaceManager(const std::string & root_path)
        : m_mutex(NULL),
          m_root_path(root_path),
          m_data_path(),
          m_gc_path(),
          m_gc_thread(NULL),
          m_gc_event() {
        m_mutex = new common::Mutex();
        m_gc_thread = new common::ThreadPool(1);
    }
    ~WorkspaceManager(){
        delete m_mutex;
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
        delete m_gc_thread;
    }
    bool Init();
    //根据task info 创建一个workspace
    int Add(const TaskInfo &task_info);
    int Remove(int64_t task_info_id, std::string* gc_path = NULL, bool delay = false);
    int DelayGC(DefaultWorkspace* workspace);
    DefaultWorkspace* GetWorkspace(const ::galaxy::TaskInfo &task_info);

    bool LoadPersistenceInfo(const WorkspaceManagerPersistence& info);

    bool DumpPersistenceInfo(WorkspaceManagerPersistence* info); 
private:
    // TODO just for a trick
    void OnGCTimeout(const std::string path);

    std::map<int64_t, DefaultWorkspace*>  m_workspace_map;
    common::Mutex * m_mutex;
    std::string m_root_path;
    std::string m_data_path;
    std::string m_gc_path;
    common::ThreadPool* m_gc_thread;
    std::map<std::string, int64_t> m_gc_event;
};

}
#endif /* !AGENT_WORKSPACE_MANAGER_H */
