// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils/resource_utils.h"

#include <set>
#include "logging.h"

namespace baidu {
namespace galaxy {

static bool CompareVolumeDesc(const Volume& l, const Volume& r) {
    return l.quota() > r.quota();
}

static bool CompareVolumeAsc(const Volume& l, const Volume& r) {
    return l.quota() < r.quota();
}

bool ResourceUtils::GtOrEq(const Resource& left,
                           const Resource& right) {
    return Compare(left, right) == -1 ? false : true;
}

bool ResourceUtils::AllocResource(const Resource& require,
                                 const std::vector<Resource>& alloc,
                          AgentInfo* agent) {
    Resource unassigned;
    Resource used_ports;
    unassigned.CopyFrom(agent->total());
    // assign cpu memory
    bool ok = ResourceUtils::Alloc(agent->assigned(), unassigned);
    if (!ok) {
        return false;
    }
    ok = ResourceUtils::AllocPort(agent->assigned(), used_ports);
    if (!ok) {
        LOG(INFO, "port alloc fails on agent %s", agent->endpoint().c_str());
        return false;
    }
    for (size_t i = 0; i < alloc.size(); i++) {
        ok = ResourceUtils::Alloc(alloc[i], unassigned);
        if (!ok) {
            LOG(INFO, "cpu mem alloc fails on agent %s", agent->endpoint().c_str());
            return false;
        }
        ok = ResourceUtils::AllocPort(alloc[i], used_ports);
        if (!ok) {
            LOG(INFO, "port alloc fails on agent %s", agent->endpoint().c_str());
            return false;
        }
    }
    ok = ResourceUtils::Alloc(require, unassigned);
    if (!ok) {
        LOG(WARNING, "cpu mem alloc fails on agent %s :mem_unassigned %ld, cpu_unassigned %d ,mem_require %ld, cpu_require %d",
                  agent->endpoint().c_str(), unassigned.memory(), unassigned.millicores(),
                  require.memory(),
                  require.millicores());
        return false;
    }
    ok = ResourceUtils::AllocPort(require, used_ports);
    if (!ok) {
        std::stringstream ss;
        for (int i = 0; i < require.ports_size(); i++) {
            if(i == 0) {
                ss << "require ports:";
            }
            ss << require.ports(i) << " ";
        }
        for (int i = 0; i < used_ports.ports_size(); i++) {
            if (i == 0) {
                ss << " used ports:";
            }
            ss << used_ports.ports(i) << " "; 
        }
        LOG(WARNING, "port alloc fails on agent %s log %s", 
                    agent->endpoint().c_str(),
                    ss.str().c_str());
        return false;
    }
    Resource assigned;
    assigned.CopyFrom(agent->total());
    ok = ResourceUtils::Alloc(unassigned, assigned);
    if (!ok) {
        return false;
    }
    assigned.mutable_ports()->CopyFrom(used_ports.ports());
    agent->mutable_assigned()->CopyFrom(assigned);
    return true;
}

bool ResourceUtils::AllocPort(const Resource& require,
                              Resource& used) {
    std::set<int32_t> used_ports;
    for (int i = 0; i < used.ports_size(); i++) {
        used_ports.insert(used.ports(i));
    }

    for (int i = 0; i < require.ports_size(); i++) {
        if (used_ports.find(require.ports(i)) != used_ports.end()) {
            LOG(WARNING, "port %d is used", require.ports(i));
            return false;
        }
    }

    for (int i = 0; i < require.ports_size(); i++) {
        used.add_ports(require.ports(i));
    }
    return true;
}

void ResourceUtils::DeallocResource(const Resource& to_be_free,
                                    AgentInfo* agent) {
    if (agent == NULL) {
        return;
    }
    Resource* assigned = agent->mutable_assigned();
    assigned->set_memory(assigned->memory() - to_be_free.memory());
    assigned->set_millicores(assigned->millicores() - to_be_free.millicores());
    DeallocPort(to_be_free, assigned);
}

void ResourceUtils::DeallocPort(const Resource& to_be_free,
                                Resource* used) {
    if (used == NULL) {
        return ;
    }
    std::set<int32_t> used_ports;
    for (int i = 0; i < used->ports_size(); i++) {
        used_ports.insert(used->ports(i));
    }
    for (int i = 0; i < to_be_free.ports_size(); i++) {
        used_ports.erase(to_be_free.ports(i));
    }
    used->clear_ports();
    std::set<int32_t>::iterator port_it = used_ports.begin();
    for (; port_it != used_ports.end(); ++port_it) {
        used->add_ports(*port_it);
    }
}

bool ResourceUtils::Alloc(const Resource& require, 
                          Resource& target) {
    // target should be >= target
    if (!GtOrEq(target, require)) {
        return false;
    }

    target.set_millicores(target.millicores() - require.millicores());
    target.set_memory(target.memory() - require.memory());
    VolumeAlloc(target.disks(), require.disks(), 
                target.mutable_disks());
    VolumeAlloc(target.ssds(), require.ssds(),
                target.mutable_ssds());
    return true;
}

void ResourceUtils::VolumeAlloc(const VolumeList& total,
                                const VolumeList& require,
                                VolumeList* left) {

    std::vector<Volume> v_total;
    std::vector<Volume> v_require;
    for (int i = 0; i < total.size(); i++) {
        v_total.push_back(total.Get(i));
    }

    for (int i = 0; i < require.size(); i++) {
        v_require.push_back(require.Get(i));
    }
    std::sort(v_total.begin(), v_total.end(), CompareVolumeAsc);
    std::sort(v_require.begin(), v_require.end(), CompareVolumeAsc);
    left->Clear();
    size_t require_index = 0;
    for (size_t i = 0; i < v_total.size(); i++) {
        if (i <= require_index) {
            if (v_require[require_index].quota() > v_total[i].quota()) {
                left->Add()->CopyFrom(v_total[i]);
            }
        } else{
            left->Add()->CopyFrom(v_total[i]);
        } 
    }
}


bool ResourceUtils::HasDiff(const Resource& left, 
                            const Resource& right) {
    if (left.millicores() != right.millicores()) {
        return true;
    }

    if (left.memory() != left.memory()) {
        return true;
    }
    std::vector<Volume> left_disks;
    for (int i = 0; i < left.disks_size(); i++) {
        left_disks.push_back(left.disks(i));
    }
    std::vector<Volume> right_disks;
    for (int i = 0; i < right.disks_size(); i++) {
        right_disks.push_back(right.disks(i));
    }

    int32_t disk_check = CompareVector(left_disks, 
                                       right_disks,
                                       CompareVolumeDesc);
    if (disk_check != 0) {
        return true;
    }
    std::vector<Volume> left_ssds;
    for (int i = 0; i < left.ssds_size(); i++) {
        left_ssds.push_back(left.ssds(i));
    }
    std::vector<Volume> right_ssds;
    for (int i = 0; i < right.ssds_size(); i++) {
        right_ssds.push_back(right.ssds(i));
    }
    int32_t ssd_check = CompareVector(left_ssds, 
                                      right_ssds,
                                      CompareVolumeDesc);
    if (ssd_check != 0) {
        return true;
    }
    return false;
}

int32_t ResourceUtils::Compare(const Resource& left,
                               const Resource& right) {
    int32_t cpu_check = 0;
    if (left.millicores() > right.millicores()) {
        cpu_check = 1;
    }else if (left.millicores() < right.millicores()) {
        return -1;
    }

    int32_t mem_check = 0;
    if (left.memory() > right.memory()) {
        mem_check = 1;
    }else if (left.memory() < right.memory()) {
        return -1;
    }

    std::vector<Volume> left_disks;
    for (int i = 0; i < left.disks_size(); i++) {
        left_disks.push_back(left.disks(i));
    }
    std::vector<Volume> right_disks;
    for (int i = 0; i < right.disks_size(); i++) {
        right_disks.push_back(right.disks(i));
    }

    int32_t disk_check = CompareVector(left_disks, 
                                       right_disks,
                                       CompareVolumeDesc);
    if (disk_check == -1) {
        return -1;
    }

    std::vector<Volume> left_ssds;
    for (int i = 0; i < left.ssds_size(); i++) {
        left_ssds.push_back(left.ssds(i));
    }
    std::vector<Volume> right_ssds;
    for (int i = 0; i < right.ssds_size(); i++) {
        right_ssds.push_back(right.ssds(i));
    }

    int32_t ssd_check = CompareVector(left_ssds, 
                                      right_ssds,
                                      CompareVolumeDesc);
    if (ssd_check == -1) {
        return -1;
    }

    return cpu_check & mem_check & disk_check & ssd_check;
}


} // galaxy
} // baidu
