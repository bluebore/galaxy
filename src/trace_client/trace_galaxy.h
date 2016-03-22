/* 
 * File:   trace_galaxy.h
 * Author: haolifei
 *
 * Created on 2016年3月21日, 下午3:34
 */

#pragma once
#include "agent/agent_internal_infos.h"

namespace baidu {
    namespace galaxy {
        class Job;
        namespace trace {
            class GalaxyAgentTracer {
            public:
                static GalaxyAgentTracer* GetInstance();
                int Setup();
                int TearDown();
                
                int TraceTaskEvent(const TaskInfo* task, 
                    const std::string& event,
                    bool inner_error,
                    int code);
                
                int TracePodStatus(const PodStatus* pod, 
                             int32_t cpu_quota, 
                             int64_t mem_quota);
                int TracePodStart(const std::string& job, const std::string& pod);
                int TracePodFinished(const std::string& pod, int errcode);
                
            private:
                GalaxyAgentTracer();
                static GalaxyAgentTracer* s_instance;
            };
            
            
            class GalaxyMasterTracer {
            public:
                static GalaxyMasterTracer* GetInstance();
                int Setup();
                int TearDown();
                
                // DONE
                int TraceJobEvent(const std::string& job, const std::string& event);
                
                // DONE
                int TraceJob(const Job* job);
                int TraceJobStop(const std::string& podid, int code);
                
                // DONE
                int TraceAgentEvent(const AgentInfo* ai, const std::string& event, int code);
                
                // TODO
                int TraceAgentStatus(const AgentInfo* ai); 
            private:
                GalaxyMasterTracer();
                static GalaxyMasterTracer* s_instance;
                
            };
            
        }
    }
}
