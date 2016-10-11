// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "client/galaxy_util.h"

int main(int argc, char** argv) {
    baidu::galaxy::sdk::JobDescription job;
    int ok = baidu::galaxy::client::BuildJobFromConfig("./app.json", &job);
    if (ok != 0) {
        return -1;
    }

    std::cout << "  name: " << job.name << std::endl;
    std::cout << "  type: " << job.type << std::endl;
    //std::cout << "  version: " << job.version << std::endl;
    //std::cout << "  run_user: " << job.run_user << std::endl;
    
    std::cout << "  deploy: " << std::endl;

    std::cout << "      replica: " << job.deploy.replica << std::endl;
    std::cout << "      step: " << job.deploy.step << std::endl;
    std::cout << "      interval: " << job.deploy.interval << std::endl;
    std::cout << "      max_per_host: " << job.deploy.max_per_host << std::endl;
    std::cout << "      tag: " << job.deploy.tag << std::endl;
    std::string pools;
    for (size_t i=0; i < job.deploy.pools.size(); ++i) {
        pools += job.deploy.pools[i] + ",";
    }
    std::cout << "      pools: " << pools << std::endl;


    std::cout << "  pod: " << std::endl;
    std::cout << "      workspace_volum: " << std::endl;
    std::cout << "          size: " << job.pod.workspace_volum.size << std::endl;
    std::cout << "          type: " << job.pod.workspace_volum.type << std::endl;
    std::cout << "          medium: " << job.pod.workspace_volum.medium << std::endl;
    //std::cout << "          source_path: " << job.pod.workspace_volum.source_path << std::endl;
    std::cout << "          dest_path: " << job.pod.workspace_volum.dest_path << std::endl;
    std::cout << "          readonly: " << job.pod.workspace_volum.readonly << std::endl;
    std::cout << "          exclusive: " << job.pod.workspace_volum.exclusive << std::endl;
    std::cout << "          use_symlink: " << job.pod.workspace_volum.use_symlink << std::endl;

    std::cout << "      data_volum: " << job.pod.data_volums.size() << std::endl;
    for (size_t i = 0; i< job.pod.data_volums.size(); ++i) {
        std::cout << "          data_volums No." << i << std::endl;
        std::cout << "              size: " << job.pod.data_volums[i].size << std::endl;
        std::cout << "              type: " << job.pod.data_volums[i].type << std::endl;
        std::cout << "              medium: " << job.pod.data_volums[i].medium << std::endl;
        //std::cout << "              source_path: " << job.pod.data_volums[i].source_path << std::endl;
        std::cout << "              dest_path: " << job.pod.data_volums[i].dest_path << std::endl;
        std::cout << "              readonly: " << job.pod.data_volums[i].readonly << std::endl;
        std::cout << "              exclusive: " << job.pod.data_volums[i].exclusive << std::endl;
        std::cout << "              use_symlink: " << job.pod.data_volums[i].use_symlink << std::endl;
    }

    std::cout << "      tasks: " << job.pod.tasks.size() << std::endl;
    for (size_t i = 0; i < job.pod.tasks.size(); ++i) {
        std::cout << "          task No." << i << std::endl;
        std::cout << "              id: " << job.pod.tasks[i].id << std::endl;
        std::cout << "              cpu: " << std::endl;
        std::cout << "                  millicores: " << job.pod.tasks[i].cpu.milli_core << std::endl;
        std::cout << "                  excess: " << job.pod.tasks[i].cpu.excess << std::endl;
        std::cout << "              mem: " << std::endl;
        std::cout << "                  size: " << job.pod.tasks[i].memory.size << std::endl;
        std::cout << "                  excess: " << job.pod.tasks[i].memory.excess << std::endl;

        std::cout << "              tcp: " << std::endl;
        std::cout << "                  recv_bps_quota: " << job.pod.tasks[i].tcp_throt.recv_bps_quota << std::endl;
        std::cout << "                  recv_bps_excess: " << job.pod.tasks[i].tcp_throt.recv_bps_excess << std::endl;
        std::cout << "                  send_bps_quota: " << job.pod.tasks[i].tcp_throt.send_bps_quota << std::endl;
        std::cout << "                  send_bps_excess: " << job.pod.tasks[i].tcp_throt.send_bps_excess << std::endl;


        std::cout << "              blkio: " << std::endl;
        std::cout << "                  weight: " << job.pod.tasks[i].blkio.weight << std::endl;

        std::cout << "              ports: " << job.pod.tasks[i].ports.size()<< std::endl;
        for (size_t j = 0; j < job.pod.tasks[i].ports.size(); ++j) {
            std::cout << "              port No." << j << std::endl;
            std::cout << "                  port_name: " << job.pod.tasks[i].ports[j].port_name << std::endl;
            std::cout << "                  port: " << job.pod.tasks[i].ports[j].port << std::endl;
        }

        std::cout << "              exe_package: " << std::endl;
        std::cout << "                  start_cmd: " 
                  << job.pod.tasks[i].exe_package.start_cmd << std::endl;
        std::cout << "                  stop_cmd: " 
                  << job.pod.tasks[i].exe_package.stop_cmd << std::endl;
        std::cout << "                  health_cmd: "
                  << job.pod.tasks[i].exe_package.health_cmd << std::endl;
        std::cout << "                  source_path: " 
                  << job.pod.tasks[i].exe_package.package.source_path << std::endl;
        std::cout << "                  dest_path: " 
                  << job.pod.tasks[i].exe_package.package.dest_path << std::endl;
        std::cout << "                  version: " 
                  << job.pod.tasks[i].exe_package.package.version << std::endl;

        std::cout << "              data_package: " << std::endl;
        std::cout << "                  reload_cmd: " 
                  << job.pod.tasks[i].data_package.reload_cmd << std::endl;
        std::cout << "                  packages: " 
                  << job.pod.tasks[i].data_package.packages.size() << std::endl;
        for (size_t j = 0; j < job.pod.tasks[i].data_package.packages.size(); ++j) {
            std::cout << "                  package No." << i << std::endl;
            std::cout << "                      source_path:" 
                      << job.pod.tasks[i].data_package.packages[j].source_path << std::endl;
            std::cout << "                      dest_path:" 
                      << job.pod.tasks[i].data_package.packages[j].dest_path << std::endl;
            std::cout << "                      version:" 
                      << job.pod.tasks[i].data_package.packages[j].version << std::endl;
        }
        
        std::cout << "              services: " 
                  << job.pod.tasks[i].services.size() << std::endl;
        for (size_t j = 0; j < job.pod.tasks[i].services.size(); ++j) {
            std::cout << "                  service No." << i << std::endl;
            std::cout << "                      service_name:" 
                      << job.pod.tasks[i].services[j].service_name << std::endl;
            std::cout << "                      port_name:" 
                      << job.pod.tasks[i].services[j].port_name  << std::endl;
            std::cout << "                      use_bns:" 
                      << job.pod.tasks[i].services[j].use_bns << std::endl;
        }

    }
    return ok;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
