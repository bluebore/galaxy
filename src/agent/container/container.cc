// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "container.h"
#include "process.h"

#include "cgroup/subsystem_factory.h"
#include "cgroup/cgroup.h"
#include "protocol/galaxy.pb.h"
#include "volum/volum_group.h"
#include "util/user.h"
#include "util/path_tree.h"
#include "util/error_code.h"

#include "boost/filesystem/operations.hpp"
#include "boost/algorithm/string/case_conv.hpp"
#include "boost/algorithm/string/split.hpp"
#include "boost/algorithm/string/classification.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/algorithm/string/replace.hpp"
#include "agent/volum/volum.h"

#include <glog/logging.h>
#include <boost/lexical_cast/lexical_cast_old.hpp>
#include <boost/bind.hpp>

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <iosfwd>
#include <fstream>
#include <iostream>
#include <sstream>


DECLARE_string(agent_ip);
DECLARE_string(agent_port);
DECLARE_string(agent_hostname);
DECLARE_string(cmd_line);

namespace baidu {
namespace galaxy {
namespace container {

Container::Container(const ContainerId& id, const baidu::galaxy::proto::ContainerDescription& desc) :
    IContainer(id, desc),
    volum_group_(new baidu::galaxy::volum::VolumGroup()),
    process_(new Process()),
    status_(id.SubId()),
    created_time_(0L),
    destroy_time_(0L),
    force_kill_time_(-1L) {
}

Container::~Container() {
}

const ContainerId& Container::Id() const {
    return id_;
}

baidu::galaxy::util::ErrorCode Container::Construct() {
    baidu::galaxy::util::ErrorCode ec = status_.EnterAllocating();

    if (ec.Code() == baidu::galaxy::util::kErrorRepeated) {
        LOG(WARNING) << ec.Message();
        return ERRORCODE_OK;
    }

    if (ec.Code() != baidu::galaxy::util::kErrorOk) {
        LOG(WARNING) << "construct failed " << id_.CompactId() << ": " << ec.Message();
        return ERRORCODE(-1, "state machine error");
    }

    created_time_ = baidu::common::timer::get_micros();
    // err && ec, return err not ec, ec is just a temporary var
    baidu::galaxy::util::ErrorCode err = Construct_();

    if (0 != err.Code()) {
        ec = status_.EnterError();
        LOG(WARNING) << "construct container " << id_.CompactId() << " failed";

        if (ec.Code() != baidu::galaxy::util::kErrorOk) {
            LOG(FATAL) << "container " << id_.CompactId() << ": " << ec.Message();
        }
    } else {
        ec = status_.EnterReady();

        if (ec.Code() != baidu::galaxy::util::kErrorOk) {
            LOG(FATAL) << "container " << id_.CompactId() << ": " << ec.Message();
        }
    }

    if (0 == err.Code()) {
        LOG(INFO) << "sucess in constructing container " << id_.CompactId();
    } else {
        LOG(INFO) << "failed to construct container " << id_.CompactId();
    }

    return err;
}

baidu::galaxy::util::ErrorCode Container::Destroy() {
    baidu::galaxy::util::ErrorCode ec = status_.EnterDestroying();

    if (ec.Code() == baidu::galaxy::util::kErrorRepeated) {
        LOG(WARNING) << "container  " << id_.CompactId() << " is in kContainerDestroying status: " << ec.Message();
        ERRORCODE(-1, "repeated destroy");
    }

    if (ec.Code() != baidu::galaxy::util::kErrorOk) {
        LOG(WARNING) << "destroy container " << id_.CompactId() << " failed: " << ec.Message();
        return ERRORCODE(-1, "status machine");
    }

    SetExpiredTimeIfAbsent(30);
    LOG(INFO) << Id().CompactId() << " try kill appworker";

    if (!Expired() && Alive() && TryKill()) {
        if (Alive()) {
            ec = status_.EnterReady();

            if (ec.Code() != 0) {
                LOG(WARNING) << Id().CompactId() << "enter ready failed: " << ec.Message();
            }

            return ERRORCODE(-1, "try kill appwork failed");
        }
    }

    baidu::galaxy::util::ErrorCode ret = Destroy_();

    if (0 != ret.Code()) {
        ec = status_.EnterError();

        if (ec.Code() != baidu::galaxy::util::kErrorOk) {
            LOG(FATAL) << "destroy container " << id_.CompactId() << " failed: " << ec.Message();
        }
    } else {
        ec = status_.EnterTerminated();

        if (ec.Code() != baidu::galaxy::util::kErrorOk) {
            LOG(FATAL) << "destroy container " << id_.CompactId() << " failed: " << ec.Message();
        }

        destroy_time_ = baidu::common::timer::get_micros();
        LOG(INFO) << "destroy container " << id_.CompactId() << " sucessful";
    }

    return ret;
}

baidu::galaxy::util::ErrorCode Container::Construct_() {
    assert(!id_.Empty());
    // cgroup
    LOG(INFO) << "to create cgroup for container " << id_.CompactId()
              << ", expect cgroup size is " << desc_.cgroups_size();
    int ret = ConstructCgroup();

    if (0 != ret) {
        LOG(WARNING) << "failed in constructing cgroup for contanier " << id_.CompactId();
        return ERRORCODE(-1, "cgroup failed");
    }

    LOG(INFO) << "succeed in constructing cgroup for contanier " << id_.CompactId();
    ret = ConstructVolumGroup();

    if (0 != ret) {
        LOG(WARNING) << "failed in constructing volum group for container " << id_.CompactId();
        return ERRORCODE(-1, "volum failed");
    }

    LOG(INFO) << "succeed in constructing volum group for container " << id_.CompactId();
    // clone
    LOG(INFO) << "to clone appwork process for container " << id_.CompactId();
    ret = ConstructProcess();

    if (0 != ret) {
        LOG(WARNING) << "failed in constructing process for container " << id_.CompactId();
        return ERRORCODE(-1, "clone failed");
    }

    LOG(INFO) << "succeed in construct process (whose pid is " << process_->Pid()
              << ") for container " << id_.CompactId();
    return ERRORCODE_OK;
}


baidu::galaxy::util::ErrorCode Container::Reload(boost::shared_ptr<baidu::galaxy::proto::ContainerMeta> meta) {
    assert(!id_.Empty());
    created_time_ = meta->created_time();
    status_.EnterAllocating();
    int ret = ConstructCgroup();

    if (0 != ret) {
        status_.EnterError();
        return ERRORCODE(-1, "failed in constructing cgroup");
    }

    LOG(INFO) << "succeed in constructing cgroup for contanier " << id_.CompactId();
    ret = ConstructVolumGroup();

    if (0 != ret) {
        status_.EnterError();
        return ERRORCODE(-1, "failed in constructing volum group");
    }

    LOG(INFO) << "succeed in constructing volum group for container " << id_.CompactId();
    process_->Reload(meta->pid());
    status_.EnterReady();
    return ERRORCODE_OK;
}

int Container::ConstructCgroup() {
    for (int i = 0; i < desc_.cgroups_size(); i++) {
        boost::shared_ptr<baidu::galaxy::cgroup::Cgroup> cg(new baidu::galaxy::cgroup::Cgroup(
                baidu::galaxy::cgroup::SubsystemFactory::GetInstance()));
        boost::shared_ptr<baidu::galaxy::proto::Cgroup> desc(new baidu::galaxy::proto::Cgroup());
        desc->CopyFrom(desc_.cgroups(i));
        cg->SetContainerId(id_.SubId());
        cg->SetDescrition(desc);
        baidu::galaxy::util::ErrorCode err = cg->Construct();

        if (0 != err.Code()) {
            LOG(WARNING) << "fail in constructing cgroup, cgroup id is " << cg->Id()
                         << ", container id is " << id_.CompactId();
            break;
        }

        cgroup_.push_back(cg);
        LOG(INFO) << "succeed in constructing cgroup(" << desc_.cgroups(i).id()
                  << ") for cotnainer " << id_.CompactId();
    }

    if (cgroup_.size() != (unsigned int)desc_.cgroups_size()) {
        LOG(WARNING) << "fail in constructing cgroup for container " << id_.CompactId()
                     << ", expect cgroup size is " << desc_.cgroups_size()
                     << " real size is " << cgroup_.size();

        for (size_t i = 0; i < cgroup_.size(); i++) {
            baidu::galaxy::util::ErrorCode err = cgroup_[i]->Destroy();

            if (err.Code() != 0) {
                LOG(WARNING) << id_.CompactId()
                             << " construc failed and destroy failed: "
                             << err.Message();
            }
        }

        return -1;
    }

    return 0;
}

int Container::ConstructVolumGroup() {
    assert(created_time_ > 0);
    volum_group_->SetContainerId(id_.SubId());
    volum_group_->SetWorkspaceVolum(desc_.workspace_volum());
    volum_group_->SetGcIndex(created_time_ / 1000000);
    volum_group_->SetOwner(desc_.run_user());

    for (int i = 0; i < desc_.data_volums_size(); i++) {
        volum_group_->AddDataVolum(desc_.data_volums(i));
    }

    baidu::galaxy::util::ErrorCode ec = volum_group_->Construct();

    if (0 != ec.Code()) {
        LOG(WARNING) << "failed in constructing volum group for container " << id_.CompactId()
                     << ", reason is: " << ec.Message();
        return -1;
    }

    return 0;
}

int Container::ConstructProcess() {
    std::string container_root_path = baidu::galaxy::path::ContainerRootPath(id_.SubId());
    int now = (int)time(NULL);
    std::stringstream ss;
    ss << container_root_path << "/stderr." << now;
    process_->RedirectStderr(ss.str());
    LOG(INFO) << "redirect stderr to " << ss.str() << " for container " << id_.CompactId();
    ss.str("");
    ss << container_root_path << "/stdout." << now;
    process_->RedirectStdout(ss.str());
    LOG(INFO) << "redirect stdout to " << ss.str() << " for container " << id_.CompactId();
    pid_t pid = process_->Clone(boost::bind(&Container::RunRoutine, this, _1), NULL, 0);

    if (pid <= 0) {
        LOG(INFO) << "fail in clonning appwork process for container " << id_.CompactId();
        return -1;
    }

    return 0;
}


baidu::galaxy::util::ErrorCode Container::Destroy_() {
    // kill appwork
    pid_t pid = process_->Pid();

    if (pid > 0) {
        baidu::galaxy::util::ErrorCode ec = Process::Kill(pid);

        if (ec.Code() != 0) {
            LOG(WARNING) << "failed in killing appwork for container "
                         << id_.CompactId() << ": " << ec.Message();
            return ERRORCODE(-1, "kill appworker");
        }
    }

    LOG(INFO) << "container " << id_.CompactId() << " suceed in killing appwork whose pid is " << pid;

    // destroy cgroup
    for (size_t i = 0; i < cgroup_.size(); i++) {
        baidu::galaxy::util::ErrorCode ec = cgroup_[i]->Destroy();

        if (0 != ec.Code()) {
            LOG(WARNING) << "container " << id_.CompactId()
                         << " failed in destroying cgroup: " << ec.Message();
            return ERRORCODE(-1, "cgroup");
        }

        LOG(INFO) << "container " << id_.CompactId() << " suceed in destroy cgroup";
    }

    // destroy volum
    baidu::galaxy::util::ErrorCode ec = volum_group_->Destroy();

    if (0 != ec.Code()) {
        LOG(WARNING) << "failed in destroying volum group in container " << id_.CompactId()
                     << " " << ec.Message();
        return ERRORCODE(-1, "volum");
    }

    LOG(INFO) << "container " << id_.CompactId() << " suceed in destroy volum";
    return ERRORCODE_OK;
}

int Container::RunRoutine(void*) {
    // mount root fs
    bool v2_support = false;
    const baidu::galaxy::proto::ContainerDescription& desc = Description();
    if (desc.has_v2_support() && desc.v2_support()) {
        v2_support = true;
    }
    
    if (0 != volum_group_->MountRootfs(v2_support)) {
        std::cerr << "mount root fs failed" << std::endl;
        return -1;
    }

    baidu::galaxy::util::ErrorCode ec = volum_group_->MountSharedVolum(dependent_volums_);

    if (ec.Code() != 0) {
        std::cerr << "mount depent volum failed: " << ec.Message() << std::endl;
        return -1;
    }

    ::chdir(baidu::galaxy::path::ContainerRootPath(Id().SubId()).c_str());
    std::cout << "succed in mounting root fs\n";

    // change root
    if (0 != ::chroot(baidu::galaxy::path::ContainerRootPath(Id().SubId()).c_str())) {
        std::cerr << "chroot failed: " << strerror(errno) << std::endl;
        return -1;
    }

    std::cout << "chroot successfully:" << baidu::galaxy::path::ContainerRootPath(Id().SubId()) << std::endl;
    // change user or sh -l
    //baidu::galaxy::util::ErrorCode ec = baidu::galaxy::user::Su(desc_.run_user());
    //if (ec.Code() != 0) {
    //    std::cout << "su user " << desc_.run_user() << " failed: " << ec.Message() << std::endl;
    //    return -1;
    //}
    std::cout << "su user " << desc_.run_user() << " sucessfully" << std::endl;
    // start appworker
    // std::cout << "start cmd: /bin/sh -c " << desc_.cmd_line() << std::endl;
    // std::string cmd_line = FLAGS_cmd_line;
    std::string cmd_line = desc_.cmd_line();
    cmd_line += " --tag=";
    cmd_line += Id().SubId();
    char* argv[] = {
        const_cast<char*>("sh"),
        const_cast<char*>("-c"),
        //const_cast<char*>(desc_.cmd_line().c_str()),
        const_cast<char*>(cmd_line.c_str()),
        NULL
    };


    // export env
    ExportEnv();

    std::cout << "start cmd: /bin/sh -c " << cmd_line.c_str() << std::endl;
    ::execv("/bin/sh", argv);
    std::cerr << "exec cmd " << cmd_line << " failed: " << strerror(errno) << std::endl;
    return -1;
}

void Container::ExportEnv() {
    std::map<std::string, std::string> env;

    for (size_t i = 0; i < cgroup_.size(); i++) {
        cgroup_[i]->ExportEnv(env);
    }

    volum_group_->ExportEnv(env);
    this->ExportEnv(env);
    std::map<std::string, std::string>::const_iterator iter = env.begin();

    while (iter != env.end()) {
        int ret = ::setenv(boost::to_upper_copy(iter->first).c_str(), iter->second.c_str(), 1);

        if (0 != ret) {
            LOG(FATAL) << "set env failed for container " << id_.CompactId();
        }

        std::cout << "set env: " << boost::to_upper_copy(iter->first).c_str() << "=" << iter->second.c_str() << std::endl;
        iter++;
    }
}

void Container::ExportEnv(std::map<std::string, std::string>& env) {
    env["baidu_galaxy_containergroup_id"] = id_.GroupId();
    env["baidu_galaxy_container_id"] = id_.SubId();
    std::string ids;

    for (size_t i = 0; i < cgroup_.size(); i++) {
        if (!ids.empty()) {
            ids += ",";
        }

        ids += cgroup_[i]->Id();
    }

    env["baidu_galaxy_container_cgroup_ids"] = ids;
    env["baidu_galaxy_agent_hostname"] = FLAGS_agent_hostname;
    env["baidu_galaxy_agent_ip"] = FLAGS_agent_ip;
    env["baidu_galaxy_agent_port"] = FLAGS_agent_port;
    env["baidu_galaxy_container_user"] = desc_.run_user();
}

/*baidu::galaxy::proto::ContainerStatus Container::Status()
{
    return status_.Status();
}*/

void Container::KeepAlive() {
    int64_t now = baidu::common::timer::get_micros();

    if (now - created_time_ < 10000000L) {
        return;
    }

    if (status_.Status() != baidu::galaxy::proto::kContainerReady) {
        return;
    }

    if (!Alive()) {
        boost::filesystem::path exit_file(baidu::galaxy::path::ContainerRootPath(id_.SubId()));
        exit_file.append(".exit");
        boost::system::error_code ec;

        if (boost::filesystem::exists(exit_file, ec)) {
            baidu::galaxy::util::ErrorCode ec = status_.EnterFinished();

            if (ec.Code() != 0) {
                LOG(WARNING) << "container " << id_.CompactId()
                             << " failed in entering finished status" << ec.Message();
            } else {
                LOG(INFO) << "container " << id_.CompactId() << " enter finished status";
            }
        } else {
            baidu::galaxy::util::ErrorCode ec = status_.EnterErrorFrom(baidu::galaxy::proto::kContainerReady);

            if (ec.Code() != 0) {
                LOG(WARNING) << "container " << id_.CompactId()
                             << " failed in entering error status from kContainerReady:" << ec.Message();
            } else {
                LOG(INFO) << "container " << id_.CompactId() << " enter error status from kContainerReady";
            }
        }
    }
}

bool Container::Alive() {
    int pid = (int)process_->Pid();

    if (pid <= 0) {
        LOG(WARNING) << "process id is le 0 " << id_.CompactId();
        return false;
    }

    std::stringstream path;
    path << "/proc/" << (int)pid << "/environ";
    FILE* file = fopen(path.str().c_str(), "rb");

    if (NULL == file) {
        LOG(WARNING) << id_.CompactId() << " failed in openning file "
                     << path.str() << ": " << strerror(errno);
        return false;
    }

    char buf[1024] = {0};
    std::string env_str;

    while (!feof(file)) {
        int size = fread(buf, 1, sizeof buf, file);

        if (0 == size || ferror(file)) {
            break;
        }

        env_str.append(buf, size);
    }

    fclose(file);

    for (size_t i = 0; i < env_str.size(); i++) {
        if (env_str[i] == '\0') {
            env_str[i] = '\n';
        }
    }

    std::vector<std::string> envs;
    boost::split(envs, env_str, boost::is_any_of("\n"));

    for (size_t i = 0; i < envs.size(); i++) {
        if (boost::starts_with(envs[i], "BAIDU_GALAXY_CONTAINER_ID=")) {
            std::vector<std::string> vid;
            boost::split(vid, envs[i], boost::is_any_of("="));

            if (vid.size() == 2) {
                if (id_.SubId() == vid[1]) {
                    return true;
                }
            }
        }
    }

    LOG(WARNING) << "donot find env " << id_.CompactId();
    return false;
}

void Container::SetExpiredTimeIfAbsent(int32_t rel_sec) {
    assert(rel_sec >= 0);

    if (-1 == force_kill_time_) {
        force_kill_time_ = rel_sec * 1000000L + baidu::common::timer::get_micros();
    }
}

bool Container::Expired() {
    assert(force_kill_time_ >= 0);
    int64_t now = baidu::common::timer::get_micros();
    return now >= force_kill_time_;
}


bool Container::TryKill() {
    if (process_->Pid() > 0 && 0 == ::kill(process_->Pid(), SIGTERM)) {
        return true;
    }

    return false;
}

boost::shared_ptr<baidu::galaxy::proto::ContainerInfo> Container::ContainerInfo(bool full_info) {
    boost::shared_ptr<baidu::galaxy::proto::ContainerInfo> ret(new baidu::galaxy::proto::ContainerInfo());
    ret->set_id(id_.SubId());
    ret->set_group_id(id_.GroupId());
    ret->set_created_time(0);
    ret->set_status(status_.Status());
    ret->set_cpu_used(0);
    boost::shared_ptr<baidu::galaxy::proto::ContainerMetrix> metrix = ContainerMetrix();

    if (metrix->has_memory_used_in_byte()) {
        ret->set_memory_used(metrix->memory_used_in_byte());
    }

    if (metrix->has_cpu_used_in_millicore()) {
        ret->set_cpu_used(metrix->cpu_used_in_millicore());
    }

    baidu::galaxy::proto::ContainerDescription* cd = ret->mutable_container_desc();

    if (full_info) {
        cd->CopyFrom(desc_);
    } else {
        cd->set_version(desc_.version());
    }

    boost::shared_ptr<baidu::galaxy::volum::Volum> wv = volum_group_->WorkspaceVolum();

    if (NULL != wv) {
        baidu::galaxy::proto::Volum* vr = ret->add_volum_used();
        vr->set_used_size(wv->Used());
        vr->set_path(wv->Description()->dest_path());
        vr->set_device_path(wv->Description()->source_path());
    }

    for (int i = 0; i < volum_group_->DataVolumsSize(); i++) {
        baidu::galaxy::proto::Volum* vr = ret->add_volum_used();
        boost::shared_ptr<baidu::galaxy::volum::Volum> dv = volum_group_->DataVolum(i);
        vr->set_used_size(dv->Used());
        vr->set_path(dv->Description()->dest_path());
        vr->set_device_path(dv->Description()->source_path());
    }

    return ret;
}


boost::shared_ptr<baidu::galaxy::proto::ContainerMeta> Container::ContainerMeta() {
    boost::shared_ptr<baidu::galaxy::proto::ContainerMeta> ret(new baidu::galaxy::proto::ContainerMeta());
    ret->set_container_id(id_.SubId());
    ret->set_group_id(id_.GroupId());
    ret->set_created_time(created_time_);
    ret->set_pid(process_->Pid());
    ret->mutable_container()->CopyFrom(desc_);
    ret->set_destroy_time(destroy_time_);
    return ret;
}

boost::shared_ptr<ContainerProperty> Container::Property() {
    boost::shared_ptr<ContainerProperty> property(new ContainerProperty);
    property->container_id_ = id_.SubId();
    property->group_id_ = id_.GroupId();
    property->created_time_ = created_time_;
    property->pid_ = process_->Pid();
    const boost::shared_ptr<baidu::galaxy::volum::Volum> wv = volum_group_->WorkspaceVolum();
    property->workspace_volum_.container_abs_path = wv->TargetPath();
    property->workspace_volum_.phy_source_path = wv->SourcePath();
    property->workspace_volum_.container_rel_path = wv->Description()->dest_path();
    property->workspace_volum_.phy_gc_path = wv->SourceGcPath();
    property->workspace_volum_.medium = baidu::galaxy::proto::VolumMedium_Name(wv->Description()->medium());
    property->workspace_volum_.quota = wv->Description()->size();
    property->workspace_volum_.phy_gc_root_path = wv->SourceGcRootPath();

    //
    for (int i = 0; i < volum_group_->DataVolumsSize(); i++) {
        ContainerProperty::Volum cv;
        const boost::shared_ptr<baidu::galaxy::volum::Volum> v = volum_group_->DataVolum(i);
        cv.container_abs_path = v->TargetPath();
        cv.phy_source_path = v->SourcePath();
        cv.container_rel_path = v->Description()->dest_path();
        cv.phy_gc_path = v->SourceGcPath();
        cv.phy_gc_root_path = v->SourceGcRootPath();
        cv.medium = baidu::galaxy::proto::VolumMedium_Name(v->Description()->medium());
        cv.quota = v->Description()->size();
        property->data_volums_.push_back(cv);
    }

    return property;
}

const baidu::galaxy::proto::ContainerDescription& Container::Description() {
    return desc_;
}

boost::shared_ptr<baidu::galaxy::proto::ContainerMetrix> Container::ContainerMetrix() {
    boost::shared_ptr<baidu::galaxy::proto::ContainerMetrix> cm(new baidu::galaxy::proto::ContainerMetrix);
    int64_t memory_used_in_byte = 0L;
    int64_t cpu_used_in_millicore = 0L;

    for (size_t i = 0; i < cgroup_.size(); i++) {
        boost::shared_ptr<baidu::galaxy::proto::CgroupMetrix> m = cgroup_[i]->Statistics();

        if (NULL != cm.get()) {
            memory_used_in_byte += m->memory_used_in_byte();
            cpu_used_in_millicore += m->cpu_used_in_millicore();
        }
    }

    cm->set_memory_used_in_byte(memory_used_in_byte);
    cm->set_cpu_used_in_millicore(cpu_used_in_millicore);
    cm->set_time(baidu::common::timer::get_micros());
    return cm;
}

std::string Container::ContainerGcPath() {
    return volum_group_->ContainerGcPath();
}

} //namespace container
} //namespace galaxy
} //namespace baidu

