// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mounter.h"

#include "boost/filesystem/path.hpp"
#include "boost/thread/thread_time.hpp"
#include "boost/filesystem/operations.hpp"

#include "glog/logging.h"

#include <sys/mount.h>

#include <string>
#include <fstream>
#include <iostream>

#ifndef MS_REC
#define MS_REC 0x4000 /* 16384: Recursive loopback */
#endif

namespace baidu {
namespace galaxy {
namespace volum {
baidu::galaxy::util::ErrorCode MountProc(const std::string& source, const std::string& target) {
    boost::filesystem::path path(target);
    boost::system::error_code ec;

    if (!boost::filesystem::exists(path, ec)) {
        return ERRORCODE(-1, "target(%s) donot exist", target.c_str());
    }

    if (!boost::filesystem::is_directory(path, ec)) {
        return ERRORCODE(-1, "target(%s) is not directory", target.c_str());
    }

    if (!boost::filesystem::exists(source, ec)) {
        return ERRORCODE(-1, "source(%s) donot exist", source.c_str());
    }

    if (!boost::filesystem::is_directory(source, ec)) {
        return ERRORCODE(-1, "source(%s) is not directory", source.c_str());
    }

    if (0 != ::mount(source.c_str(), target.c_str(), "proc", 0, NULL)) {
        return PERRORCODE(-1, errno, "mount %s->%s failed", 
                    source.c_str(),
                    target.c_str());
    }

    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode MountDir(const std::string& source, const std::string& target) {
    boost::filesystem::path source_path(source);
    boost::system::error_code ec;

    if (!boost::filesystem::exists(source_path, ec)) {
        return ERRORCODE(-1, "source(%s) donot exist", source.c_str());
    }

    if (!boost::filesystem::is_directory(source_path, ec)) {
        return ERRORCODE(-1, "target(%s) is not directory", source.c_str());
    }

    boost::filesystem::path target_path(target);

    if (!boost::filesystem::exists(target_path, ec)) {
        return  ERRORCODE(-1, "target(%s) donot exist", target.c_str());
    }

    if (!boost::filesystem::is_directory(target_path, ec)) {
        return ERRORCODE(-1, "target(%s) is not directory", target.c_str());
    }

    if (0 != ::mount(source.c_str(), target.c_str(), "", MS_BIND | MS_REC, NULL)) {
        std::cerr << "mount failed: " << source << "->" << target << std::endl;
        return PERRORCODE(-1, errno, "mount %s to %s failed", source.c_str(), target.c_str());
    }

    std::cout << "mount ok: " << source << "->" << target << std::endl;
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode MountTmpfs(const std::string& target, uint64_t size, bool readonly) {
    boost::system::error_code ec;
    boost::filesystem::path target_path(target);

    if (!boost::filesystem::exists(target_path, ec)) {
        return  ERRORCODE(-1, "target(%s) donot exist", target.c_str());
    }

    std::stringstream ss;
    ss << "size=" << size;
    int flag = 0;

    if (readonly) {
        flag |= MS_RDONLY;
    }

    if (0 != ::mount("tmpfs", target.c_str(), "tmpfs", flag,  ss.str().c_str())) {
        return PERRORCODE(-1, errno, "mount tmpfs to %s failed", target.c_str());
    }

    std::cout << "mount tmpfs ok: " << target << std::endl;
    return ERRORCODE_OK;
}


baidu::galaxy::util::ErrorCode Umount(const std::string& target_path) {
    VLOG(10) << "to umount " << target_path;
    std::map<std::string, boost::shared_ptr<Mounter> > mounters;
    ListMounters(mounters);
    std::map<std::string, boost::shared_ptr<Mounter> >::iterator iter = mounters.find(target_path);

    if (mounters.end() == iter) {
        VLOG(10) << target_path << " umount donot exists in /proc/mounts, return ok";
        return ERRORCODE_OK;
    }

    //int n = 0;
    if (0 != ::umount2(target_path.c_str(), MNT_FORCE)) {
        return PERRORCODE(-1, errno, "umount path %s failed", target_path.c_str());
    }

    mounters.clear();
    ListMounters(mounters);
    iter =  mounters.find(target_path);

    if (iter == mounters.end()) {
        LOG(INFO) << "umount successful " << target_path;
        return ERRORCODE_OK;
    }

    return ERRORCODE(-1, "umount path %s failed, which still exists",
            target_path.c_str());
}

baidu::galaxy::util::ErrorCode ListMounters(std::map<std::string, boost::shared_ptr<Mounter> >& mounters) {
    mounters.clear();
    std::ifstream inf("/proc/mounts", std::ios::in);

    if (!inf.is_open()) {
        return ERRORCODE(-1, "open file failed");
    }

    std::string line;
    char source[256];
    char target[256];
    char filesystem[256];
    char option[256];
    char t1[256];
    char t2[256];

    while (getline(inf, line)) {
        if (6 != sscanf(line.c_str(), "%s %s %s %s %s %s",
                source,
                target,
                filesystem,
                option,
                t1,
                t2
                                                )) {
            continue;
        }

        boost::shared_ptr<Mounter> m(new Mounter());
        m->source = source;
        m->target = target;
        m->filesystem = filesystem;
        m->option = option;
        mounters[m->target] = m;
    }

    return ERRORCODE_OK;
}

}
}
}
