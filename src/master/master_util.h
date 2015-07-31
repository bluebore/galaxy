// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_GALAXY_MASTER_UTIL_H
#define BAIDU_GALAXY_MASTER_UTIL_H
#include <string>

namespace baidu {
namespace galaxy {
class JobDescriptor;
class MasterUtil {
public:
    static std::string GenerateJobId(const JobDescriptor& job_desc);
    static std::string GeneratePodId(const JobDescriptor& job_desc);
private:
    static std::string UUID();
};

}   
}

#endif
