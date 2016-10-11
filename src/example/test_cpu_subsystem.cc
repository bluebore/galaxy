// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/cgroup/cgroup.h"
#include "protocol/galaxy.pb.h"
#include "agent/cgroup/cpu_subsystem.h"
#include "protocol/galaxy.pb.h"
#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"

#include <boost/shared_ptr.hpp>

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: " << argv[0] << " container_id cgroup_id milli_core excess_mode" << std::endl;
        return -1;
    }
    
    std::string container_id = argv[1];
    std::string cgroup_id = argv[2];
    int millicore = atoi(argv[3]);
    bool excess = strcmp(argv[4], "true") == 0;
    
    std::cerr << "container_id: " << container_id << "\n"
            << "cgroup_id: " << cgroup_id << "\n"
            << "millicore: " << millicore << "\n"
            << "excess: " << excess << "\n";
    
    baidu::galaxy::cgroup::CpuSubsystem cpu_ss;
    
    
    boost::shared_ptr<baidu::galaxy::proto::Cgroup> cgroup(new baidu::galaxy::proto::Cgroup());
    baidu::galaxy::proto::CpuRequired* cr = new baidu::galaxy::proto::CpuRequired();
    cr->set_milli_core(millicore);
    cr->set_excess(excess);
    
    cgroup->set_allocated_cpu(cr);
    cgroup->set_id(cgroup_id);
    
    cpu_ss.SetContainerId(container_id);
    cpu_ss.SetCgroup(cgroup);
    
    baidu::galaxy::util::ErrorCode err =  cpu_ss.Construct();
    if (err.Code() != 0) {
        std::cerr << "construct subsystem" << cpu_ss.Name() 
            << " failed: " << err.Message();
        cpu_ss.Destroy();
    } else {
        std::cerr << "construct subsystem" << cpu_ss.Name() << " successfully\n";
    }
    return 0;
}
