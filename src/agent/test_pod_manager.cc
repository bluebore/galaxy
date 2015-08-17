// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/pod_manager.h"
#include <gflags/gflags.h>

static baidu::galaxy::PodManager pod_manager;

void TestLanuchInitd() {
    ::baidu::galaxy::PodInfo info;
    info.pod_id = "test_pod";
    info.initd_port = 8769;
    int ret = pod_manager.LanuchInitd(&info);
    if (ret != 0) {
        fprintf(stderr, "LanuchInitd failed\n");
        return;
    } 
    fprintf(stdout, "LanuchInitd success");
}

int main(int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    TestLanuchInitd();
}


/* vim: set ts=4 sw=4 sts=4 tw=100 */
