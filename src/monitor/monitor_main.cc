// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: zhoushiyong@baidu.com

#include "monitor_impl.h"

#include <sofa/pbrpc/pbrpc.h>

#include <stdio.h>
#include <signal.h>
#include <gflags/gflags.h>

DECLARE_string(monitor_conf_path);
DECLARE_int32(task_id);

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/) {
        s_quit = true;
}

int main(int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);

    galaxy::MonitorImpl* monitor_impl = new galaxy::MonitorImpl();

    monitor_impl->ParseConfig(FLAGS_monitor_conf_path);
    monitor_impl->Run();
    
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    while (!s_quit) {
        sleep(1);
    }
    
    delete monitor_impl;
    return EXIT_SUCCESS;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
