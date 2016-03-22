// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "gflags/gflags.h"
#include "agent/resource_collector.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "input error, ./test_cgroup_resource_collector group_path interval\n"); 
        return EXIT_FAILURE;
    }
    ::google::ParseCommandLineFlags(&argc, &argv, true);    

    std::string group_path(argv[1]);
    int interval = ::atoi(argv[2]);
    using baidu::galaxy::CGroupResourceCollector;
    CGroupResourceCollector* collector = new CGroupResourceCollector(group_path);
    while (1) {
        usleep(interval); 
        bool ret = collector->CollectStatistics();
        if (!ret) {
            fprintf(stderr, "collect cgroup %s error\n", 
                            group_path.c_str());
            continue;
        }

        fprintf(stdout, "collect cgroup %s cpu %lf mem %ld\n",
                        group_path.c_str(),
                        collector->GetCpuCoresUsage(),
                        collector->GetMemoryUsage());
    }
    return EXIT_SUCCESS;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
