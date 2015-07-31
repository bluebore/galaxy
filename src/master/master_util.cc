// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "master_util.h"

#include <sstream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "proto/master.pb.h"

namespace baidu {
namespace galaxy {

std::string MasterUtil::GenerateJobId(const JobDescriptor& job_desc) {
    return "job_" + job_desc.name() + UUID();
}

std::string MasterUtil::GeneratePodId(const JobDescriptor& job_desc) {
    return "pod_" + job_desc.name() + UUID();
}

std::string MasterUtil::UUID() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::stringstream sm_uuid;
    sm_uuid << uuid;
    std::string str_uuid= sm_uuid.str();
    return str_uuid;
}

}
}
