// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "subsystem_factory.h"

namespace baidu {
namespace galaxy {
namespace cgroup {
            
void SubsystemFactory::Register(const std::string& name, boost::shared_ptr<Subsystem> cgroup) {
    assert(NULL != cgroup.get());
    cgroups_seed_[name] = cgroup;
}

boost::shared_ptr<Subsystem> SubsystemFactory::CreateSubsystem(const std::string& cgroup_config) {
    boost::shared_ptr<Subsystem> ret;
    std::map<std::string, boost::shared_ptr<Subsystem> >::iterator iter = cgroups_seed_.find(cgroup_config);
    if (cgroups_seed_.end() != iter) {
        ret = iter->second->Clone();
    }
    
    return ret;
}


void SubsystemFactory::GetSubsystems(std::vector<std::string>& subsystems) {
    subsystems.clear();
    std::map<std::string, boost::shared_ptr<Subsystem> >::iterator iter = cgroups_seed_.begin();
    while (iter != cgroups_seed_.end()) {
        subsystems.push_back(iter->first);
    }
}
                
} //namespace cgroup
} //namespace galaxy
} //namespace baidu
