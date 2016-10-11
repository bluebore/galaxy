// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_CLIENT_JOB_ACTION_H
#define BAIDU_GALAXY_CLIENT_JOB_ACTION_H

#include "galaxy_util.h"
#include <gflags/gflags.h>
#include "sdk/galaxy_sdk_appmaster.h"
#include "sdk/galaxy_sdk_resman.h"

namespace baidu {
namespace galaxy {
namespace client {

class JobAction {

public:
    JobAction();
    ~JobAction();
    bool SubmitJob(const std::string& json_file);
    bool UpdateJob(const std::string& json_file, const std::string& jobid, 
                        const std::string& operation, uint32_t update_break_count);
    bool StopJob(const std::string& jobid);
    bool RemoveJob(const std::string& jobid);
    bool ListJobs(const std::string& soptions);
    bool ShowJob(const std::string& jobid, const std::string& soptions, bool show_meta);
    bool RecoverInstance(const std::string& jobid, const std::string& podid);
    bool ExecuteCmd(const std::string& jobid, const std::string& cmd);
    bool CalRes(const std::string& json_file, const std::string& soptions);
    bool GenerateJson(const std::string& jobid);

private:
    bool Init();
    bool InitResman();
    std::string StringUnit(int64_t num);
    static void* ListContainers(void* param);
    static void* ListJobs(void* param);
    static void* ShowJob(void* param);
    static void* ShowContainerGroup(void* param);
    //-1 cpu not enough
    //-2 memory not enough
    //-3 disk not enough
    int CalResPolicy(int64_t need_cpu, int64_t need_mem, 
               const ::baidu::galaxy::sdk::VolumRequired& workspace_volum,
               const std::vector< ::baidu::galaxy::sdk::VolumRequired>& data_volums,
               const ::baidu::galaxy::sdk::AgentStatistics& agent,
               int* max_per_host);
private:
    ::baidu::galaxy::sdk::AppMaster* app_master_;
    ::baidu::galaxy::sdk::User user_;
    ::baidu::galaxy::sdk::ResourceManager* resman_;
}; // end class JobAction

} // end namespace client
} // end namespace galaxy
} // end namespace baidu

#endif  // BAIDU_GALAXY_CLIENT_JOB_ACTION_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
