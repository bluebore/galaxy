// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/cgroup/cgroup.h"
#include "protocol/galaxy.pb.h"
#include "agent/cgroup/memory_subsystem.h"
#include "protocol/galaxy.pb.h"
#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"

#include <boost/shared_ptr.hpp>

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: " << argv[0] << " container_id cgroup_id size_in_byte excess_mode" << std::endl;
        return -1;
    }
    
    std::string container_id = argv[1];
    std::string cgroup_id = argv[2];
    int size_in_byte = atoi(argv[3]);
    bool excess = strcmp(argv[4], "true") == 0;
    pid_t pid = atoi(argv[5]);
    
    std::cerr << "container_id: " << container_id << "\n"
            << "cgroup_id: " << cgroup_id << "\n"
            << "size_in_byte: " << size << "\n"
            << "excess: " << excess << "\n";
    
    baidu::galaxy::cgroup::MemorySubsystem subsystem;
    
    
    boost::shared_ptr<baidu::galaxy::proto::Cgroup> cgroup(new baidu::galaxy::proto::Cgroup());
    baidu::galaxy::proto::MemoryRequired* mr = new baidu::galaxy::proto::MemoryRequired();
    mr->set_size(size_in_byte);
    mr->set_excess(excess);
    
    cgroup->set_allocated_memory(mr);
    cgroup->set_id(cgroup_id);
    
    subsystem.SetContainerId(container_id);
    subsystem.SetCgroup(cgroup);
    
    if (0 != subsystem.Construct()) {
        std::cerr << "construct subsystem" << subsystem.Name() << " failed\n";
        subsystem.Destroy();
    } else {
        std::cerr << "construct subsystem" << subsystem.Name() << " successfully\n";
    }
  
    return 0;
}
