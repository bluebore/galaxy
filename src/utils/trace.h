// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_TRACE_H
#define BAIDU_GALAXY_TRACE_H
#include <google/protobuf/message.h>
#include "proto/log.pb.h"
#include "proto/galaxy.pb.h"
#include "master/job_manager.h"
#include "agent/agent_internal_infos.h"

namespace baidu {
namespace galaxy {

class Trace {

public:
    static void TraceJobEvent(const TraceLevel& level, 
                              const std::string& action, 
                              const Job* job);

    static void TracePodEvent(const TraceLevel& level,
                              const PodStatus* pod, 
                              const std::string& from,
                              const std::string& reason);
    static void TraceClusterStat(ClusterStat& stat);
    static void TraceJobStat(const Job* job);
    static void TracePodStat(const PodStatus* pod, 
                             int32_t cpu_quota, 
                             int64_t mem_quota);
    static void TraceTaskEvent(const TraceLevel& level,
                               const TaskInfo* task, 
                               const std::string& error,
                               const TaskState& status,
                               bool internal_error,
                               int32_t exit_code);
    static void TraceAgentEvent(AgentEvent& e);
    static void Log(::google::protobuf::Message& msg, const std::string& pk);
    static void Init();
    static std::string GenUuid();
    Trace(){}
    ~Trace(){}
};

}
}
#endif


