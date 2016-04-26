// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/cgroup/cgroup.h"
#include "protocol/galaxy.pb.h"
#include "agent/cgroup/cpu_subsystem.h"
#include "protocol/galaxy.pb.h"

#include <boost/shared_ptr.hpp>

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cout << "usage: " << argv[0] << " container_id milli_core excess_mode" << std::endl;
        return -1;
    }
    
    baidu::galaxy::cgroup::CpuSubsystem cpu_ss;
    
    
    boost::shared_ptr<baidu::galaxy::proto::Cgroup> cgroup(new baidu::galaxy::proto::Cgroup());
    baidu::galaxy::proto::CpuRequired* cr = new baidu::galaxy::proto::CpuRequired();
    cr->set_milli_core(atoi(argv[2]));
    std::string excess(argv[3]);
    if (excess == "true") {
        cr->set_excess(true);
    } else {
        cr->set_excess(false);
    }
    cgroup->set_allocated_cpu(cr);
    cgroup->set_id("1");
    
    cpu_ss.SetContainerId(argv[1]);
    cpu_ss.SetCgroup(cgroup);
    
    if (0 != cpu_ss.Construct()) {
        std::cerr << "construct subsystem" << cpu_ss.Name() << " failed\n";
    } else {
        std::cerr << "construct subsystem" << cpu_ss.Name() << " successfully\n";
    }
    
    if (0 != cpu_ss.Destroy()) {
        std::cerr << "destroy subsystem" << cpu_ss.Name() << " failed\n";
    } else {
        std::cerr << "destroy subsystem" << cpu_ss.Name() << " successfully\n";
    }
  
    return 0;
}
