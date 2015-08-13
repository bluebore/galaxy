// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#include "agent/resource_manager.h"
#include "common/logging.h"
#include "agent/dynamic_resource_scheduler.h"

namespace galaxy{

int ResourceManager::Allocate(const TaskResourceRequirement& requirement,
                              int64_t task_id){
    common::MutexLock lock(&mutex_);
    LOG(INFO,"task %ld request resource cpu %f mem %ld",
             task_id,requirement.cpu_limit,requirement.mem_limit);
    if(!CanAllocate(requirement,task_id)){
        LOG(WARNING,"no enough resource to allocate resource for %ld",task_id);
        return -1;
    }
    //remove task exist on the agent
    Remove(task_id);

    resource_.the_left_cpu -= requirement.cpu_limit;
    resource_.the_left_mem -= requirement.mem_limit;

    // dynamic scheduler to release
    DynamicResourceScheduler* scheduler = GetDynamicResourceScheduler();
    if (scheduler->Allocate(requirement) != 0) {
        LOG(WARNING, "no enough resource in scheduler require %lf",
                requirement.cpu_limit);
        return -1; 
    }
     
    TaskResourceRequirement req(requirement);
    task_req_map_[task_id] = req;
    return 0;
}

void ResourceManager::Free(int64_t task_id){
    common::MutexLock lock(&mutex_);
    Remove(task_id);
}


void ResourceManager::Status(AgentResource* resource){
    common::MutexLock lock(&mutex_);
    resource->total_cpu = resource_.total_cpu;
    resource->total_mem = resource_.total_mem;
    resource->the_left_cpu = resource_.the_left_cpu;
    resource->the_left_mem = resource_.the_left_mem;
}


bool ResourceManager::CanAllocate(const TaskResourceRequirement& requirement,
                                 int64_t task_id){
    mutex_.AssertHeld();
    double the_left_cpu = resource_.the_left_cpu;
    int64_t the_left_mem = resource_.the_left_mem;
    std::map<int64_t,TaskResourceRequirement>::iterator it = task_req_map_.find(task_id);
    if(it != task_req_map_.end()){
         the_left_cpu += it->second.cpu_limit;
         the_left_mem += it->second.mem_limit;
    }
    if(the_left_cpu < requirement.cpu_limit || the_left_mem < requirement.mem_limit){
        return false;
    }
    return true;
}

void ResourceManager::Remove(int64_t task_id){
    mutex_.AssertHeld();
    std::map<int64_t,TaskResourceRequirement>::iterator it = task_req_map_.find(task_id);
    if(it == task_req_map_.end()){
        return;
    }

    resource_.the_left_cpu += it->second.cpu_limit;
    resource_.the_left_mem += it->second.mem_limit;

    // dynamic scheduler to release
    DynamicResourceScheduler* scheduler 
        = GetDynamicResourceScheduler();
    scheduler->Release(it->second);

    task_req_map_.erase(it);
}

bool ResourceManager::DumpPersistenceInfo(
        ResourceManagerPersistence* info) {
    if (info == NULL)
        return false;

    common::MutexLock lock(&mutex_);
    info->set_total_cpu(resource_.total_cpu); 
    info->set_total_mem(resource_.total_mem);
    info->set_left_cpu(resource_.the_left_cpu);
    info->set_left_mem(resource_.the_left_mem);

    std::map<int64_t, TaskResourceRequirement>::iterator it 
        = task_req_map_.begin();
    for (; it != task_req_map_.end(); ++it) {
        TaskResourceRequirement& require = it->second; 
        ResourceRequirePersistence* p_require = info->add_resource_requires();
        p_require->set_id(it->first);
        p_require->set_cpu_limit(require.cpu_limit);
        p_require->set_mem_limit(require.mem_limit);
    }
    return true;
}

bool ResourceManager::LoadPersistenceInfo(
        const ResourceManagerPersistence& info) {
    common::MutexLock lock(&mutex_);
    if (!info.has_total_cpu()
            || !info.has_total_mem()
            || !info.has_left_cpu()
            || !info.has_left_mem()) {
        return false; 
    }

    resource_.total_cpu = info.total_cpu();
    resource_.total_mem = info.total_mem();
    resource_.the_left_cpu = info.left_cpu();
    resource_.the_left_mem = info.left_mem();

    for (int i = 0; i < info.resource_requires_size(); i++) {
        const ResourceRequirePersistence& p_require 
            = info.resource_requires(i);
        TaskResourceRequirement require;
        require.cpu_limit = p_require.cpu_limit();
        require.mem_limit = p_require.mem_limit();
        task_req_map_[p_require.id()] = require;
    }
    return true;
}

}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
