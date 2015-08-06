// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gce/cgroups.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "logging.h"

namespace baidu {
namespace galaxy {
namespace cgroups {

bool GetPidsFromCgroup(const std::string& hierarchy,
                       const std::string& cgroup,
                       std::vector<std::string>* pids) {
    if (pids == NULL) {
        return false; 
    }
    std::string value;
    if (0 != Read(hierarchy, cgroup, "cgroup.proc", &value)) {
        return false; 
    }

    LOG(DEBUG, "get from cgroup.proc %s", value.c_str());
    boost::split(*pids, value, boost::is_any_of("\n"));    
    LOG(DEBUG, "get from cgroup.proc pids %u", pids->size());
    return true;
}

bool AttachPid(const std::string& hierarchy,
               const std::string& cgroup,
               int pid) {
    if (0 == Write(
                hierarchy, 
                cgroup, 
                "tasks", 
                boost::lexical_cast<std::string>(pid))) {
        return true; 
    }
    return false;
}

int Write(const std::string& hierarchy,
          const std::string& cgroup,
          const std::string& control_file,
          const std::string& value) {                        
    std::string file_path = hierarchy + "/" 
        + cgroup + "/" + control_file;
    FILE* fd = ::fopen(file_path.c_str(), "we");
    if (fd == NULL) {
        return -1;
    }

    int ret = ::fprintf(fd, "%s", value.c_str()); 
    ::fclose(fd);
    if (ret <= 0) {
        return -1; 
    }
    return 0;
}

int Read(const std::string& hierarchy,
         const std::string& cgroup,
         const std::string& control_file,
         std::string* value) {
    if (value == NULL) {
        return -1; 
    }    
    std::string file_path = hierarchy + "/" 
        + cgroup + "/" + control_file;
    FILE* fin = ::fopen(file_path.c_str(), "re"); 
    if (NULL == fin) {
        return -1; 
    }

    const int TEMP_BUF_SIZE = 30;
    char temp_read_buffer[TEMP_BUF_SIZE];
    while (::feof(fin) == 0) {
        size_t readlen = ::fread((void*)temp_read_buffer, 
                                 sizeof(char), 
                                 TEMP_BUF_SIZE, fin); 
        if (readlen == 0) {
            break; 
        }
        value->append(temp_read_buffer, readlen);
    }

    if (::ferror(fin) != 0) {
        ::fclose(fin);
        return -1;
    }
    ::fclose(fin);
    return 0;
}


}   // ending namespace cgroups
}   // ending namespace galaxy
}   // ending namespace baidu
