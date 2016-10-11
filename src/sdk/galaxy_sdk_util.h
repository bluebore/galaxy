// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_SDK_UTIL_H
#define BAIDU_GALAXY_SDK_UTIL_H

#include "string_util.h"
#include "protocol/galaxy.pb.h"
#include "galaxy_sdk.h"

namespace baidu {
namespace galaxy {
namespace sdk {

bool FillUser(const User& sdk_user, ::baidu::galaxy::proto::User* user);
bool FillVolumRequired(const VolumRequired& sdk_volum, ::baidu::galaxy::proto::VolumRequired* volum);
bool FillCpuRequired(const CpuRequired& sdk_cpu, ::baidu::galaxy::proto::CpuRequired* cpu);
bool FillMemRequired(const MemoryRequired& sdk_mem, ::baidu::galaxy::proto::MemoryRequired* mem);
bool FillTcpthrotRequired(const TcpthrotRequired& sdk_tcp, ::baidu::galaxy::proto::TcpthrotRequired* tcp);
bool FillBlkioRequired(const BlkioRequired& sdk_blk, ::baidu::galaxy::proto::BlkioRequired* blk);
bool FillPortRequired(const PortRequired& sdk_port, ::baidu::galaxy::proto::PortRequired* port);
bool FillCgroup(const Cgroup& sdk_cgroup, 
                ::baidu::galaxy::proto::Cgroup* cgroup,
                std::vector<std::string>& vec_port_names,
                std::vector<std::string>& vec_ports);
bool FillContainerDescription(const ContainerDescription& sdk_container,
                              ::baidu::galaxy::proto::ContainerDescription* container);

bool FillGrant(const Grant& sdk_grant, ::baidu::galaxy::proto::Grant* grant);

bool FillService(const Service& sdk_service, ::baidu::galaxy::proto::Service* service);
bool FillPackage(const Package& sdk_package, ::baidu::galaxy::proto::Package package);
bool FilldataPackage(const DataPackage& sdk_data, ::baidu::galaxy::proto::DataPackage* data);
bool FillImagePackage(const ImagePackage& sdk_image, ::baidu::galaxy::proto::ImagePackage* image);
bool FillTaskDescription(const TaskDescription& sdk_task, 
                         ::baidu::galaxy::proto::TaskDescription* task, 
                         std::vector<std::string>& vec_port_names,
                         std::vector<std::string>& vec_ports,
                         std::vector<std::string>& vec_service_names);
bool FillPodDescription(const PodDescription& sdk_pod, ::baidu::galaxy::proto::PodDescription* pod);
bool FillDeploy(const Deploy& sdk_deploy, ::baidu::galaxy::proto::Deploy* deploy);
bool FillJobDescription(const JobDescription& sdk_job,
                        ::baidu::galaxy::proto::JobDescription* job);

void PbJobDescription2SdkJobDescription(const ::baidu::galaxy::proto::JobDescription& pb_job, JobDescription* job);

std::string Strim(const std::string& str);
bool CheckEndPoint(std::string& endpoint);

} // end namespace sdk
} // end namespace galaxy
} // end namespace baidu

#endif  // BAIDU_GALAXY_SDK_UTIL_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
