// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "subsystem.h"
#include "protocol/galaxy.pb.h"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast/lexical_cast_old.hpp>

#include <stdio.h>

namespace baidu {
namespace galaxy {
namespace cgroup {

int Subsystem::Destroy() {
    boost::filesystem::path path(this->Path());
    boost::system::error_code ec;

    if (!boost::filesystem::exists(path, ec)) {
        return 0;
    }

    boost::filesystem::remove_all(path, ec);
    return boost::filesystem::exists(path, ec) ? -1 : 0;
}

std::string Subsystem::Path() {
    std::string id = container_id_ + "_" + cgroup_->id();

    boost::filesystem::path path("galaxy");
    path.append(this->Name());
    path.append(id);
    return path.string();

}

int Subsystem::Attach(pid_t pid) {
    boost::filesystem::path proc_path(Path());
    proc_path.append("cgroup.procs");
    boost::system::error_code ec;

    if (!boost::filesystem::exists(proc_path, ec)) {
        return -1;
    }

    return baidu::galaxy::cgroup::Attach(proc_path.c_str(), int64_t(pid));
}

int Subsystem::GetProcs(std::vector<int>& pids) {
    boost::filesystem::path proc_path(Path());
    proc_path.append("cgroup.procs");
    boost::system::error_code ec;

    if (!boost::filesystem::exists(proc_path, ec)) {
        return -1;
    }

    FILE* fin = ::fopen(proc_path.string().c_str(), "re");

    if (NULL == fin) {
        return -1;
    }

    std::string value;
    char temp_read_buffer[30];

    while (::feof(fin) == 0) {
        size_t readlen = ::fread((void*) temp_read_buffer,
                         sizeof(char),
                         sizeof temp_read_buffer, fin);

        if (readlen == 0) {
            break;
        }

        value.append(temp_read_buffer, readlen);
    }

    if (::ferror(fin) != 0) {
        ::fclose(fin);
        return -1;
    }

    ::fclose(fin);
    boost::algorithm::trim(value);
    std::vector<std::string> str_pids;
    boost::split(str_pids, value, boost::is_any_of("\n"));

    for (size_t i = 0; i < str_pids.size(); i++) {
        pids.push_back(atoi(str_pids[i].c_str()));
    }

    return 0;
}


const static int64_t CFS_PERIOD = 100000L; // cpu.cfs_period_us
const static int64_t CFS_SHARE = 1000L; // unit
const static int64_t MILLI_CORE = 1000L; // unit

int Attach(const std::string& file, int64_t value) {
    return Attach(file, boost::lexical_cast<std::string>(value));
}

int Attach(const std::string& file, const std::string& value) {
    FILE* fd = ::fopen(file.c_str(), "we");

    if (NULL == fd) {
        return -1;
    }

    int ret = ::fprintf(fd, "%s", value.c_str());
    ::fclose(fd);

    if (ret <= 0) {
        return -1;
    }

    return 0;
}

int64_t CfsToMilliCore(int64_t cfs) {
    if (cfs <= 0) {
        return -1L;
    }

    return cfs * MILLI_CORE / CFS_PERIOD;
}

int64_t ShareToMilliCore(int64_t share) {
    if (share <= 0) {
        return 0;
    }

    return share * MILLI_CORE / CFS_SHARE;
}

int64_t MilliCoreToCfs(int64_t millicore) {
    if (millicore <= 0) {
        return -1L;
    }

    return millicore * CFS_PERIOD / MILLI_CORE;
}

int64_t MilliCoreToShare(int64_t millicore) {
    if (millicore <= 0) {
        return 0L;
    }

    return millicore * CFS_SHARE / MILLI_CORE;
}

} //namespace cgroup
} //namespace galaxy
} //namespace baidu
