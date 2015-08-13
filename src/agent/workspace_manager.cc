// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#include "agent/workspace_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <boost/bind.hpp>
#include <pwd.h>
#include <sstream>

#include "common/logging.h"
#include "agent/utils.h"
#include <gflags/gflags.h>

DECLARE_int32(agent_gc_timeout);
DECLARE_string(task_acct);

namespace galaxy {

const std::string DATA_PATH = "/data/";
const std::string GC_PATH = "/gc/";

int WorkspaceManager::Add(const TaskInfo& task_info) {
    MutexLock lock(m_mutex);
    TaskInfo my_task_info(task_info);
    if (m_workspace_map.find(my_task_info.task_id()) != m_workspace_map.end()) {
        return 0;
    }

    DefaultWorkspace* ws = new DefaultWorkspace(my_task_info, m_data_path);
    int status = ws->Create();

    if (status == 0) {
        m_workspace_map[my_task_info.task_id()] = ws;
    }

    return status;
}

bool WorkspaceManager::Init() {
    const int MKDIR_MODE = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    // clear work_dir and kill tasks
    m_data_path = m_root_path + DATA_PATH;
    m_gc_path  = m_root_path + GC_PATH;
    if (access(m_root_path.c_str(), F_OK) != 0) {
        if (mkdir(m_root_path.c_str(), MKDIR_MODE) != 0) {
            LOG(WARNING, "mkdir data failed %s err[%d: %s]",
                    m_root_path.c_str(), errno, strerror(errno));
            return false;
        }
        if (mkdir(m_data_path.c_str(), MKDIR_MODE) != 0) {
            LOG(WARNING, "mkdir data failed %s err[%d: %s]",
                    m_data_path.c_str(), errno, strerror(errno));
            return false;
        }
        LOG(INFO, "init workdir %s", m_data_path.c_str());
        if (mkdir(m_gc_path.c_str(), MKDIR_MODE) != 0) {
            LOG(WARNING, "mkdir gc failed %s err[%d: %s]",
                    m_gc_path.c_str(), errno, strerror(errno)); 
            return false;
        } 
        LOG(INFO, "init gcpath %s", m_gc_path.c_str());
        return true;
    }

    
    if (access(m_data_path.c_str(), F_OK) == 0) {
        if (!file::Remove(m_data_path)) {
            LOG(WARNING, "clera dirty data %s failed", m_data_path.c_str());
            return false;
        }
        LOG(INFO, "clear dirty data %s", m_data_path.c_str());
    }

    if (mkdir(m_data_path.c_str(), MKDIR_MODE) != 0) {
        LOG(WARNING, "mkdir data failed %s err[%d: %s]",
                m_data_path.c_str(), errno, strerror(errno));
        return false;
    }
    LOG(INFO, "init workdir %s", m_data_path.c_str());
    if (access(m_gc_path.c_str(), F_OK) == 0) {
        if (!file::Remove(m_gc_path)) {
            LOG(WARNING, "clear gc path %s failed", m_gc_path.c_str());
            return false;
        }
    }

    if (mkdir(m_gc_path.c_str(), MKDIR_MODE) != 0) {
        LOG(WARNING, "mkdir gcpath failed %s err[%d: %s]",
                m_gc_path.c_str(), errno, strerror(errno)); 
        return false;
    }
    LOG(INFO, "init gcpath %s", m_gc_path.c_str());

    //create acct
    passwd *pw = getpwnam(FLAGS_task_acct.c_str());
    if (NULL == pw) {
        std::stringstream add_user;
        add_user << "useradd -d /home/users/" << FLAGS_task_acct.c_str()
            << " -m " << FLAGS_task_acct.c_str();
        system(add_user.str().c_str());
        if (errno) {
            LOG(WARNING, "create acct failed %s err[%d: %s]",
                FLAGS_task_acct.c_str(), errno, strerror(errno));
            return false;
        }
    }

    return true;
}

void WorkspaceManager::OnGCTimeout(const std::string path) {
    if (!file::Remove(path)) {
        LOG(WARNING, "rm gc %s failed", path.c_str()); 
    }        
    common::MutexLock lock(m_mutex);
    m_gc_event.erase(path);
}

int WorkspaceManager::Remove(int64_t task_info_id, std::string* gc_path, bool delay) {
    MutexLock lock(m_mutex);
    if (m_workspace_map.find(task_info_id) == m_workspace_map.end()) {
        return 0;
    }

    Workspace* ws = m_workspace_map[task_info_id];

    if (ws != NULL) {

        if (!delay) {
            int status =  ws->Clean();
            if (status != 0) {
                return status;
            }
        }
        else {
            if (0 != ws->MoveTo(m_gc_path)) {
                return -1; 
            }
            int32_t now = common::timer::now_time(); 
            m_gc_event[ws->GetPath()] = now + FLAGS_agent_gc_timeout;
            m_gc_thread->DelayTask(FLAGS_agent_gc_timeout, 
                    boost::bind(&WorkspaceManager::OnGCTimeout, 
                        this, ws->GetPath()));
        }
        if (gc_path != NULL) {
            *gc_path = ws->GetPath(); 
        }

        m_workspace_map.erase(task_info_id);
        delete ws;
        return 0;
    }
    return -1;

}

DefaultWorkspace* WorkspaceManager::GetWorkspace(const TaskInfo& task_info) {
    MutexLock lock(m_mutex);
    if (m_workspace_map.find(task_info.task_id()) == m_workspace_map.end()) {
        return NULL;
    }

    return m_workspace_map[task_info.task_id()];

}

bool WorkspaceManager::LoadPersistenceInfo(const WorkspaceManagerPersistence& info) {
    MutexLock lock(m_mutex);
    if (!info.has_root_path()
            || !info.has_data_path() 
            || !info.has_gc_path()) {
        LOG(WARNING, "[PERSISTENCE] persistence info is not valid"); 
        return false;
    }
    
    m_root_path = info.root_path();
    m_data_path = info.data_path();
    m_gc_path = info.gc_path();
   
    LOG(DEBUG, "[PERSISTENCE] workspace manager load "
            "persistence info[%s:%s:%s:%ld:%ld]",
            m_root_path.c_str(), 
            m_data_path.c_str(), 
            m_gc_path.c_str(),
            info.work_paths_size(),
            info.gc_events_size());
    for (int i = 0; i < info.work_paths_size(); i++) {
        WorkspacePersistence work_info = info.work_paths(i); 
        DefaultWorkspace* ws = new DefaultWorkspace(work_info.task_info(), m_data_path);
        m_workspace_map[work_info.task_info().task_id()] = ws;
        if (!ws->LoadPersistenceInfo(work_info)) {
            return false; 
        }
    }

    if (m_gc_thread == NULL) {
        LOG(WARNING, "[PERSISTENCE] workspace manager gc thread not init yet"); 
        return false;
    }
    int32_t now = common::timer::now_time();
    for (int i = 0; i < info.gc_events_size(); i++) {
        GarbageEvent event = info.gc_events(i); 
        m_gc_event[event.garbage_path()] = event.gc_time();
        int32_t delay_time = now - event.gc_time();
        if (delay_time < 0) {
            delay_time = 0; 
        }
        m_gc_thread->DelayTask(delay_time, boost::bind(&WorkspaceManager::OnGCTimeout,
                    this, event.garbage_path()));
    }
    return true;
}

bool WorkspaceManager::DumpPersistenceInfo(WorkspaceManagerPersistence* info) {
    if (info == NULL) {
        return false; 
    }

    MutexLock lock(m_mutex);
    info->set_root_path(m_root_path); 
    info->set_data_path(m_data_path);
    info->set_gc_path(m_gc_path);
    LOG(DEBUG, "[PERSISTENCE] workspace manager "
            "dump persistence info[%s:%s:%s:%ld:%ld]",
            m_root_path.c_str(),
            m_data_path.c_str(),
            m_gc_path.c_str(),
            m_workspace_map.size(),
            m_gc_event.size());
    std::map<std::string, int64_t>::iterator gc_it 
        = m_gc_event.begin();
    for (; gc_it != m_gc_event.end(); ++gc_it) {
        GarbageEvent* gc_event_pb = info->add_gc_events();     
        gc_event_pb->set_garbage_path(gc_it->first);
        gc_event_pb->set_gc_time(gc_it->second);
    }

    std::map<int64_t, DefaultWorkspace*>::iterator work_it
        = m_workspace_map.begin();  
    for (; work_it != m_workspace_map.end(); ++work_it) {
        WorkspacePersistence* workspace_path = info->add_work_paths();
        if (!work_it->second->DumpPersistenceInfo(workspace_path)) {
            return false; 
        }
    }
    return true;
}

}   // ending namespace galaxy

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
