// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _PODS_MANAGER_H
#define _PODS_MANAGER_H

#include "agent/agent_internal_infos.h"

#include <string>
#include <vector>
#include <set>

#include "proto/galaxy.pb.h"
#include "mutex.h"
#include "thread_pool.h"
#include "agent/task_manager.h"
#include "agent/index_list.h"

namespace baidu {
namespace galaxy {

// NOTE all interface not thread safe, 
// thread safe keep by agent_impl
class PodManager {
public:
    PodManager();
    virtual ~PodManager();

    int Init();

    int AddPod(const PodInfo& info);
    int DeletePod(const std::string& pod_id);
    int UpdatePod(const std::string& pod_id, const PodInfo& info);
    int ShowPods(std::vector<PodInfo>* pods);
    int ShowPod(const std::string& pod_id, PodInfo* pod);
    int CheckPod(const std::string& pod_id);

    int ReloadPod(const PodInfo& info);
protected:
    void ReloadInitdPort(int port);
    int AllocPortForInitd(int& port);
    void ReleasePortFromInitd(int port);

    int LanuchInitd(PodInfo* info); 
    int LanuchInitdByFork(PodInfo* info);
    int CleanPodEnv(const PodInfo& pod_info);
    void DelayGarbageCollect(const std::string& workspace);
    //int LanuchInitdByFork(PodInfo* info);
    //Mutex pods_lock_;
    std::map<std::string, PodInfo> pods_;
    TaskManager* task_manager_;
    IndexList<int> initd_free_ports_;

    ThreadPool garbage_collect_thread_; 
};

}   // ending namespace galaxy
}   // ending namespace baidu

#endif  //_PODS_MANAGER_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
