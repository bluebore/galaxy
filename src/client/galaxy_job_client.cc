// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include "galaxy_job_action.h"

DEFINE_string(f, "", "specify config file");
DEFINE_string(i, "", "specify job id");
DEFINE_string(I, "", "specify pod id");
DEFINE_string(n, "", "specify job name");
DEFINE_string(c, "", "specify cmd");
DEFINE_int32(t, 0, "specify task num or update breakpoint");
DEFINE_int32(d, 1, "specify data_volums num");
DEFINE_int32(p, 1, "specify port num");
DEFINE_int32(a, 1, "specify packages num in data_package");
DEFINE_int32(s, 1, "specify service num");
DEFINE_string(o, "", "operation");
DEFINE_bool(b, false, "show base meta info");

DECLARE_string(flagfile);

const std::string kGalaxyUsage = "galaxy_client.\n"
                                 "galaxy_client [--flagfile=flagfile]\n"
                                 "Usage:\n"
                                 "      galaxy_client submit -f jobconfig(json format)\n"
                                 "      galaxy_client update -f jobconfig(json format) -i id [-t breakpoint -o pause|continue|rollback]\n"
                                 //"      galaxy_client stop -i id\n"
                                 "      galaxy_client remove -i id\n"
                                 "      galaxy_client list [-o cpu,mem,volums]\n"
                                 "      galaxy_client cal -f jobconfig(json format) [-o cpu,mem,volums]\n"
                                 "      galaxy_client show -i id [-o cpu,mem,volums -b(show meta)]\n"
                                 "      galaxy_client recover -i id -I podid\n"
                                 "      galaxy_client exec -i id -c cmd\n"
                                 "      galaxy_client json [-i jobid -n jobname -t num_task -d num_data_volums -p num_port -a num_packages in data_package -s num_service]\n"
                                 "Options: \n"
                                 "      -f specify config file, job config file or label config file.\n"
                                 "      -c specify cmd.\n"
                                 "      -i specify job id.\n"
                                 "      -I specify pod id.\n"
                                 "      -t specify specify task num or update breakpoint, default 0.\n"
                                 "      -d spicify data_volums num, default 1\n"
                                 "      -p specify port num, default 1\n"
                                 "      -a specify packages num in data_package, default 1\n"
                                 "      -s specify service num, default 1\n"
                                 "      -n specify job name\n"
                                 "      -o specify operation.\n"
                                 "      -b specify show base meta info\n"
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

    ::baidu::galaxy::client::JobAction* jobAction = new 
                ::baidu::galaxy::client::JobAction();
    if (strcmp(argv[1], "submit") == 0) {
        if (FLAGS_f.empty()) {
            fprintf(stderr, "-f is needed\n");
            return -1;
        }
        ok = jobAction->SubmitJob(FLAGS_f);
    } else if (strcmp(argv[1], "update") == 0) {
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }

        ok = jobAction->UpdateJob(FLAGS_f, FLAGS_i, FLAGS_o, FLAGS_t);
    } else if (strcmp(argv[1], "remove") == 0) {
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }
        ok = jobAction->RemoveJob(FLAGS_i);

    } else if (strcmp(argv[1], "list") == 0) {
        ok = jobAction->ListJobs(FLAGS_o);
    } else if (strcmp(argv[1], "cal") == 0) {
        ok = jobAction->CalRes(FLAGS_f, FLAGS_o);
    } else if (strcmp(argv[1], "stop") == 0) { 
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }

        ok = jobAction->StopJob(FLAGS_i);
    } else if (strcmp(argv[1], "show") == 0) {
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }
        ok = jobAction->ShowJob(FLAGS_i, FLAGS_o, FLAGS_b);
    
    } else if (strcmp(argv[1], "recover") == 0) {
        if (FLAGS_i.empty() || FLAGS_I.empty()) {
            fprintf(stderr, "-i or -I needed\n");
            return false;
        }
        ok = jobAction->RecoverInstance(FLAGS_i, FLAGS_I);
    } else if (strcmp(argv[1], "exec") == 0) {
        if (FLAGS_i.empty()) {
            fprintf(stderr, "-i is needed\n");
            return -1;
        }

        if (FLAGS_c.empty()) {
            fprintf(stderr, "-c is needed\n");
            return -1;
        }
        ok = jobAction->ExecuteCmd(FLAGS_i, FLAGS_c);
    } else if (strcmp(argv[1], "json") == 0) { 
        if (FLAGS_i.empty()) {
            ok = ::baidu::galaxy::client::GenerateJson(FLAGS_t, 
                                                       FLAGS_d,
                                                       FLAGS_p, 
                                                       FLAGS_a, 
                                                       FLAGS_s,
                                                       FLAGS_n
                                                      ); 
        } else {
            ok = jobAction->GenerateJson(FLAGS_i);
        }
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
