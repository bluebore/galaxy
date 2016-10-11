// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>
#include "galaxy_res_action.h"

DEFINE_string(f, "", "specify config file");
DEFINE_string(i, "", "specify container id");
DEFINE_string(p, "", "specify agent pool");
DEFINE_string(e, "", "specify agent endpoint");
DEFINE_string(t, "", "specify agent tag or token");
DEFINE_string(u, "", "username");
DEFINE_string(o, "", "operation");
DEFINE_string(a, "", "authority, split by ,");
DEFINE_int32(c, 0, "specify millicores");
DEFINE_int32(r, 0, "specify replica");
DEFINE_string(d, "", "specify disk size");
DEFINE_string(s, "", "specify ssd size");
DEFINE_string(m, "", "specify memory size");

DECLARE_string(flagfile);

const std::string kGalaxyUsage = "galaxy_res_client.\n"
                                 "galaxy_res_client [--flagfile=flagfile]\n"
                                 "Usage:\n"
                                 "  container usage:\n"
                                 "      galaxy_res_client create_container -f jobconfig(json format) [-t volum|normal]\n"
                                 "      galaxy_res_client update_container -f jobconfig(json format) -i id [-t volum|normal]\n"
                                 "      galaxy_res_client remove_container -i id\n"
                                 "      galaxy_res_client list_containers [-o cpu,mem,volums]\n"
                                 "      galaxy_res_client show_container -i id\n\n"
                                 "  agent usage:\n"
                                 "      galaxy_res_client add_agent -p pool -e endpoint\n"
                                 "      galaxy_res_client set_agent -p pool -e endpoint\n"
                                 "      galaxy_res_client show_agent -e endpoint [-o cpu,mem,volums]\n"
                                 "      galaxy_res_client remove_agent -e endpoint\n"
                                 "      galaxy_res_client list_agents [-p pool -t tag -o cpu,mem,volums]\n"
                                 "      galaxy_res_client online_agent -e endpoint\n"
                                 "      galaxy_res_client offline_agent -e endpoint\n\n"
                                 "      galaxy_res_client preempt -i container_group_id -e endpoint\n\n"
                                 "  safemode usage:\n"
                                 "      galaxy_res_client enter_safemode\n"
                                 "      galaxy_res_client leave_safemode\n\n"
                                 "  status usage:\n"
                                 "      galaxy_res_client status\n\n"
                                 "  tag usage:\n"
                                 "      galaxy_res_client create_tag -t tag -f endpoint_file\n"
                                 "      galaxy_res_client list_tags [-e endpoint]\n"
                                 "  pool usage:\n"
                                 "      galaxy_res_client list_pools -e endpoint\n\n"
                                 "  user usage:\n"
                                 "      galaxy_res_client add_user -u user -t token\n"
                                 "      galaxy_res_client remove_user -u user\n"
                                 "      galaxy_res_client list_users\n"
                                 "      galaxy_res_client show_user -u user\n"
                                 "      galaxy_res_client grant_user -u user -p pool -o [add|remove|set|clear]\n" 
                                 "                                   -a [create_container,remove_container,update_container,\n"
                                 "                                   list_containers,submit_job,remove_job,update_job,list_jobs] \n"
                                 "      galaxy_res_client assign_quota -u user -c millicores -d disk_size -s ssd_size -m memory_size -r replica\n"
                                 "Options: \n"
                                 "      -f specify config file, job config file or label config file.\n"
                                 "      -i specify container id.\n"
                                 "      -p specify agent pool.\n"
                                 "      -e specify endpoint.\n"
                                 "      -u specity user.\n"
                                 "      -t specify agent tag or token or contaier type.\n"
                                 "      -o specify operation [cpu,mem,volums].\n"
                                 "      -a specify authority split by ,\n"
                                 "      -c specify millicores, such as 1000\n"
                                 "      -r specify replica, such as 100\n"
                                 "      -d specify disk size, such as 1G\n"
                                 "      -s specify ssd size, such as 1G\n"
                                 "      -m specify memory size, such as 1G\n"
                                 "      --flagfile specify flag file, default ./galaxy.flag\n";


int main(int argc, char** argv) {
    bool ok = true;
    std::vector<char*> vec_argv;
    bool has_flagfile = false;
    ::google::SetUsageMessage(kGalaxyUsage);
    for (int i = 0; i < argc; ++i) {
        if (strncmp(argv[i], "--flagfile", 10) == 0) {
            has_flagfile = true;
            std::string strargv = argv[i];
            size_t pos = strargv.find_first_of("=");
            if (pos == std::string::npos || strargv.size() <= pos + 1) { //value is empty
                fprintf(stderr, "--flagfile= have no value\n");
                return -1;
            }
        }
        vec_argv.push_back(argv[i]);
    }

    if (!has_flagfile) {
        vec_argv.push_back(const_cast<char*>("--flagfile=./galaxy.flag"));

        char** new_argv = new char*[vec_argv.size() + 1];

        std::vector<char*>::iterator it = vec_argv.begin();
        for (int i = 0; it != vec_argv.end(); ++it, ++i) {
            char* temp = new char[strlen(*it) + 1];
            uint32_t length = strlen(strcpy(temp, *it));
            if (length != strlen(*it)) {
                fprintf(stderr, "params copy error\n");
                return -1; 
            }
            new_argv[i] = temp;
        }
        new_argv[vec_argv.size()] = '\0';
        argc = vec_argv.size();

        ::google::ParseCommandLineFlags(&argc, &new_argv, true);
    } else {
        ::google::ParseCommandLineFlags(&argc, &argv, true);
    }

    if (argc < 2) {
        fprintf(stderr, "%s", kGalaxyUsage.c_str());
        return -1;
    }

    ::baidu::galaxy::client::ResAction* resAction = new 
                ::baidu::galaxy::client::ResAction();
    if (strcmp(argv[1], "create_container") == 0) {
        if (FLAGS_f.empty()) {
            fprintf(stderr, "-f is needed\n");
            return -1;
        }
        if (FLAGS_t.empty()) {
            FLAGS_t = "normal";
        }
        ok = resAction->CreateContainerGroup(FLAGS_f, FLAGS_t);
    } else if (strcmp(argv[1], "update_container") == 0) {
        if (FLAGS_f.empty()) {
            fprintf(stderr, "-f is needed\n");
            return -1;
        }
        
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }
        if (FLAGS_t.empty()) {
            FLAGS_t = "normal";
        }
        ok = resAction->UpdateContainerGroup(FLAGS_f, FLAGS_i, FLAGS_t);
    } else if (strcmp(argv[1], "remove_container") == 0) {
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }
        ok = resAction->RemoveContainerGroup(FLAGS_i);

    } else if (strcmp(argv[1], "list_containers") == 0) {
        ok = resAction->ListContainerGroups(FLAGS_o);
    } else if (strcmp(argv[1], "show_container") == 0) {
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }
        ok = resAction->ShowContainerGroup(FLAGS_i);

    } else if (strcmp(argv[1], "add_agent") == 0) {
        //agent不存在,将agent加入到pool
        if (FLAGS_p.empty()) {
            fprintf(stderr, "-p is needed\n");
            return -1;
        }
        if (FLAGS_e.empty()) {
            fprintf(stderr, "-e is needed\n");
            return -1;
        }
        ok =  resAction->AddAgent(FLAGS_p, FLAGS_e);
    } else if (strcmp(argv[1], "set_agent") == 0) { 
        //已有agent,重置pool
        if (FLAGS_p.empty()) {
            fprintf(stderr, "-p is needed\n");
            return -1;
        }
        if (FLAGS_e.empty()) {
            fprintf(stderr, "-e is needed\n");
            return -1;
        }
        ok =  resAction->AddAgentToPool(FLAGS_e, FLAGS_p);
    } else if (strcmp(argv[1], "show_agent") == 0) { 
        if (FLAGS_e.empty()) {
            fprintf(stderr, "-e is needed\n");
            return -1;
        }
        ok = resAction->ShowAgent(FLAGS_e, FLAGS_o);
    } else if (strcmp(argv[1], "remove_agent") == 0) {
        if (FLAGS_e.empty()) {
            fprintf(stderr, "-e is needed\n");
            return -1;
        }
        ok = resAction->RemoveAgent(FLAGS_e);
    } else if (strcmp(argv[1], "list_agents") == 0) {
        if (!FLAGS_t.empty()) {
            ok = resAction->ListAgentsByTag(FLAGS_t, FLAGS_p, FLAGS_o);
        } else if (!FLAGS_p.empty()) {
            ok = resAction->ListAgentsByPool(FLAGS_p, FLAGS_o);
        } else {
            ok = resAction->ListAgents(FLAGS_o);
        }
    } else if (strcmp(argv[1], "enter_safemode") == 0) { 
        ok =  resAction->EnterSafeMode();
    } else if (strcmp(argv[1], "leave_safemode") == 0) {
        ok = resAction->LeaveSafeMode();
    } else if (strcmp(argv[1], "online_agent") == 0) {
        if (FLAGS_e.empty()) {
            fprintf(stderr, "-e is needed\n");
            return -1;
        }

        ok = resAction->OnlineAgent(FLAGS_e);
    } else if (strcmp(argv[1], "offline_agent") == 0) {
        if (FLAGS_e.empty()) {
            fprintf(stderr, "-e is needed\n");
            return -1;
        }
        ok = resAction->OfflineAgent(FLAGS_e);
    } else if (strcmp(argv[1], "preempt") == 0) {
        if (FLAGS_e.empty()) {
            fprintf(stderr, "-e is needed\n");
            return -1;
        }
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }
        ok = resAction->Preempt(FLAGS_i, FLAGS_e);
    } else if (strcmp(argv[1], "status") == 0) {
        ok = resAction->Status();
    } else if (strcmp(argv[1], "create_tag") == 0) { 
        if (FLAGS_t.empty() || FLAGS_f.empty()) {
            fprintf(stderr, "-t and -f needed\n");
            return -1;
        }
        ok = resAction->CreateTag(FLAGS_t, FLAGS_f);
    } else if (strcmp(argv[1], "list_tags") == 0) {
        if (!FLAGS_e.empty()) {
            ok = resAction->GetTagsByAgent(FLAGS_e);
        } else {
            ok = resAction->ListTags();
        }
    } else if (strcmp(argv[1], "list_pools") == 0) {
        if (FLAGS_e.empty()) {
            fprintf(stderr, "-e is needed\n");
            return -1;
        }
        ok = resAction->GetPoolByAgent(FLAGS_e);
    } else if (strcmp(argv[1], "add_user") == 0) { 
        if (FLAGS_u.empty() || FLAGS_t.empty()) {
            fprintf(stderr, "-u and -t are needed\n");
            return -1;
        }
        ok = resAction->AddUser(FLAGS_u, FLAGS_t);
    } else if (strcmp(argv[1], "remove_user") == 0) { 
        if (FLAGS_u.empty()) {
            fprintf(stderr, "-u are needed\n");
            return -1;
        }
        ok = resAction->RemoveUser(FLAGS_u);
    } else if (strcmp(argv[1], "list_users") == 0) {
        ok = resAction->ListUsers();
    } else if (strcmp(argv[1], "show_user") == 0) {
        if (FLAGS_u.empty()) {
            fprintf(stderr, "-u\n");
            return -1;
        }
        ok = resAction->ShowUser(FLAGS_u);
    } else if (strcmp(argv[1], "grant_user") == 0) {
        if (FLAGS_u.empty() || FLAGS_p.empty()) {
            fprintf(stderr, "-u, -p are needed\n");
            return -1;
        }
        
        if (FLAGS_a.empty() || FLAGS_o.empty()) {
            fprintf(stderr, "-a and -o are needed\n");
            return -1;
        }

        ok =  resAction->GrantUser(FLAGS_u, FLAGS_p, FLAGS_o, FLAGS_a);

    } else if (strcmp(argv[1], "assign_quota") == 0) {
        if (FLAGS_s.empty() || FLAGS_d.empty() || FLAGS_m.empty() || FLAGS_u.empty()) {
            fprintf(stderr, "-u, -s, -d, -m and -c are needed\n");
            return -1;
        }
        ok = resAction->AssignQuota(FLAGS_u, FLAGS_c, FLAGS_m, FLAGS_d, FLAGS_s, FLAGS_r);
    } else {
        fprintf(stderr, "%s", kGalaxyUsage.c_str());
        return -1;
    }

    if (ok) {
        return 0;
    } else {
        return -1;
    }
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
