// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_GALAXY_MASTER_UTIL_H
#define BAIDU_GALAXY_MASTER_UTIL_H

#include <vector>
#include <set>
#include <algorithm> 
#include <string>

namespace baidu {
namespace galaxy {

class JobDescriptor;
class Resource;
class AgentInfo;

class MasterUtil {
public:

    static void AddResource(const Resource& from, Resource* to);
    static void SubstractResource(const Resource& from, Resource* to);
    static bool FitResource(const Resource& from, const Resource& to);
    static std::string SelfEndpoint();
    static void TraceJobDesc(const JobDescriptor& job_desc);
    static void SetDiff(const std::set<std::string>& left_set, 
                        const std::set<std::string>& right_set, 
                        std::set<std::string>* left_diff, 
                        std::set<std::string>* right_diff);
    static void ResetLabels(AgentInfo* agent, const std::set<std::string>& labels);
    static std::string UUID();
    static std::string ShortName(const std::string& job_name);
};

}   
}

#endif
