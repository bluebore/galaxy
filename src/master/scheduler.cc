// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: sunjunyi01@baidu.com

#include "scheduler.h"

#include <math.h>
#include "common/logging.h"

namespace galaxy {

double LoadBalanceScheduler::CalcLoad(const AgentInfo* agent) {
    assert(agent);
    const double tasks_count_base_line = 32.0;
    int64_t mem_used = agent->mem_used;
    double cpu_used = agent->cpu_used;
    double mem_factor = mem_used / static_cast<double>(agent->mem_share);
    double cpu_factor = cpu_used / agent->cpu_share;
    double task_count_factor = agent->running_tasks.size() / tasks_count_base_line;
    return exp(cpu_factor) + exp(mem_factor) + exp(task_count_factor);
}

std::string LoadBalanceScheduler::PickAgent(const AgentLoadIndex* load_index,
                                            const JobInfo* p_job) {
    assert(load_index);
    assert(p_job);
    std::string addr;
    const JobInfo& job = *p_job;
    cpu_left_index& cidx = boost::multi_index::get<1>((AgentLoadIndex&)*load_index);
    // NOTO first iterator value not satisfy requirement
    cpu_left_index::iterator it = cidx.lower_bound(job.cpu_share);
    cpu_left_index::iterator it_start = cidx.begin();
    cpu_left_index::iterator cur_agent;
    bool last_found = false;
    double current_min_load = 0;
    if(it != cidx.end() 
            && it->mem_left >= job.mem_share
            && it->cpu_left >= job.cpu_share){
        LOG(DEBUG, "alloc resource for job %ld list agent %s cpu left %lf mem left %ld",
                job.id,
                it->agent_addr.c_str(),
                it->cpu_left,
                it->mem_left);
        last_found = true;
        current_min_load = it->load;
        addr = it->agent_addr;
        cur_agent = it;
    }
    for(;it_start != it;++it_start){
        LOG(DEBUG, "alloc resource for job %ld list agent %s cpu left %lf mem left %ld",
                job.id,
                it_start->agent_addr.c_str(),
                it_start->cpu_left,
                it_start->mem_left);
        //判断内存是否满足需求
        if(it_start->mem_left < job.mem_share){
            continue;
        }
        //第一次赋值current_min_load;
        if(!last_found){
            current_min_load = it_start->load;
            addr = it_start->agent_addr;
            last_found = true;
            cur_agent = it;
            continue;
        }
        //找到负载更小的节点
        if(current_min_load > it_start->load){
            addr = it_start->agent_addr;
            current_min_load = it_start->load;
            cur_agent = it;
        }
    }
    if(last_found){
        LOG(INFO,"alloc resource for job %ld on host %s with load %f cpu left %f mem left %ld",
                job.id,addr.c_str(),
                current_min_load,
                cur_agent->cpu_left,
                cur_agent->mem_left);
    }else{
        LOG(WARNING,"no enough  resource to alloc for job %ld",job.id);
    }
    return addr;
}

} //namespace galaxy
