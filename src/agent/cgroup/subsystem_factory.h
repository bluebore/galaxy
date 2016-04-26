// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#include "subsystem.h"

#include <boost/shared_ptr.hpp>
#include <boost/core/noncopyable.hpp>

#include <map>
#include <vector>

namespace baidu {
namespace galaxy {
namespace cgroup {

class SubsystemFactory : public boost::noncopyable {
public:
    SubsystemFactory* Register(Subsystem* malloc_subsystem);
    boost::shared_ptr<Subsystem> CreateSubsystem(const std::string& name);
    void GetSubsystems(std::vector<std::string>& subsystems);

private:
    std::map<const std::string, boost::shared_ptr<Subsystem> > cgroups_seed_;
};

} //namespace cgroup
} //namespace galaxy
} //namespace baidu
