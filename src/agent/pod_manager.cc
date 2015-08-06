#include "pod_manager.h"
#include <boost/bind.hpp>
#include "gflags/gflags.h"
#include "proto/galaxy.pb.h"
#include "thread.h"

DECLARE_int32(agent_monitor_pods_interval);

namespace baidu {
namespace galaxy {

PodManager::PodManager() {

    monitor_thread_.reset(new common::Thread());
    monitor_thread_->Start(boost::bind(&PodManager::LoopCheckPodInfos, 
                                      this));
}

PodManager::~PodManager() {
}

int PodManager::Run(const PodDesc& pod) {
    int ret = DoPodOperation(pod, kCreate);
    return ret;
}

int PodManager::Kill(const PodDesc& pod) {
    int ret = DoPodOperation(pod, kDelete);
    return ret;
}

int PodManager::Query(const std::string& podid, 
                      boost::shared_ptr<PodInfo>& info)  {

    {
    MutexLock lock(&infos_mutex_);
    PodInfosType::iterator it = pod_infos_.find(podid);
    if (it == pod_infos_.end()) {
        // not found
        LOG(INFO, "not found pod[%s]", podid.c_str());
        return -1;
    } else {
        *info = *(it->second);
    }
    } // end lock
    return 0;
}

int PodManager::List(std::vector<std::string>* pod_ids) {
    if (pod_ids == NULL) {
        return -1;
    }

    {
    MutexLock lock(&infos_mutex_);
    for (PodInfosType::iterator it = pod_infos_.begin(); 
         it != pod_infos_.end(); ++it) {
        pod_ids->push_back(it->first);
    }
    }
    return 0;
}

void PodManager::LoopCheckPodInfos() {
    while (true) {
        PodHandlersType pod_handlers;
        {
        // copy pod handlers
        MutexLock lock(&handlers_mutex_);
        pod_handlers = pod_handlers_;
        }

        for (PodHandlersType::iterator it = pod_handlers.begin(); 
             it != pod_handlers.end(); ++it) {
            boost::shared_ptr<PodInfo> info;
            it->second->Show(info);
            // update internal pod info
            {
            MutexLock lock(&infos_mutex_);
            pod_infos_[it->first] = info;
            }
        }
        
        sleep(FLAGS_agent_monitor_pods_interval * 1000L);
    }
}

int PodManager::DoPodOperation(const PodDesc& pod, 
                               const Operation op) {
    int ret = 0; 
    boost::shared_ptr<InitdHandler> handler;
    {
    MutexLock lock(&handlers_mutex_);
    PodHandlersType::iterator it = pod_handlers_.find(pod.id);
    
    // not found
    if (it == pod_handlers_.end()) {
        handler.reset(new InitdHandler());
        pod_handlers_[pod.id] = handler;
    } else {
        handler = it->second;
    }
    }

    switch (op) {
    case kCreate:
        ret = handler->Create(pod);
        break;
    case kDelete:
        ret = handler->Delete();
        break;
    default:
        ret = kUnknown;
        break;
    }
    return ret;
}

int PodManager::FesibilityCheck(const Resource& resource) {
    return 0; 
}

}
}
