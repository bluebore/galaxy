// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "sdk/galaxy.h"

#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <gflags/gflags.h>

DEFINE_string(master_addr, "localhost:8102", "master rpc-server endpoint");
DEFINE_string(job_name, "", "job name shown on galaxy");
DEFINE_string(task_raw, "", "job package which could be a ftp address or a local file ");
DEFINE_string(cmd_line, "", "the boot command of job");
DEFINE_int32(replicate_num, 0, "the replication number of job");
DEFINE_double(cpu_soft_limit, 0.0, "cpu soft limit which is lower than cpu limit");
DEFINE_double(cpu_limit, 0.0, "cpu limit which a task can reach but not overtop");
DEFINE_int32(deploy_step_size ,0, "how many tasks can be deployed in concurrent");
DEFINE_bool(one_task_per_host , false, "every node just run one task of job");
DEFINE_int64(task_id, -1, "the identify of task");
DEFINE_int64(job_id, -1, "the identify of job");
DEFINE_string(agent_addr, "", "the address of a agent shown by listnode");
DEFINE_int64(mem_gbytes, 32, "mem in giga bytes that job needs");
DEFINE_string(restrict_tag, "", "specify tag to run task");
DEFINE_string(tag, "", "add tag to agent");
DEFINE_string(agent_list, "", "specify agent list to add tag ,eg host:port,host2:port");
DEFINE_string(monitor_conf, "", "specify monitor agent config file");

std::string USAGE = "\n./galaxy_client add --master_addr=localhost:8102 --job_name=1234 ...\n" 
                   "./galaxy_client list --task_id=1234 --master_addr=localhost:8102 \n"
                   "./galaxy_client list --job_id=9527 --master_addr=localhost:8102\n"
                    "./galaxy_client kill --job_id=9527 --master_addr=localhost:8102";

int ProcessNewJob(){
    if(FLAGS_job_name.empty()){
        fprintf(stderr, "--job_name or -job_name option which can not be empty  is required\n");
        return -1;
    }
    if(FLAGS_task_raw.empty()){
        fprintf(stderr, "--package  option which can not be empty  is required\n");
        return -1;
    }
    if(FLAGS_cmd_line.empty()){
        fprintf(stderr, "--cmd_line  option which can not be empty  is required\n");
        return -1;
    }
    std::string task_raw;
    if (!boost::starts_with(FLAGS_task_raw, "ftp://")) {
        FILE* fp = fopen(FLAGS_task_raw.c_str(), "r");
        if (fp == NULL) {
            fprintf(stderr, "Open %s for read fail\n", FLAGS_task_raw.c_str());
            return -2;
        }
        char buf[1024];
        int len = 0;
        while ((len = fread(buf, 1, 1024, fp)) > 0) {
            task_raw.append(buf, len);
        }
        fclose(fp);
        printf("Task binary len %lu\n", task_raw.size());
    }
    else {
        task_raw = FLAGS_task_raw;
    }
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    galaxy::JobDescription job;
    galaxy::PackageDescription pkg;
    pkg.source = task_raw;
    job.pkg = pkg;
    job.cmd_line = FLAGS_cmd_line;
    job.replicate_count = FLAGS_replicate_num;
    job.job_name = FLAGS_job_name;
    job.cpu_share = FLAGS_cpu_soft_limit;
    job.mem_share = 1024 * 1024 * 1024 * FLAGS_mem_gbytes;
    job.deploy_step_size = FLAGS_deploy_step_size;
    job.cpu_limit = FLAGS_cpu_limit;
    job.one_task_per_host = FLAGS_one_task_per_host;
    job.restrict_tag = FLAGS_restrict_tag;
    if (!FLAGS_monitor_conf.empty()) {
        FILE* fd = fopen(FLAGS_monitor_conf.c_str(), "r");
        if (fd == NULL) {
                fprintf(stderr, "Open %s for read fail [%d:%s]\n", 
                        FLAGS_monitor_conf.c_str(), errno, strerror(errno));
                return -2;
            }
            std::string monitor_conf;
            char buf[1024];
            int len = 0;
            while ((len = fread(buf, 1, 1024, fd)) > 0) {
                monitor_conf.append(buf, len);
            }
            fclose(fd);
            job.monitor_conf = monitor_conf;
    }
    fprintf(stdout, "%ld", galaxy->NewJob(job));
    return 0;
}

int ListTask(){
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    galaxy->ListTask(FLAGS_job_id, FLAGS_task_id, NULL);
    return 0;
}

int ListJob(){
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    std::vector<galaxy::JobInstanceDescription> jobs;
    galaxy->ListJob(&jobs);
    std::vector<galaxy::JobInstanceDescription>::iterator it = jobs.begin();
    fprintf(stdout, "================================\n");
    for(;it != jobs.end();++it){
        fprintf(stdout, "%ld\t%s\t%d\t%d\n",
                it->job_id, it->job_name.c_str(),
                it->running_task_num, it->replicate_count);
    }
    return 0;
}

int ListNode(){
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    std::vector<galaxy::NodeDescription> nodes;
    galaxy->ListNode(&nodes);
    std::vector<galaxy::NodeDescription>::iterator it = nodes.begin();
    fprintf(stdout, "================================\n");
    for(; it != nodes.end(); ++it){
        fprintf(stdout, "%ld\t%s\tTASK:%d\tCPU:%0.2f\t"
                    "USED:%0.2f\tMEM:%ldGB\tUSED:%ldGB\tTAG:%s\n",
                    it->node_id, it->addr.c_str(),
                    it->task_num, it->cpu_share,
                    it->cpu_used, it->mem_share/(1024*1024*1024),
                    it->mem_used/(1024*1024*1024),
                    boost::algorithm::join(it->tags, ",").c_str());
    }
    return 0;
}
int ListTaskByAgent(){
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    if(FLAGS_agent_addr.empty()){
        fprintf(stderr, "--agent_addr which can not be empty  is required ");
        return -1;
    }
    galaxy->ListTaskByAgent(FLAGS_agent_addr, NULL);
    return 0;
}

int KillTask(){
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    galaxy->KillTask(FLAGS_task_id);
    return 0;
}

int KillJob(){
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    galaxy->TerminateJob(FLAGS_job_id);
    return 0;
}

int UpdateJob(){
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    galaxy::JobDescription job;
    job.replicate_count = FLAGS_replicate_num;
    job.job_id  =  FLAGS_job_id;
    job.deploy_step_size = FLAGS_deploy_step_size;
    galaxy->UpdateJob(job);
    return 0;
}

int TagAgent() {
    galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(FLAGS_master_addr);
    if (FLAGS_tag.empty()) {
        fprintf(stderr, "--tag  which can not be empty is required\n");
        return -1;
    }
    std::set<std::string> agents;
    boost::split(agents, FLAGS_agent_list, boost::is_any_of(","));
    bool ret = galaxy->TagAgent(FLAGS_tag, &agents);
    if (!ret) {
        fprintf(stderr, "fail to tag agent\n");
    }
    return ret ? 0: -1;
}

int main(int argc, char* argv[]) {

    ::google::SetUsageMessage(USAGE);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    if(argc < 2){
        fprintf(stderr, "subcommand is required , eg ./galaxy_client list --task_id=9527\n");
        return -1;
    }
    if(FLAGS_master_addr.empty()){
        fprintf(stderr, "--master_addr which can not be empty  is required\n");
        return -1;
    }
    if (strcmp(argv[1], "add") == 0) {
        return ProcessNewJob();
    } else if (strcmp(argv[1], "list") == 0) {
        return ListTask();
    } else if (strcmp(argv[1], "listjob") == 0) {
        return ListJob();
    } else if (strcmp(argv[1], "listnode") == 0) {
        return ListNode();
    } else if (strcmp(argv[1], "kill") == 0) {
        return KillTask();
    } else if (strcmp(argv[1], "killjob") == 0) {
        return KillJob();
    } else if (strcmp(argv[1], "listtaskbyagent") == 0){
        return ListTaskByAgent();
    } else if (strcmp(argv[1], "updatejob") == 0) {
        return UpdateJob();
    
    } else if (strcmp(argv[1], "tagagent") == 0) {
        return TagAgent();
    }
    else {
        fprintf(stderr,"use ./galaxy_client --help for help\n");
        return -1;
    }

}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
