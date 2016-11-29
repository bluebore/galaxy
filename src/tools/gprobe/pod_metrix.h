// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <sstream>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>

namespace baidu {
namespace galaxy {
namespace tools {


class PodMetrix {
public:
    class Volum {
    public:
        Volum() :
            total_in_byte(0),
            used_in_byte(0) {
        }

    public:
        std::string path;
        int64_t total_in_byte;
        int64_t used_in_byte;
    };

    PodMetrix(const std::string& id) :
        pod_id(id),
        cpu_used_in_millicore(0),
        cpu_limit_in_millicore(0),
        rss_used_in_byte(0L),
        cache_used_in_byte(0L),
        memory_limit_in_byte(0L),
        memory_usage_in_byte(0L) {}

    ~PodMetrix() {}

    const std::string& PodId() {
        return pod_id;
    }

    const std::string ToString() {
        std::stringstream ss;
        ss << "GALAXY_CPU_USED_IN_MILLICORE:" << cpu_used_in_millicore << "\n"
           << "GALAXY_CPU_USAGE:" <<  cpu_used_in_millicore * 100.0 / cpu_limit_in_millicore << "\n"
           << "GALAXY_CPU_QUOTA_IN_MILLICORE:" << cpu_limit_in_millicore << "\n"
           << "GALAXY_MEM_USED:" << cache_used_in_byte + rss_used_in_byte << "\n"
           << "GALAXY_MEM_USED_PERCENT:" << (cache_used_in_byte + rss_used_in_byte) * 100.0 / memory_limit_in_byte << "\n"
           << "GALAXY_MEM_TOTAL:" << memory_limit_in_byte << "\n";

        for (size_t i = 0; i < volums.size(); i++) {
            std::string path = volums[i]->path;
            boost::algorithm::to_upper(path);
            boost::algorithm::replace_all(path, "/", "_");
            ss << "GALAXY_DISK" << path << "_TOTAL:" << volums[i]->total_in_byte << "\n"
               << "GALAXY_DISK" << path << "_USED:" << volums[i]->used_in_byte << "\n";
        }

        return ss.str();
    }

public:
    const std::string pod_id;
    // cpu
    int cpu_used_in_millicore;
    int cpu_limit_in_millicore;

    // memory
    int64_t rss_used_in_byte;
    int64_t cache_used_in_byte;
    int64_t memory_limit_in_byte;
    int64_t memory_usage_in_byte;

    // tcp throt

    // volum
    std::vector<boost::shared_ptr<PodMetrix::Volum> > volums;
};
}
}
}
