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



}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
