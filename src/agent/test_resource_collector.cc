// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "agent/resource_collector.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "input error, ./test_resource_collector pid\n"); 
        return EXIT_FAILURE;
    }

    int pid = ::atoi(argv[1]);
    int interval = ::atoi(argv[2]);
    using baidu::galaxy::ProcResourceCollector;
    ProcResourceCollector* collector = new ProcResourceCollector(pid);

    collector->ResetPid(pid);
    while (1) {
        usleep(interval);
        bool ret = collector->CollectStatistics();
        if (!ret) {
            fprintf(stderr, "collect pid %d error\n", pid); 
            continue; 
        }

        fprintf(stdout, "collect pid %d cpu %lf mem %ld\n",
                pid,
                collector->GetCpuCoresUsage(),
                collector->GetMemoryUsage());
    }
    return EXIT_SUCCESS;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
