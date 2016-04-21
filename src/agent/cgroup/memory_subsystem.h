// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#include "subsystem.h"
#include <boost/shared_ptr.hpp>

namespace baidu {
namespace galaxy {
namespace cgroup {

class MemorySubsystem : public Subsystem {
public:
    MemorySubsystem();
    ~MemorySubsystem();

    boost::shared_ptr<google::protobuf::Message> Status();
    boost::shared_ptr<Subsystem> Clone();
    std::string Name();
    int Construct();
};

} //namespace cgroup
} //namespace galaxy
} //namespace baidu
