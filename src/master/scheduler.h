// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: sunjunyi01@baidu.com

#ifndef GALAXY_MASTER_SHCEDULER_H_
#define GALAXY_MASTER_SHCEDULER_H_

#include <string>
#include "master_impl.h"

namespace galaxy {

class Scheduler {
public:
    virtual std::string PickAgent(const AgentLoadIndex* load_index,
                                  const JobInfo* job) = 0;
    virtual double CalcLoad(const AgentInfo* agent) = 0;
};

class LoadBalanceScheduler : public Scheduler {
public:
    std::string PickAgent(const AgentLoadIndex* load_index,
                          const JobInfo* job);
    double CalcLoad(const AgentInfo* agent);
};

} //namespace galaxy

#endif
