// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/cgroups.h"

#include <stdio.h>
#include <stdlib.h>
#include "gflags/gflags.h"

DEFINE_string(cgroup_mount_path, "", "cgroup mount root path");
DEFINE_string(operation, "", "support operation : AttachPid GetPids Read Write Frozen Thaw");
DEFINE_string(cgroups_subsystem, "", "operate subsystem");
DEFINE_string(cgroups_name, "", "use cgroup_name");
DEFINE_string(cgroup_control_file, "", "control file name");
DEFINE_int32(attach_pid, -1, "pid which to attachted");
DEFINE_string(write_value, "", "value to write to cgroup control file");

int main(int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);    
    std::string hierarchy = FLAGS_cgroup_mount_path + "/" + FLAGS_cgroups_subsystem;
    if (FLAGS_operation == "AttachPid") {
        bool ret = baidu::galaxy::cgroups::AttachPid(
                        hierarchy, 
                        FLAGS_cgroups_name, 
                        FLAGS_attach_pid); 
        if (!ret) {
            fprintf(stderr, "attach pid %d failed to cgroup %s",
                            FLAGS_attach_pid, 
                            FLAGS_cgroups_name.c_str());
        }
    } else if (FLAGS_operation == "GetPids") {
        std::vector<int> pids;
        bool ret = baidu::galaxy::cgroups::GetPidsFromCgroup(
                    hierarchy, FLAGS_cgroups_name, &pids); 
        if (!ret) {
            fprintf(stderr, "get pids from cgroup %s failed\n",
                            FLAGS_cgroups_name.c_str()); 
        } else {
            for (size_t i = 0; i < pids.size(); ++i) {
                fprintf(stdout, "get pids from cgroup %s : %d\n",
                                FLAGS_cgroups_name.c_str(),
                                pids[i]); 
            }
        }
    } else if (FLAGS_operation == "Read") {
        std::string value;
        int ret = baidu::galaxy::cgroups::Read(hierarchy,
                                               FLAGS_cgroups_name,
                                               FLAGS_cgroup_control_file,
                                               &value); 
        if (ret != 0) {
            fprintf(stderr, "read %s from %s failed\n",
                            FLAGS_cgroup_control_file.c_str(),
                            FLAGS_cgroups_name.c_str());

        } else {
            fprintf(stdout, "read %s from %s value %s\n",
                            FLAGS_cgroup_control_file.c_str(),
                            FLAGS_cgroups_name.c_str(),
                            value.c_str()); 
        }
    } else if (FLAGS_operation == "Write") {
        int ret = baidu::galaxy::cgroups::Write(hierarchy,
                                                FLAGS_cgroups_name,
                                                FLAGS_cgroup_control_file,
                                                FLAGS_write_value); 
        if (ret != 0) {
            fprintf(stderr, "write %s to %s failed\n",
                            FLAGS_cgroup_control_file.c_str(),
                            FLAGS_cgroups_name.c_str()); 
        } else {
            fprintf(stdout, "write %s to %s value %s\n",
                            FLAGS_cgroup_control_file.c_str(),
                            FLAGS_cgroups_name.c_str(),
                            FLAGS_write_value.c_str()); 
        }
    } else if (FLAGS_operation == "Frozen") {
        bool ret = baidu::galaxy::cgroups::FreezerSwitch(hierarchy,
                                                        FLAGS_cgroups_name, 
                                                        "FROZEN");
        if (!ret) {
            fprintf(stderr, "freezer switch %s to Frozen failed\n",
                            FLAGS_cgroups_name.c_str()); 
        } else {
            fprintf(stdout, "freezer switch %s to Frozen success\n",
                            FLAGS_cgroups_name.c_str()); 
        }
    } else if (FLAGS_operation == "Thaw") {
        bool ret = baidu::galaxy::cgroups::FreezerSwitch(hierarchy,
                                                         FLAGS_cgroups_name,
                                                         "THAWED"); 
        if (!ret) {
            fprintf(stderr, "freezer switch %s to Thaw failed\n",
                    FLAGS_cgroups_name.c_str()); 
        } else {
            fprintf(stdout, "freezer switch %s to Thaw success\n",
                    FLAGS_cgroups_name.c_str()); 
        }
    } else {
        fprintf(stderr, "invalid operation %s\n", FLAGS_operation.c_str());
        return -1;
    }
    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
