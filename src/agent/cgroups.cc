// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/cgroups.h"

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

bool FreezerSwitch(const std::string& hierarchy,
                   const std::string& cgroup,
                   const std::string& freezer_state) {
   return FreezerSwitch(hierarchy + "/" + cgroup, freezer_state); 
}

bool FreezerSwitch(const std::string& cgroup,
                   const std::string& freezer_state) {
    const int MAX_RETRY_TIMES = 3;    
    for (int i = 0; i < MAX_RETRY_TIMES; ++i) {
        if (0 != Write(cgroup, 
                       "freezer.state", 
                       freezer_state)) {
            return false; 
        }     
        ::usleep(100000);  
        std::string freezer_state_val;
        if (0 != Read(cgroup,
                      "freezer.state",
                      &freezer_state_val)) {
            return false; 
        } 
        if (freezer_state_val == freezer_state) {
            return true; 
        }
    }    
    return false;
}



bool GetPidsFromCgroup(const std::string& hierarchy,
                       const std::string& cgroup,
                       std::vector<int>* pids) {
    return GetPidsFromCgroup(hierarchy + "/" + cgroup, pids);
}

bool GetPidsFromCgroup(const std::string& cgroup,
                       std::vector<int>* pids) {
    if (pids == NULL) {
        return false; 
    }
    std::string value;
    if (0 != Read(cgroup, "cgroup.procs", &value)) {
        return false; 
    }

    LOG(DEBUG, "get from cgroup.proc %s", value.c_str());
    std::vector<std::string> str_pids;
    boost::split(str_pids, value, boost::is_any_of("\n"));    
    for (size_t i = 0; i < str_pids.size(); i++) {
        std::string& value = str_pids[i];
        if (value.size() == 0 || value.empty()) {
            continue; 
        }
        pids->push_back(::atoi(value.c_str())); 
    }
    LOG(DEBUG, "get from cgroup.proc pids %u", pids->size());
    return true;
}


bool AttachPid(const std::string& hierarchy,
               const std::string& cgroup,
               int pid) {
    return AttachPid(hierarchy+"/"+ cgroup, pid);
}

bool AttachPid(const std::string& cgroup_folder,
              int pid) {
    if (0 == Write( cgroup_folder, 
                "tasks", 
                boost::lexical_cast<std::string>(pid))) {
        return true; 
    }
    return false;
}

int Write(const std::string& folder,
          const std::string& control_file,
          const std::string& value) {
    std::string file_path = folder + "/" + control_file;
    FILE* fd = ::fopen(file_path.c_str(), "we");
    if (fd == NULL) {
        LOG(WARNING, "open %s failed", file_path.c_str());
        return -1;
    }

    int ret = ::fprintf(fd, "%s", value.c_str()); 
    ::fclose(fd);
    if (ret <= 0) {
        LOG(WARNING, "write %s failed", file_path.c_str());
        return -1; 
    }
    return 0;

}

int Write(const std::string& hierarchy,
          const std::string& cgroup,
          const std::string& control_file,
          const std::string& value) {                        
    return Write(hierarchy + "/" + cgroup, control_file, value);
}


int Read(const std::string& hierarchy,
         const std::string& cgroup,
         const std::string& control_file,
         std::string* value) {
    return Read(hierarchy+"/"+cgroup, control_file, value);
}

int Read(const std::string& cgroup,
         const std::string& control_file,
         std::string* value) {
    if (value == NULL) {
        return -1; 
    }    
    std::string file_path = cgroup + "/" + control_file;
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
    boost::algorithm::trim(*value);
    return 0;
}



}   // ending namespace cgroups
}   // ending namespace galaxy
}   // ending namespace baidu
