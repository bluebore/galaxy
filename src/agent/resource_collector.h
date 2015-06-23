// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#ifndef _RESOURCE_COLLECTOR_H
#define _RESOURCE_COLLECTOR_H

#include "proto/task.pb.h"

namespace galaxy {

class ResourceCollector {
public:
    virtual ~ResourceCollector() {}
protected: 
    friend class ResourceCollectorEngine;
    virtual bool CollectStatistics() = 0;
};

class CGroupResourceCollectorImpl;
class CGroupResourceCollector : public ResourceCollector {
public:
    explicit CGroupResourceCollector(const std::string& cgroup_name);
    virtual ~CGroupResourceCollector();
    void ResetCgroupName(const std::string& group_name);
    void Clear();
    double GetCpuUsage();   
    long GetMemoryUsage();
    double GetCpuCoresUsage();
protected:
    virtual bool CollectStatistics();
private:
    CGroupResourceCollectorImpl* impl_;
    std::string cgroup_name_;
};

class ProcResourceCollectorImpl;
class ProcResourceCollector : public ResourceCollector {
public:
    explicit ProcResourceCollector(int pid);
    virtual ~ProcResourceCollector();
    void ResetPid(int pid);
    void Clear();
    double GetCpuUsage();
    long GetMemoryUsage();
protected:
    virtual bool CollectStatistics();
private:
    ProcResourceCollectorImpl* impl_;
};

}   // ending namespace galaxy

#endif  //_RESOURCE_COLLECTOR_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
