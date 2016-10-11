// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_SDK_APPMASTER_H
#define BAIDU_GALAXY_SDK_APPMASTER_H

#include "galaxy_sdk.h"

namespace baidu {
namespace galaxy {
namespace sdk {

class AppMaster {
public:
    static AppMaster* ConnectAppMaster(const std::string& nexus_addr, const std::string& path);
    virtual ~AppMaster() = 0;
    virtual bool SubmitJob(const SubmitJobRequest& request, SubmitJobResponse* response) = 0;
    virtual bool UpdateJob(const UpdateJobRequest& request, UpdateJobResponse* response) = 0;
    virtual bool StopJob(const StopJobRequest& request, StopJobResponse* response) = 0;
    virtual bool RemoveJob(const RemoveJobRequest& request, RemoveJobResponse* response) = 0;
    virtual bool ListJobs(const ListJobsRequest& request, ListJobsResponse* response) = 0;
    virtual bool ShowJob(const ShowJobRequest& request, ShowJobResponse* response) = 0;
    virtual bool RecoverInstance(const RecoverInstanceRequest& request, RecoverInstanceResponse* response) = 0;
    virtual bool ExecuteCmd(const ExecuteCmdRequest& request, ExecuteCmdResponse* response) = 0;
};

} //end namespace sdk
} //end namespace client
} //end namespace baidu

#endif  // BAIDU_GALAXY_SDK_APPMASTER_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
