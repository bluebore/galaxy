// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/container/container.h"
#include "protocol/galaxy.pb.h"
#include "agent/cgroup/subsystem_factory.h"
#include "agent/util/path_tree.h"

#include <iostream>

int main(int argc, char** argv)
{
    baidu::galaxy::path::SetRootPath("/tmp");

    baidu::galaxy::cgroup::SubsystemFactory::GetInstance()->Setup();

    baidu::galaxy::proto::ContainerDescription cd;
    cd.set_run_user("galaxy");
    baidu::galaxy::proto::VolumRequired* wv = new baidu::galaxy::proto::VolumRequired();
    wv->set_source_path("/home/disk1");            // agent
    wv->set_source_path("/home/disk1");            // agent
    wv->set_exclusive(false);                      // rm
    wv->set_readonly(false);                       // rm
    wv->set_medium(baidu::galaxy::proto::kSsd);    // rm
    wv->set_size(1000000);                         // rm & agent
    wv->set_use_symlink(true);
    cd.set_allocated_workspace_volum(wv);

    baidu::galaxy::proto::VolumRequired* dv = cd.add_data_volums();
    dv->set_source_path("/home/disk2");            // agent
    dv->set_dest_path("/home/disk3");              // aget
    dv->set_exclusive(false);                      // rm
    dv->set_readonly(false);                       // rm
    dv->set_medium(baidu::galaxy::proto::kSsd);    // rm
    dv->set_size(1000000);                         // rm & agent
    dv->set_use_symlink(true);

    baidu::galaxy::proto::Cgroup* cg = cd.add_cgroups();
    cg->set_id("cgroup_id1");

    baidu::galaxy::proto::CpuRequired* cpu_required = new baidu::galaxy::proto::CpuRequired();
    cpu_required->set_milli_core(1000);
    cpu_required->set_excess(false);
    cg->set_allocated_cpu(cpu_required);

    baidu::galaxy::proto::MemoryRequired* memory_required = new baidu::galaxy::proto::MemoryRequired();
    memory_required->set_excess(false);
    memory_required->set_size(1024);
    cg->set_allocated_memory(memory_required);

    baidu::galaxy::container::ContainerId id("group_id", "container_id");
    baidu::galaxy::container::Container container(id, cd);
    if (0 != container.Construct().Code()) {
        std::cout << "construct container fail" << std::endl;
        //container.Destroy();
        return -1;
    }

    /*    if (0 != container.Destroy()) {
            std::cout << "destroy container fail" << std::endl;
            return -1;
        }*/
    return 0;
}
