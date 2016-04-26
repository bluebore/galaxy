// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "container.h"
#include "cgroup/cgroup.h"
#include "protocol/galaxy.pb.h"
#include "volum/volum_group.h"
#include "agent/util/user.h"
#include "agent/util/path_tree.h"

#include <unistd.h>

namespace baidu {
namespace galaxy {
namespace container {

Container::Container(const baidu::galaxy::proto::ContainerDescription& desc) :
    desc_(desc),
    volum_group_(new baidu::galaxy::volum::VolumGroup()) {
}

Container::~Container() {
}

std::string Container::Id() const {
    return "";
}

int Container::Construct() {
    // cgroup
    for (int i = 0; i < desc_.cgroups_size(); i++) {
        boost::shared_ptr<baidu::galaxy::cgroup::Cgroup> cg;
        boost::shared_ptr<baidu::galaxy::proto::Cgroup> desc(new baidu::galaxy::proto::Cgroup);
        desc->CopyFrom(desc_.cgroups(i));
        cg->SetContainerId("");
        cg->SetDescrition(desc);

        if (0 != cg->Construct()) {
            break;
        }

        cgroup_.push_back(cg);
    }

    if (cgroup_.size() != (unsigned int)desc_.cgroups_size()) {
        for (size_t i = 0; i < cgroup_.size(); i++) {
            cgroup_[i]->Destroy();
        }

        return -1;
    }

    // volum
    volum_group_->SetContainerId("");
    volum_group_->SetWorkspaceVolum(desc_.workspace_volum());

    for (int i = 0; i < desc_.data_volums_size(); i++) {
        volum_group_->AddDataVolum(desc_.data_volums(i));
    }

    if (0 != volum_group_->Construct()) {
        return -1;
    }

    // clone
    return 0;
}


int Container::RunRoutine(Container* container) {
    assert(NULL != container);

    // mount root fs
    if (0 != baidu::galaxy::volum::VolumGroup::MountRootfs()) {
        return -1;
    }

    // mount workspace & datavolum
    if (0 != container->volum_group_->Mount(container->desc_.run_user())) {
        return -1;
    }

    // change root
    if(0 != ::chroot(baidu::galaxy::path::ContainerRootPath(container->Id()).c_str())) {
        return -1;
    }
    
    // change user or sh -l
    if(!baidu::galaxy::user::Su(container->desc_.run_user())) {
        return -1;
    }
    
    // start appworker 
    char* argv[] = {
        const_cast<char*>("sh"),
        const_cast<char*>("-c"),
        const_cast<char*>(container->desc_.cmd_line().c_str()),
        NULL};
    ::execv("/bin/sh", argv);

    return 0;
}

int Container::Destroy() {
    // destroy cgroup
    for (size_t i = 0; i < cgroup_.size(); i++) {
        if (0 != cgroup_[i]->Destroy()) {
            return -1;
        }
    }

    // destroy volum
    // workspace backup
    // datavolum destroy
    return 0;
}

int Container::Run() {
    return 0;
}

int Container::Tasks(std::vector<pid_t>& pids) {
    return -1;
}

int Container::Pids(std::vector<pid_t>& pids) {
    return -1;
}

boost::shared_ptr<google::protobuf::Message> Container::Status() {
    boost::shared_ptr<google::protobuf::Message> ret;
    return ret;
}

int Container::Attach(pid_t pid) {
    return -1;
}

int Container::Detach(pid_t pid) {
    return -1;
}

int Container::Detachall() {
    return -1;
}

} //namespace container
} //namespace galaxy
} //namespace baidu

