// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cpu_subsystem.h"
#include "protocol/galaxy.pb.h"
#include "agent/util/path_tree.h"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/error_code.hpp>

#include <assert.h>

namespace baidu {
namespace galaxy {
namespace cgroup {

CpuSubsystem::CpuSubsystem() {
}

CpuSubsystem::~CpuSubsystem() {
}

std::string CpuSubsystem::Name() {
    return "cpu";
}

int CpuSubsystem::Construct() {
    assert(NULL != cgroup_);
    assert(!container_id_.empty());
    const std::string cpu_path = this->Path();
    boost::filesystem::path path(cpu_path);
    int ret = -1;
    boost::system::error_code ec;

    if (!boost::filesystem::exists(path) && !boost::filesystem::create_directories(path, ec)) {
        return ret;
    }

    boost::filesystem::path cpu_share(cpu_path);
    cpu_share.append("cpu.shares");
    boost::filesystem::path cpu_cfs(cpu_path);
    cpu_cfs.append("cpu.cfs_quota_us");

    if (cgroup_->cpu().excess()) {
        if (0 == baidu::galaxy::cgroup::Attach(cpu_share.c_str(), MilliCoreToShare(cgroup_->cpu().milli_core()))
                && 0 == baidu::galaxy::cgroup::Attach(cpu_cfs.c_str(), -1)) {
            ret = 0;
        }
    } else {
        boost::filesystem::path cpu_cfs(cpu_path);
        cpu_cfs.append("cpu.cfs_quota_us");

        if (0 == baidu::galaxy::cgroup::Attach(cpu_cfs.c_str(), MilliCoreToCfs(cgroup_->cpu().milli_core()))) {
            ret = 0;
        }
    }

    return ret;
}

boost::shared_ptr<google::protobuf::Message> CpuSubsystem::Status() {
    boost::shared_ptr<google::protobuf::Message> ret;
    return ret;
}

boost::shared_ptr<Subsystem> CpuSubsystem::Clone() {
    boost::shared_ptr<Subsystem> ret(new CpuSubsystem());
    return ret;
}

} //namespace cgroup
} //namespace galaxy
} //namespace baidu
