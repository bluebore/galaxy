// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#include "cgroup.h"

#include <boost/shared_ptr.hpp>
#include <map>

namespace baidu {
namespace galaxy {
namespace cgroup {
            
class CgroupFactory {
public:
    void Register(const std::string& name, boost::shared_ptr<Cgroup> subsystem);
    boost::shared_ptr<Cgroup> CreateCgroup(const std::string& name);
    
private:
    std::map<const std::string, boost::shared_ptr<Cgroup> > _m_cgroup;
};

} //namespace cgroup
} //namespace galaxy
} //namespace baidu
