// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CGROUPS_H_
#define _CGROUPS_H_

#include <string>
#include <vector>

namespace baidu {
namespace galaxy {
namespace cgroups {

bool FreezerSwitch(const std::string& hierarchy,
                   const std::string& cgroup,
                   const std::string& freezer_state);

bool FreezerSwitch(const std::string& cgroup,
                   const std::string& freezer_state);


bool GetPidsFromCgroup(const std::string& hierarchy, 
                       const std::string& cgroup, 
                       std::vector<int>* pids);

bool GetPidsFromCgroup(const std::string& cgroup, 
                       std::vector<int>* pids);


bool AttachPid(const std::string& hierarchy,
               const std::string& cgroup,
               int pid);
bool AttachPid(const std::string& cgroup_folder, int pid);
// hierarchy path for cgroups root. example : /cgroups/subsystem/
// cgroup relative path for hierarchy. example ./xxxxxx/
// control file. example cpu.share
// value    
int Write(const std::string& hierarchy, 
          const std::string& cgroup, 
          const std::string& control_file, 
          const std::string& value);

int Write(const std::string& folder, 
          const std::string& control_file, 
          const std::string& value);

int Read(const std::string& hierarchy,
         const std::string& cgroup,
         const std::string& control_file,
         std::string* value);

int Read(const std::string& cgroup,
         const std::string& control_file,
         std::string* value);


}   // ending namespace cgroups
}   // ending namespace galaxy
}   // ending namespace baidu

#endif

