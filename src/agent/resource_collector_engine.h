// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#ifndef _RESOURCE_COLLECTOR_ENGINE_H
#define _RESOURCE_COLLECTOR_ENGINE_H

#include <map>
#include "common/thread_pool.h"
#include "common/mutex.h"
#include "agent/resource_collector.h"

namespace galaxy {

class ResourceCollectorEngine;
ResourceCollectorEngine* GetResourceCollectorEngine();

class ResourceCollectorEngine {
public:
    explicit ResourceCollectorEngine(int interval) 
        : collector_thread_(NULL),
          collect_interval_(interval),
          collector_set_(),
          next_collector_id_(0L),
          collector_set_lock_() {
        collector_thread_ = new ThreadPool(1); 
    }   

    virtual ~ResourceCollectorEngine() {
        delete collector_thread_; 
    }

    long AddCollector(ResourceCollector* collector, int collect_interval = 0);
    void DelCollector(long collector_id);

    void Start();
    bool Stop() {
        return collector_thread_->Stop(false); 
    }
protected:
    void OnTimeout();

    struct CollectCell {
        ResourceCollector* collector_handler;  
        long next_collect_time;
        long collector_id;
        int collect_interval;
        CollectCell() : collector_handler(NULL),
            next_collect_time(0),
            collector_id(0),
            collect_interval(0) {
        }
    };

    common::ThreadPool* collector_thread_;
    int collect_interval_;
    std::map<long, CollectCell> collector_set_;
    //std::map<long, ResourceCollector*> collector_set_;
    // TODO change id generator
    long next_collector_id_;
    common::Mutex collector_set_lock_;
};

}   // ending namespace galaxy

#endif  //_RESOURCE_COLLECTOR_ENGINE_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
