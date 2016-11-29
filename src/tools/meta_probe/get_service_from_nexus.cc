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
#include "protocol/galaxy.pb.h"
#include "boost/shared_ptr.hpp"
#include <gflags/gflags.h>
#include <stdio.h>
#include <iostream>
#include <sstream>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>

DEFINE_string(nexus_addr, "", "");
DEFINE_string(container_group_path, "", "");
DEFINE_string(service_path, "", "");

void PrintHelp(const char* argv0);


int LoadFromNexus(boost::shared_ptr<galaxy::ins::sdk::InsSDK> nexus, 
            const std::string& path, 
            std::vector<std::string>& data);

struct MetrixInfo {
    std::string id;
    std::string naming_node;
    std::string naming_token;
    std::vector<std::string> metrix;

    std::string ToString() {
        std::stringstream ss;
        ss << id << " " << naming_node << " " << naming_token;
        for (size_t i = 0; i < metrix.size(); i++) {
            std::string path = metrix[i];
             boost::algorithm::to_upper(path);
             boost::algorithm::replace_all(path, "/", "_");
             ss << " " << "GALAXY_DISK" << path << "_USED";
             ss << " " << "GALAXY_DISK" << path << "_TOTAL";
        }

        ss << " GALAXY_CPU_USED_IN_MILLICORE"
            << " GALAXY_CPU_USAGE"
            << " GALAXY_CPU_QUOTA_IN_MILLICORE"
            << " GALAXY_MEM_USED"
            << " GALAXY_MEM_USED_PERCENT"
            << " GALAXY_MEM_TOTAL";
        return ss.str();;
    }
};

int main(int argc, char** argv) {

    if (argc <=1 ) {
        PrintHelp(argv[0]);
        exit(-1);
    }

    google::ParseCommandLineFlags(&argc, &argv, true);
    boost::shared_ptr<galaxy::ins::sdk::InsSDK> nexus(new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_addr));

    // get_volum_paths
    std::map<std::string, std::vector<std::string> > volum_paths;
    std::vector<std::string> cg_str;
    if (0 != LoadFromNexus(nexus, FLAGS_container_group_path, cg_str)) {
        return -1;
    }


    for (size_t i = 0; i < cg_str.size(); i++) {
        baidu::galaxy::proto::ContainerGroupMeta cgm;
        if (cgm.ParseFromString(cg_str[i])) {
            if (cgm.has_desc()) {
                baidu::galaxy::proto::ContainerDescription desc = cgm.desc();
                if (desc.has_container_type() && desc.container_type() == baidu::galaxy::proto::kVolumContainer) {
                    std::vector<std::string> vpath;
                    vpath.push_back(desc.workspace_volum().dest_path());

                    for (int i = 0; i < desc.data_volums_size(); i++) {
                        vpath.push_back(desc.data_volums(i).dest_path());
                    }
                    volum_paths[cgm.id()] = std::vector<std::string>();
                    volum_paths[cgm.id()].swap(vpath);
                }
            }
        }
    }



    // get_services
    std::vector<std::string> vservice_str;
    if (0 != LoadFromNexus(nexus, FLAGS_service_path, vservice_str)) {
        return -1;
    }


    std::vector<boost::shared_ptr<MetrixInfo> > vminfo;
    for (size_t i = 0; i < vservice_str.size(); i++) {
        baidu::galaxy::proto::JobInfo job;
        if (job.ParseFromString(vservice_str[i])) {
            baidu::galaxy::proto::PodDescription pod = job.desc().pod();
            for (int j = 0; j < pod.tasks_size(); j++) {
                if (pod.tasks(j).services_size() > 0
                            && pod.tasks(j).services(0).has_use_bns()
                            && pod.tasks(j).services(0).use_bns()) {

                    boost::shared_ptr<MetrixInfo> mi(new MetrixInfo);
                    mi->id = job.jobid().c_str();
                    mi->naming_node = pod.tasks(j).services(0).service_name().c_str();
                    mi->naming_token = pod.tasks(j).services(0).token().c_str();

                    mi->metrix.push_back(pod.workspace_volum().dest_path());
                    for (int x = 0; x < pod.data_volums_size(); x++) {
                        mi->metrix.push_back(pod.data_volums(x).dest_path());
                    }

                    for (int k = 0; k < job.desc().volum_jobs_size(); k++) {
                        const std::vector<std::string>& p = volum_paths[job.desc().volum_jobs(k)];
                        for (size_t m = 0; m < p.size(); m++) {
                            mi->metrix.push_back(p[m]); 
                        }
                    }
                    vminfo.push_back(mi);
                    break;
                }
            }
        }
    }

    for (size_t i = 0; i < vminfo.size(); i++) {
        std::cout << vminfo[i]->ToString() << std::endl;
    }

    return 0;
}


void PrintHelp(const char* argv0) {
    std::cout << "usage: " << argv0 << " --nexus_addr=xx nexus_path=xx\n";
}

int LoadFromNexus(boost::shared_ptr<galaxy::ins::sdk::InsSDK> nexus, 
            const std::string& path, 
            std::vector<std::string>& data) {

    int i = 0;
    const std::string start_key = path;
    const std::string end_key = path + "~";
    ::galaxy::ins::sdk::ScanResult* result
        = nexus->Scan(start_key, end_key);
    while (!result->Done()) {
        data.push_back(result->Value());
        result->Next();
        i++;
        if (i > 5000) {
            delete result;
            return -1;
        }
    }
    delete result;
    return 0;
}
