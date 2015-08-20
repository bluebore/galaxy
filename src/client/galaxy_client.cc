// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sdk/galaxy.h"

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <string.h>
#include <gflags/gflags.h>

#include <tprinter.h>
#include <string_util.h>

DECLARE_string(master_host);
DECLARE_string(master_port);
DECLARE_string(flagfile);

const std::string kGalaxyUsage = "\n./galaxy_client submit <job_name> <job_package> <replica> <cpu> <mem> <start_cmd> <batch> <deploy_step>\n" 
                                 "./galaxy_client list\n"
                                 "./galaxy_client listagent\n"
                                 "./galaxy_client kill <jobid>\n"
                                 "./galaxy_client label <label_name> <agent_endpoints_file>";

bool LoadAgentEndpointsFromFile(
        const std::string& file_name, 
        std::vector<std::string>* agents) {
    const int LINE_BUF_SIZE = 1024;
    char line_buf[LINE_BUF_SIZE];
    std::ifstream fin(file_name.c_str(), std::ifstream::in);        
    if (!fin.is_open()) {
        fprintf(stderr, "open %s failed\n", file_name.c_str());
        return false; 
    }
    
    bool ret = true;
    while (fin.good()) {
        fin.getline(line_buf, LINE_BUF_SIZE);     
        if (fin.gcount() == LINE_BUF_SIZE) {
            fprintf(stderr, "line buffer size overflow\n");     
            ret = false;
            break;
        } else if (fin.gcount() == 0) {
            continue; 
        }
        fprintf(stdout, "label %s\n", line_buf);
        // NOTE string size should == strlen
        std::string agent_endpoint(line_buf, strlen(line_buf));
        agents->push_back(agent_endpoint);
    }
    if (!ret) {
        fin.close();
        return false;
    }

    if (fin.fail()) {
        fin.close(); 
        return false;
    }
    fin.close(); 
    return true;
}

int LabelAgent(int argc, char* argv[]) {
    if (argc < 1) {
        return -1; 
    }
    std::string label(argv[0]);
    std::vector<std::string> agent_endpoints;
    if (argc == 2 && LoadAgentEndpointsFromFile(argv[1], &agent_endpoints)) {
        return -1;     
    }
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_master_host + ":" + FLAGS_master_port);
    if (!galaxy->LabelAgents(label, agent_endpoints)) {
        return -1; 
    }
    return 0;
}

void ReadBinary(const std::string& file, std::string* binary) {
    FILE* fp = fopen(file.c_str(), "rb");
    if (fp == NULL) {
        fprintf(stderr, "Read binary fail\n");
        return;
    }
    char buf[1024];
		int len = 0;
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        binary->append(buf, len);
    }
		fclose(fp);
}

int AddJob(int argc, char* argv[]) {
    if (argc < 5) {
        return 1;
    }
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_master_host + ":" + FLAGS_master_port);
    baidu::galaxy::JobDescription job;
    job.job_name = argv[0];
    ReadBinary(argv[1], &job.binary);
    printf("binary size %lu\n", job.binary.size());
    job.replica = atoi(argv[2]);
    job.cpu_required = atoi(argv[3]);
    job.mem_required = atoi(argv[4]);
    job.cmd_line = argv[5];
    job.is_batch = (argc > 6 && 0 == strcmp(argv[6], "batch"));
    job.deploy_step = job.replica;
    if (argc > 7) {
        job.deploy_step = atoi(argv[7]);
    }
    std::string jobid = galaxy->SubmitJob(job);
    if (jobid.empty()) {
        fprintf(stderr, "Submit job fail\n");
        return 1;
    }
    printf("Submit job %s\n", jobid.c_str());
    return 0;
}
int ListAgent(int argc, char* argv[]) {
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_master_host + ":" + FLAGS_master_port);
    std::vector<baidu::galaxy::NodeDescription> agents;
    baidu::common::TPrinter tp(10);
    tp.AddRow(10, "", "addr", "pods", "cpu_used", "cpu_assigned", "cpu_total", "mem_used", "mem_assigned", "mem_total", "labels");
    if (galaxy->ListAgents(&agents)) {
        for (uint32_t i = 0; i < agents.size(); i++) {
            std::vector<std::string> vs;
            vs.push_back(baidu::common::NumToString(i + 1));
            vs.push_back(agents[i].addr);
            vs.push_back(baidu::common::NumToString(agents[i].task_num));
            vs.push_back(baidu::common::NumToString(agents[i].cpu_used));
            vs.push_back(baidu::common::NumToString(agents[i].cpu_assigned));
            vs.push_back(baidu::common::NumToString(agents[i].cpu_share));
            vs.push_back(baidu::common::NumToString(agents[i].mem_used));
            vs.push_back(baidu::common::NumToString(agents[i].mem_assigned));
            vs.push_back(baidu::common::NumToString(agents[i].mem_share));
            vs.push_back(agents[i].labels);
            tp.AddRow(vs);
		}
        printf("%s\n", tp.ToString().c_str());
				return 0;
		}
		printf("Listagent fail\n");
		return 1;
}
 
int ListJob(int argc, char* argv[]) {
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_master_host + ":" + FLAGS_master_port);
    std::vector<baidu::galaxy::JobInformation> infos;
    baidu::common::TPrinter tp(8);
    tp.AddRow(8, "", "id", "name", "stat(r/p/d)", "replica", "batch", "cpu", "memory");
    if (galaxy->ListJobs(&infos)) {
        for (uint32_t i = 0; i < infos.size(); i++) {
            std::vector<std::string> vs;
            vs.push_back(baidu::common::NumToString(i + 1));
            vs.push_back(infos[i].job_id);
            vs.push_back(infos[i].job_name);
            vs.push_back(baidu::common::NumToString(infos[i].running_num) + "/" + 
                         baidu::common::NumToString(infos[i].pending_num) + "/" +
                         baidu::common::NumToString(infos[i].deploying_num));
            vs.push_back(baidu::common::NumToString(infos[i].replica));
						vs.push_back(infos[i].is_batch ? "batch" : "");
            vs.push_back(baidu::common::NumToString(infos[i].cpu_used));
            vs.push_back(baidu::common::NumToString(infos[i].mem_used));
            tp.AddRow(vs);
        }
        printf("%s\n", tp.ToString().c_str());
        return 0;
    }
    fprintf(stderr, "List fail\n");
    return 1;
}
int main(int argc, char* argv[]) {
    FLAGS_flagfile = "./galaxy.flag";
    ::google::SetUsageMessage(kGalaxyUsage);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    if(argc < 2){
        fprintf(stderr,"Usage:%s\n", kGalaxyUsage.c_str());
        return -1;
    }
    if (strcmp(argv[1], "submit") == 0) {
        return AddJob(argc - 2, argv + 2);
    } else if (strcmp(argv[1], "list") == 0) {
        return ListJob(argc - 2, argv + 2);
    } else if (strcmp(argv[1], "listagent") == 0) {
		return ListAgent(argc - 2, argv + 2);
    } else if (strcmp(argv[1], "label") == 0) {
        return LabelAgent(argc - 2, argv + 2);
    } else {
        fprintf(stderr,"Usage:%s\n", kGalaxyUsage.c_str());
        return -1;
    }
    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
