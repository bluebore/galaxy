// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/persistence_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>

#include "gflags/gflags.h"

DEFINE_string(persistence_path, "", "persistence path");
DEFINE_string(operation, "", "support operation Save, Scan, Delete");
DEFINE_string(pod_id, "", "operate pod id");

void SavePod(baidu::galaxy::PersistenceHandler* handler) {
    baidu::galaxy::TaskInfo task_info;
    task_info.task_id = "test_task";
    task_info.pod_id = FLAGS_pod_id;
    task_info.desc.set_start_command("start command");

    baidu::galaxy::PodInfo info; 
    info.pod_id = FLAGS_pod_id;
    info.job_id = "test_job";
    info.pod_desc.add_labels("label1");
    info.initd_port = 8989;
    info.initd_pid = 99;
    info.tasks[task_info.task_id] = task_info;
    
    bool ret = handler->SavePodInfo(info); 
    if (!ret) {
        fprintf(stderr, "save pod %s failed\n", 
                        FLAGS_pod_id.c_str());
    }
    return;
}

void ScanPods(baidu::galaxy::PersistenceHandler* handler) {
    std::vector<baidu::galaxy::PodInfo> pods;
    bool ret = handler->ScanPodInfo(&pods);
    if (!ret) {
        fprintf(stderr, "scan pod failed\n"); 
        return;
    }

    for (size_t i = 0; i < pods.size(); i++) {
        fprintf(stdout, "list pod %s\n", pods[i].pod_id.c_str()); 
    }
    return;
}

void DeletePod(baidu::galaxy::PersistenceHandler* handler) {
    bool ret = handler->DeletePodInfo(FLAGS_pod_id);
    if (!ret) {
        fprintf(stderr, "delete pod %s failed\n", FLAGS_pod_id.c_str()); 
    }
    return;
}

int main(int argc, char* argv[]) {
    using baidu::galaxy::PersistenceHandler;
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    PersistenceHandler handler;
    if (FLAGS_persistence_path.empty()) {
        fprintf(stderr, "persistence path not setted\n");
        return EXIT_FAILURE; 
    }
    if (!handler.Init(FLAGS_persistence_path)) {
        fprintf(stderr, "persistence handler init failed\n");
        return EXIT_FAILURE;
    } 

    if (FLAGS_operation == "Save") {
        SavePod(&handler);
    } else if (FLAGS_operation == "Scan") {
        ScanPods(&handler);    
    } else if (FLAGS_operation == "Delete") {
        DeletePod(&handler); 
    } else {
        fprintf(stderr, "invalid operation for pod\n"); 
    }

    return EXIT_SUCCESS;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
