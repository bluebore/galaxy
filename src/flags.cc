// Copyright (c) 2014, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>

// master
DEFINE_string(master_port, "7828", "Master service listen port");

// scheduler

// agent

// gce
DEFINE_string(gce_cgroup_root, "/cgroups/", "Cgroup root mount path");
DEFINE_string(gce_support_subsystems, "cpu,memory,cpuacct,frozen", "Cgroup default support subsystems");
DEFINE_int64(gce_initd_zombie_check_interval, 10000, "Initd Zombie Check Interval");
DEFINE_string(gce_initd_dump_file, "initd_checkpoint_file", "Initd Checkpoint File Name");
DEFINE_string(gce_initd_port, "8765", "gce initd listen port");
