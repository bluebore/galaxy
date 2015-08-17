// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/task_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>

#include "gflags/gflags.h"

#include "agent/agent_internal_infos.h"
#include "proto/galaxy.pb.h"

DEFINE_string(binary, "", "task binary support binary and ftp");
DEFINE_string(start_command, "", "start command");
DEFINE_string(stop_command, "", "stop command");
DEFINE_string(initd_endpoint, "", "initd endpoint");
DEFINE_string(task_id, "1234566", "task id");
DEFINE_string(pod_id, "654321", "pod id");

int main(int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    ::baidu::galaxy::TaskManager task_manager;    
    
    ::baidu::galaxy::TaskInfo info;
    ::baidu::galaxy::TaskDescriptor desc;

    if (boost::starts_with(FLAGS_binary, "ftp://")) {
        desc.set_binary(FLAGS_binary); 
        desc.set_source_type(::baidu::galaxy::kSourceTypeFTP);
    } else {
        FILE* fp = ::fopen(FLAGS_binary.c_str(), "r"); 
        if (fp == NULL) {
            fprintf(stderr, "open %s for read failed err[%d: %s]\n", 
                    FLAGS_binary.c_str(), errno, strerror(errno)); 
            return -1;
        }
        std::string task_raw;
        char buf[1024];
        int len = 0;
        while ((len = fread(buf, 1, 1024, fp)) > 0) {
            task_raw.append(buf, len); 
        }
        fclose(fp);
        fprintf(stdout, "task binary len %lu\n", task_raw.size());
        desc.set_binary(task_raw);
        desc.set_source_type(::baidu::galaxy::kSourceTypeBinary);
    }

    desc.set_start_command(FLAGS_start_command);
    desc.set_stop_command(FLAGS_stop_command);
    info.initd_endpoint = FLAGS_initd_endpoint;
    info.task_id = FLAGS_task_id;
    info.pod_id = FLAGS_pod_id;
    info.desc.CopyFrom(desc);
    task_manager.CreateTask(info);
    sleep(10);
    task_manager.QueryTask(&info);
    fprintf(stdout, "task %s state %s\n",
            info.task_id.c_str(), 
            ::baidu::galaxy::PodState_Name(info.status.state()).c_str());
    task_manager.DeleteTask(info.task_id);
    sleep(10);
    task_manager.QueryTask(&info);
    fprintf(stdout, "task %s state %s\n",
            info.task_id.c_str(), 
            ::baidu::galaxy::PodState_Name(info.status.state()).c_str());
    sleep(1000);
    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
