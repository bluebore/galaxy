/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file container_description.cc
 * @author haolifei(com@baidu.com)
 * @date 2016/11/03 18:59:57
 * @brief 
 *  
 **/

#include "ins_sdk.h"
#include "protocol/appmaster.pb.h"
#include "boost/shared_ptr.hpp"
#include <gflags/gflags.h>
#include <stdio.h>
#include <iostream>

DEFINE_string(nexus_addr, "", "");
DEFINE_string(nexus_path, "", "");

void PrintHelp(const char* argv0);

int main(int argc, char** argv) {

    if (argc <=1 ) {
        PrintHelp(argv[0]);
        exit(-1);
    }

    google::ParseCommandLineFlags(&argc, &argv, true);
    boost::shared_ptr<galaxy::ins::sdk::InsSDK> nexus(new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_addr));

    const std::string start_key = FLAGS_nexus_path;
    const std::string end_key = FLAGS_nexus_path + "~";

    ::galaxy::ins::sdk::ScanResult* result
        = nexus->Scan(start_key, end_key);

    while (!result->Done()) {
        const std::string& value = result->Value();

        baidu::galaxy::proto::UserMeta user;
        if (user.ParseFromString(value)) {
            std::cout << user.user().user() << " " << user.user().token() << std::endl;
        }

        result->Next();
    }
    delete result;

    return 0;
}


void PrintHelp(const char* argv0) {
    std::cout << "usage: " << argv0 << " --nexus_addr=xx nexus_path=xx\n";
}
