// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_SCHEDULER_IO_H
#define BAIDU_GALAXY_SCHEDULER_IO_H


#include <map>
#include <string>
#include <assert.h>
#include "proto/master.pb.h"
#include "scheduler/scheduler.h"
#include "master/master_watcher.h"
#include "rpc/rpc_client.h"
#include <mutex.h>
#include <thread_pool.h>
namespace baidu {
namespace galaxy {
class SchedulerIO {

public:
    SchedulerIO() : rpc_client_(),
                    master_stub_(NULL),
                    scheduler_(),
                    thread_pool_(4){
    }
    ~SchedulerIO(){
        thread_pool_.Stop(false);
    }
    void HandleMasterChange(const std::string& new_master_endpoint);
    bool Init();
    void Sync();
private:
    void SyncPendingJob();
    void SyncPendingJobCallBack(const GetPendingJobsRequest* req,
                          GetPendingJobsResponse* response,
                          bool failed, int);

    void SyncResources();
    void SyncJobDescriptor();
    void SyncResourcesCallBack(const GetResourceSnapshotRequest* request,
                               GetResourceSnapshotResponse* response,
                               bool failed, int);
    void SyncJobDescriptorCallBack(const GetJobDescriptorRequest* request,
                                   GetJobDescriptorResponse* response,
                                   bool failed, int);
 
private:
    std::string master_addr_;
    RpcClient rpc_client_;
    Master_Stub* master_stub_;
    Scheduler scheduler_;
    MasterWatcher master_watcher_;
    Mutex master_mutex_;
    ThreadPool thread_pool_;
};

} // galaxy
}// baidu
#endif
