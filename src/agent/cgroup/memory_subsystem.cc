// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "memory_subsystem.h"
#include "agent/util/path_tree.h"
#include "protocol/galaxy.pb.h"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>


namespace baidu {
namespace galaxy {
namespace cgroup {

MemorySubsystem::MemorySubsystem() {
}

MemorySubsystem::~MemorySubsystem() {
}

std::string MemorySubsystem::Name() {
    return "memory";
}

int MemorySubsystem::Construct() {
    assert(!this->container_id_.empty());
    assert(NULL != this->cgroup_.get());
    std::string path = this->Path();
    boost::system::error_code ec;

    if (!boost::filesystem::exists(path, ec) && !boost::filesystem::create_directories(path, ec)) {
        return -1;
    }

    boost::filesystem::path memory_limit_path = path;
    memory_limit_path.append("memory.limit_in_bytes");

    if (0 != baidu::galaxy::cgroup::Attach(memory_limit_path.c_str(), this->cgroup_->memory().size())) {
        return -1;
    }

    int64_t excess_mode = this->cgroup_->memory().excess() ? 1L : 0L;
    boost::filesystem::path excess_mode_path = path;
    excess_mode_path.append("memory.excess_mode");

    if (0 != baidu::galaxy::cgroup::Attach(memory_limit_path.c_str(), excess_mode)) {
        return -1;
    }

    boost::filesystem::path kill_mode_path = path;
    kill_mode_path.append("memory.kill_mode");

    if (0 != baidu::galaxy::cgroup::Attach(kill_mode_path.c_str(), 0L)) {
        return -1;
    }

    return 0;
}

boost::shared_ptr<google::protobuf::Message> MemorySubsystem::Status() {
    boost::shared_ptr<google::protobuf::Message> ret;
    return ret;
}

boost::shared_ptr<Subsystem> MemorySubsystem::Clone() {
    boost::shared_ptr<Subsystem> ret(new MemorySubsystem());
    return ret;
}

} //namespace cgroup
} //namespace galaxy
} //namespace baidu
