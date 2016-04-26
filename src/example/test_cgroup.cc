// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/cgroup/cgroup.h"
#include "agent/cgroup/cpu_subsystem.h"
#include "agent/cgroup/freezer_subsystem.h"
#include "agent/cgroup/memory_subsystem.h"
#include "agent/cgroup/subsystem_factory.h"
#include "agent/cgroup/tcp_throt_subsystem.h"
#include "protocol/galaxy.pb.h"

#include <boost/shared_ptr.hpp>

#include <iostream>

int main(int argc, char** argv) {
    boost::shared_ptr<baidu::galaxy::cgroup::SubsystemFactory> factory(new baidu::galaxy::cgroup::SubsystemFactory);
    factory->Register(new baidu::galaxy::cgroup::CpuSubsystem())
        ->Register(new baidu::galaxy::cgroup::MemorySubsystem())
        ->Register(new baidu::galaxy::cgroup::FreezerSubsystem)
        ->Register(new baidu::galaxy::cgroup::TcpThrotSubsystem);
    
    boost::shared_ptr<baidu::galaxy::cgroup::Cgroup> cgroup(new baidu::galaxy::cgroup::Cgroup(factory));
    
    boost::shared_ptr<baidu::galaxy::proto::Cgroup> description(new baidu::galaxy::proto::Cgroup());
    baidu::galaxy::proto::CpuRequired* cpu_required = new baidu::galaxy::proto::CpuRequired();
    cpu_required->set_milli_core(1000);
    cpu_required->set_excess(false);
    description->set_allocated_cpu(cpu_required);
    
    baidu::galaxy::proto::MemoryRequired* memory_required = new baidu::galaxy::proto::MemoryRequired();
    memory_required->set_excess(false);
    memory_required->set_size(1024);
    description->set_allocated_memory(memory_required);
    description->set_id("cgroup_id");
    
    cgroup->SetDescrition(description);
    cgroup->SetContainerId("container_id");
    
    if (0 != cgroup->Construct()) {
        std::cerr << "construct cgroup failed" << std::endl;
    } else {
        std::cerr << "construct cgroup successfully" << std::endl;
    }
    
    if (0 != cgroup->Destroy()) {
        std::cerr << "destroy cgroup failed" << std::endl;
    } else {
        std::cerr << "destroy cgroup successfully" << std::endl;
    }
    
    return 0;
    
}
