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
#include "common/mutex.h"
#include "agent/task_runner.h"
#include "agent/workspace.h"
#include "rpc/rpc_client.h"
namespace galaxy{

class TaskManager{
public:
    TaskManager(){
        m_mutex = new common::Mutex();
        rpc_client_ = new RpcClient();
    }
    ~TaskManager(){
        std::map<int64_t,TaskRunner*>::iterator it = m_task_runner_map.begin();
        for(;it!=m_task_runner_map.end();++it){
            //it->second->Stop();
            delete it->second;
        }
        delete m_mutex;
        delete rpc_client_;
    }
    bool Init();
    int Add(const ::galaxy::TaskInfo &task_info,
            DefaultWorkspace* workspace,
            bool download = true);
    int Start(const int64_t& task_info_id);
    int Remove(const int64_t& task_info_id);
    int Status(std::vector<TaskStatus>& task_status_vector, int64_t id = -1);

private:
    common::Mutex * m_mutex;
    std::map<int64_t,TaskRunner*> m_task_runner_map;
    std::string m_task_meta_dir;
    RpcClient* rpc_client_; 
};
}
#endif /* !AGENT_TASK_MANAGER_H */
