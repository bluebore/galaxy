// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <stdio.h>
#include "sdk/lumia.h"
#include <tprinter.h>
#include <string_util.h>
#include "ins_sdk.h"

DEFINE_string(i, "", "Report dead minion with specify ip address");
DEFINE_string(d, "", "Import minion dict from path");
DEFINE_string(s, "", "Import system script from dir");
DECLARE_string(flagfile);
DECLARE_string(nexus_servers);
DECLARE_string(lumia_main);
DECLARE_string(lumia_root_path);

const std::string kLumiaUsage = "lumia client.\n"
                                 "Usage:\n"
                                 "    lumia report -i <minion ip>\n"
                                 "    lumia show -i <minion ip>\n"
                                 "    lumia import -d <minion dict path> -s <scripts dir>\n"
                                 "    lumia enter safemode\n"
                                 "    lumia leave safemode\n"
                                 "Options:\n"
                                 "    -i ip          Report dead minion using ip address\n"
                                 "    -d path        Import minion dict from file path\n"
                                 "    -s script dir  Import system scripts from dir\n"
                                 "Subcommand:\n"
                                 "    report         Report dead minion to lumia\n"
                                 "    show           Query the information of minion with ip\n"
                                 "    import         Import dict and script that lumia needs\n"
                                 "    enter safemode Enter safemode that lumia will suspend it'self \n"
                                 "    leave safemode Leave safemode that lumia will enter work mode\n";

bool GetLumiaAddr(std::string* lumia_addr) {
    if (lumia_addr == NULL) {
        return false;
    }
    ::galaxy::ins::sdk::SDKError err;
    ::galaxy::ins::sdk::InsSDK nexus(FLAGS_nexus_servers);
    std::string path_key = FLAGS_lumia_root_path + FLAGS_lumia_main;
    bool ok = nexus.Get(path_key, lumia_addr, &err);
    return ok;
}

int ImportData() {
    std::string lumia_addr;
    bool ok = GetLumiaAddr(&lumia_addr);
    if (!ok) {
        fprintf(stderr, "fail to get lumia addr");
        return -1;
    }
    ::baidu::lumia::LumiaSdk* lumia = ::baidu::lumia::LumiaSdk::ConnectLumia(lumia_addr);
    ok = lumia->ImportData(FLAGS_d, FLAGS_s);
    fprintf(stdout, "import data successfully\n");
    return 0;
}

int ReportDeadMinion() {
    if (FLAGS_i.empty()) {
        fprintf(stderr, "-i is required\n");
        return -1;
    }
    std::string lumia_addr;
    bool ok = GetLumiaAddr(&lumia_addr);
    if (!ok) {
        fprintf(stderr, "fail to get lumia addr");
        return -1;
    }
    ::baidu::lumia::LumiaSdk* lumia = ::baidu::lumia::LumiaSdk::ConnectLumia(lumia_addr);
    ok = lumia->ReportDeadMinion(FLAGS_i, "client report");
    if (ok) {
        fprintf(stdout, "report dead minion %s successfully\n", FLAGS_i.c_str());
        return 0;
    }
    fprintf(stderr, "fail to report dead minion %s\n", FLAGS_i.c_str());
    return -1;
}

int ShowMinion() {
    if (FLAGS_i.empty()) {
        fprintf(stderr, "-i is required\n");
        return -1;
    }
    std::string lumia_addr;
    bool ok = GetLumiaAddr(&lumia_addr);
    if (!ok) {
        fprintf(stderr, "fail to get lumia addr");
        return -1;
    }
    ::baidu::lumia::LumiaSdk* lumia = ::baidu::lumia::LumiaSdk::ConnectLumia(lumia_addr);
    std::vector<std::string> ips;
    ips.push_back(FLAGS_i);
    std::vector<std::string> place_holder;
    std::vector< ::baidu::lumia::MinionDesc> minions;
    ok = lumia->GetMinion(ips, place_holder, place_holder, & minions);
    if (!ok) {
        fprintf(stderr, "fail to get minions \n");
        return -1;
    }
    baidu::common::TPrinter tp(12);
    tp.AddRow(12, "", "id", "ip", "hostname", "cpu", "mem", "disk", "flash", "state", "rock ip", "datacenter", "healthy");
    for (uint32_t i = 0; i < minions.size(); i++) {
        std::vector<std::string> vs;
        vs.push_back(baidu::common::NumToString(i + 1));
        vs.push_back(minions[i].id);
        vs.push_back(minions[i].ip);
        vs.push_back(minions[i].hostname);

        vs.push_back(minions[i].cpu.clock + " * " + baidu::common::NumToString(minions[i].cpu.count));

        vs.push_back(baidu::common::HumanReadableString(minions[i].mem.size)+" * " + baidu::common::NumToString(minions[i].mem.count));

        vs.push_back(baidu::common::HumanReadableString(minions[i].disk.size) + " * " + baidu::common::NumToString(minions[i].disk.count));
        
        vs.push_back(baidu::common::HumanReadableString(minions[i].flash.size) + " * " + baidu::common::NumToString(minions[i].flash.count));

        vs.push_back(minions[i].state);
        vs.push_back(minions[i].rock_ip);
        vs.push_back(minions[i].datacenter);
        vs.push_back("mount:" + baidu::common::NumToString(minions[i].mount_ok) +",device:"+baidu::common::NumToString(minions[i].device_ok));
        tp.AddRow(vs);
    }
    printf("%s\n", tp.ToString().c_str());
    return 0;
}

int main(int argc, char* argv[]) {
    FLAGS_flagfile = "./lumia.flag";
    ::google::SetUsageMessage(kLumiaUsage);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    if (argc < 2) {
        fprintf(stderr, "%s", kLumiaUsage.c_str());
        return -1;
    }
    if (strcmp(argv[1], "report") == 0) {
        return ReportDeadMinion();
    } else if (strcmp(argv[1], "show") == 0) {
        return ShowMinion();
    } else if (strcmp(argv[1], "import") == 0){
        return ImportData();
    } else {
        fprintf(stderr, "%s", kLumiaUsage.c_str());
        return -1;
    }
}

