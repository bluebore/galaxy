// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cgroup.h"
#include "subsystem_factory.h"
#include "subsystem.h"
#include "freezer_subsystem.h"

#include <iostream>

namespace baidu {
namespace galaxy {
namespace cgroup {

Cgroup::Cgroup(const boost::shared_ptr<SubsystemFactory> factory) :
    factory_(factory) {
}

Cgroup::~Cgroup() {
}

void Cgroup::SetContainerId(const std::string& container_id) {
    container_id_ = container_id;
}

void Cgroup::SetDescrition(boost::shared_ptr<baidu::galaxy::proto::Cgroup> cgroup) {
    cgroup_ = cgroup;
}

int Cgroup::Construct() {
    assert(subsystem_.empty());
    std::vector<std::string> subsystems;
    factory_->GetSubsystems(subsystems);

    bool ok = true;
    for (size_t i = 0; i < subsystems.size(); i++) {
        boost::shared_ptr<Subsystem> ss = factory_->CreateSubsystem(subsystems[i]);
        assert(NULL != ss.get());

        ss->SetContainerId(container_id_);
        ss->SetCgroup(cgroup_);

        if (subsystems[i] == "freezer") {
            freezer_ = boost::dynamic_pointer_cast<FreezerSubsystem>(ss);
            assert(NULL != freezer_.get());
        } else {
            subsystem_.push_back(ss);
        }

        if (0 != ss->Construct()) {
            ok = false;
            break;
        }
    }

    if (!ok) {
        // destroy
        for (size_t i = 0; i < subsystem_.size(); i++) {
            subsystem_[i]->Destroy();
        }
        subsystem_.clear();

        if (NULL != freezer_.get()) {
            freezer_->Destroy();
            freezer_.reset();
        }
        return -1;
    }

    return 0;
}

// Fixme: freeze first, and than kill
int Cgroup::Destroy() {
    int ret = 0;

    for (size_t i = 0; i < subsystem_.size(); i++) {
        if (0 != subsystem_[i]->Destroy()) {
            ret = -1;
        }
    }
    subsystem_.clear();

    if (NULL !=  freezer_.get()) {
         freezer_->Destroy();
         freezer_.reset();
    }

    return ret;
}

boost::shared_ptr<google::protobuf::Message> Report() {
    boost::shared_ptr<google::protobuf::Message> ret;
    return ret;
}

int ExportEnv(std::map<std::string, std::string>& evn) {
    return 0;
}

}
}
}


