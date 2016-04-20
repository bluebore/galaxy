// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cgroup_factory.h"
#include "subsystem.h"

namespace baidu {
namespace galaxy {
namespace cgroup {
            
void CgroupFactory::Register(const std::string& name, boost::shared_ptr<Subsystem> cgroup) {
    assert(NULL != cgroup.get());
    _m_cgroup[name] = cgroup;
}

boost::shared_ptr<Cgroup> CgroupFactory::CreateSubsystem(const std::string& name) {
    boost::shared_ptr<Cgroup> ret;
    std::map<std::string, boost::shared_ptr<Subsystem> >::iterator iter = _m_cgroup.find(cgroup_config);
    if (_m_cgroup.end() != iter) {
        ret = iter->second->Clone();
    }
    
    return ret;
}
                
} //namespace cgroup
} //namespace galaxy
} //namespace baidu
