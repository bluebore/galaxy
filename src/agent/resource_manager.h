// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com


#ifndef AGENT_RESOURCE_MANAGER_H
#define AGENT_RESOURCE_MANAGER_H

#include <map>
#include "common/mutex.h"
#include "proto/agent.pb.h"
namespace galaxy {

//agent resource description
struct AgentResource{

    double total_cpu;
    //bytes
    int64_t total_mem;

    double the_left_cpu;

    int64_t the_left_mem;

};


//
struct TaskResourceRequirement{
    double cpu_limit;
    int64_t mem_limit;
};


class ResourceManager{

public:
    ResourceManager(const AgentResource& resource):
        resource_(resource){}
    int Allocate(const TaskResourceRequirement& requirement, int64_t task_id);
    void Free(int64_t task_Id);
    void Status(AgentResource* resource);
    bool DumpPersistenceInfo(ResourceManagerPersistence* info); 
    bool LoadPersistenceInfo(const ResourceManagerPersistence& info);
private:
    void Remove(int64_t task_id);
    bool CanAllocate(const TaskResourceRequirement& requirement,int64_t task_id);
private:
    AgentResource resource_;
    common::Mutex mutex_;
    std::map<int64_t,TaskResourceRequirement> task_req_map_;
};


}
#endif

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
