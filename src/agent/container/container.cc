// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "container.h"
#include "cgroup/cgroup.h"
#include "protocol/galaxy.pb.h"
#include "volum/volum_group.h"

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

