// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once
#include "protocol/galaxy.pb.h"
#include <sys/types.h>

#include <boost/shared_ptr.hpp>
#include <google/protobuf/message.h>

#include <string>
#include <vector>

namespace baidu {
namespace galaxy {
    namespace cgroup {
        class Cgroup;
    }
    
    namespace volum {
        class VolumGroup;
    }
    
namespace container {
class Container {
public:
    Container(const baidu::galaxy::proto::ContainerDescription& desc);
    ~Container();
            
    std::string Id() const;
    int Construct();
    int Destroy();
    int Run();
    int Tasks(std::vector<pid_t>& pids);
    int Pids(std::vector<pid_t>& pids);
    boost::shared_ptr<google::protobuf::Message> Status();
    
private:
    int RunRoutine(Container* container);
    const baidu::galaxy::proto::ContainerDescription desc_;
    std::vector<boost::shared_ptr<baidu::galaxy::cgroup::Cgroup> > cgroup_;
    boost::shared_ptr<baidu::galaxy::volum::VolumGroup> volum_group_;
    
    int Attach(pid_t pid);
    int Detach(pid_t pid);
    int Detachall();
    
};

} //namespace container
} //namespace galaxy
} //namespace baidu
