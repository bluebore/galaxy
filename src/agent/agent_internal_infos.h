// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _AGENT_INTERNAL_INFOS_H
#define _AGENT_INTERNAL_INFOS_H

#include <string>
#include <vector>

#include "agent/resource_collector.h"
#include "boost/lexical_cast.hpp"
#include "google/protobuf/text_format.h"
#include "proto/galaxy.pb.h"
#include "proto/initd.pb.h"
#include "proto/agent.pb.h"

namespace baidu {
namespace galaxy {

struct TaskInfo {
    std::string task_id;    
    std::string pod_id;     // which pod belong to 
    std::string job_id;
    std::string job_name;
    TaskDescriptor desc;
    TaskStatus status;
    std::string initd_endpoint;
    TaskStage stage;

    ProcessInfo main_process;
    ProcessInfo deploy_process;
    ProcessInfo stop_process;

    std::string cgroup_path;
    std::string task_workspace;
    int64_t ctime;
    std::map<std::string, std::string> cgroups;
    std::string task_chroot_path; // chroot path
    int fail_retry_times;
    int max_retry_times;
    int stop_timeout_point;
    int initd_check_failed;
    std::string gc_dir;
    Initd_Stub* initd_stub;
    CGroupResourceCollector* resource_collector;
    CGroupIOStatistics old_io_stat;
    int64_t io_collect_counter;
    std::string ToString() {
        std::string pb_str;
        std::string str_format;     
        str_format.append("task_id : " + task_id + "\n");
        str_format.append("pod_id : "+ pod_id + "\n");
        str_format.append("task desc : {\n"); 
        google::protobuf::TextFormat::PrintToString(desc, &pb_str);      

        str_format.append(pb_str);
        str_format.append("}\n"); 
        str_format.append("task status : {\n");
        pb_str.clear(); 
        google::protobuf::TextFormat::PrintToString(status, &pb_str);

        str_format.append(pb_str);
        str_format.append("}\n");
        str_format.append("stage : " + TaskStage_Name(stage) + "\n"); 

        str_format.append("main_process : {\n");
        pb_str.clear();
        google::protobuf::TextFormat::PrintToString(main_process, &pb_str);
        str_format.append(pb_str);
        str_format.append("}\n");
        str_format.append("deploy_process : {\n");
        pb_str.clear();
        google::protobuf::TextFormat::PrintToString(deploy_process, &pb_str);
        str_format.append(pb_str);
        str_format.append("}\n");
        str_format.append("stop_process : {\n");
        pb_str.clear();
        google::protobuf::TextFormat::PrintToString(stop_process, &pb_str);
        str_format.append(pb_str);
        str_format.append("}\n");
        str_format.append("cgroup_path : " + cgroup_path + "\n");
        str_format.append("task_workspace : " + task_workspace + "\n");
        str_format.append("task_chroot_path : " + task_chroot_path + "\n");
        return str_format;
    }

    TaskInfo() 
        : task_id(), 
          pod_id(), 
          desc(), 
          status(),
          initd_endpoint(),
          stage(kTaskStagePENDING),
          main_process(),
          deploy_process(),
          stop_process(),
          cgroup_path(),
          task_workspace(),
          task_chroot_path(),
          fail_retry_times(0),
          max_retry_times(3),
          stop_timeout_point(0),
          initd_check_failed(0),
          initd_stub(NULL),
          resource_collector(NULL),
          old_io_stat(),
          io_collect_counter(0){
    }

    void CopyFrom(const TaskInfo& task) {
        task_id = task.task_id; 
        pod_id = task.pod_id;
        job_name = task.job_name;
        job_id = task.job_id;
        desc.CopyFrom(task.desc);
        status.CopyFrom(task.status);
        initd_endpoint = task.initd_endpoint;
        stage = task.stage;
        main_process.CopyFrom(task.main_process);
        deploy_process.CopyFrom(task.deploy_process);
        stop_process.CopyFrom(task.stop_process);
        cgroup_path = task.cgroup_path;
        task_workspace = task.task_workspace;
        task_chroot_path = task.task_chroot_path;
        fail_retry_times = task.fail_retry_times;
        max_retry_times = task.max_retry_times;
        stop_timeout_point = task.stop_timeout_point;
        initd_check_failed = task.initd_check_failed;
        initd_stub = task.initd_stub;
        resource_collector = task.resource_collector;
        gc_dir = task.gc_dir;
    }

    TaskInfo& operator=(const TaskInfo& task) {
        CopyFrom(task);
        return *this; 
    }

    TaskInfo(const TaskInfo& task) {
        CopyFrom(task);
    }
};

struct PodInfo {
    std::string pod_id;
    std::string job_id;
    PodDescriptor pod_desc;     
    PodStatus pod_status;
    int initd_port;
    int initd_pid;
    std::map<std::string, TaskInfo> tasks;
    std::string pod_path;
    std::string job_name; 

    std::string ToString() {
        std::string str_format; 
        std::string pb_str;

        str_format.append("pod_id : " + pod_id + "\n");
        str_format.append("job_id : " + job_id + "\n");
        str_format.append("pod_desc : {\n");
        pb_str.clear(); 
        google::protobuf::TextFormat::PrintToString(pod_desc, &pb_str);
        str_format.append(pb_str); 
        str_format.append("}\n");
        str_format.append("pod_status : {\n");
        pb_str.clear();
        google::protobuf::TextFormat::PrintToString(pod_status, &pb_str);
        str_format.append(pb_str);
        str_format.append("}\n");

        str_format.append("initd port : " + boost::lexical_cast<std::string>(initd_port) + "\n"); 
        str_format.append("initd pid : " + boost::lexical_cast<std::string>(initd_pid) + "\n");
        str_format.append("tasks : [\n");
        std::map<std::string, TaskInfo>::iterator task_it = tasks.begin();
        for (; task_it != tasks.end(); ++task_it) {
            str_format.append(task_it->second.ToString());     
            str_format.append(",\n");
        }
        str_format.append("]\n");
        str_format.append("pod path: ");
        str_format.append(pod_path);
        str_format.append("\n");
        str_format.append("job name: ");
        str_format.append(job_name);
        str_format.append("\n");
        return str_format;
    }

    PodInfo() 
        : pod_id(), 
          job_id(),
          pod_desc(),
          pod_status(),
          initd_port(-1),
          initd_pid(-1),
          tasks(),
          pod_path(),
          job_name() {
    }

    void CopyFrom(const PodInfo& pod_info) {
        pod_id = pod_info.pod_id;     
        job_id = pod_info.job_id;
        pod_desc.CopyFrom(pod_info.pod_desc);
        pod_status.CopyFrom(pod_info.pod_status);
        initd_port = pod_info.initd_port;
        initd_pid = pod_info.initd_pid;
        tasks = pod_info.tasks;
        pod_path = pod_info.pod_path;
        job_name = pod_info.job_name;
    }

    PodInfo(const PodInfo& pod_info) {
        CopyFrom(pod_info);
    }

    PodInfo& operator=(const PodInfo& from) {
        CopyFrom(from);
        return *this;
    }
};

}   // ending namespace galaxy
}   // ending namespace baidu



#endif  //_AGENT_INTERNAL_INFOS_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
