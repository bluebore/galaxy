// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include "galaxy_sdk_util.h"
#include "string_util.h"

#ifndef GALAXY_SDK_UTIL_H
#define GALAXY_SDK_UTIL_H

namespace baidu {
namespace galaxy {
namespace sdk {

std::string Strim(const std::string& str) {
    const std::string split_symbol = " \n\t";
    return ::baidu::common::TrimString(str, split_symbol);
}

bool CheckNum(const int value) {
    if (value < 0 || value > 255) {
        return false;
    }
    return true;
}

bool CheckEndPoint(std::string& endpoint) {

    size_t pos = endpoint.rfind(":");
    if (pos == std::string::npos) {
        fprintf(stderr, "endpoint format not correct, must be IP:Port\n");
        return false;
    }
    std::string ip = Strim(endpoint.substr(0, pos));
    int ip_1 = 0;
    int ip_2 = 0;
    int ip_3 = 0;
    int ip_4 = 0;
    char other = '\0';
    int num = sscanf(ip.c_str(), "%d.%d.%d.%d%s", &ip_1, &ip_2, &ip_3, &ip_4, &other);
    bool check_num = CheckNum(ip_1) && CheckNum(ip_2) && CheckNum(ip_3) && CheckNum(ip_4);
    if (num != 4 || !check_num || other != '\0') {
        fprintf(stderr, "Ip not correct\n");
        return false;
    }
    std::string port = Strim(endpoint.substr(pos + 1));
    if(port.empty() || std::string::npos != port.find_first_not_of("0123456789")) {
        fprintf(stderr, "Port not correct\n");
        return false;
    }
    endpoint = ::baidu::common::NumToString(ip_1) + "." 
               + ::baidu::common::NumToString(ip_2) + "." 
               + ::baidu::common::NumToString(ip_3) + "." 
               + ::baidu::common::NumToString(ip_4) + ':' + port;
    return true;
}

bool FillUser(const User& sdk_user, ::baidu::galaxy::proto::User* user) {
    std::string name = Strim(sdk_user.user);
    if (name.empty()) {
        fprintf(stderr, "user must not be empty\n");
        return false;
    }
    std::string token = Strim(sdk_user.token);
    if (token.empty()) {
        fprintf(stderr, "token must not be empty\n");
        return false;
    }
    user->set_user(name);
    user->set_token(token);
    return true;
}

bool FillVolumRequired(const VolumRequired& sdk_volum, ::baidu::galaxy::proto::VolumRequired* volum) {
    if (sdk_volum.size <= 0) {
        fprintf(stderr, "volum size must be greater than 0\n");
        return false;
    }
    volum->set_size(sdk_volum.size);

    if (sdk_volum.type == kEmptyDir) {
        volum->set_type(::baidu::galaxy::proto::kEmptyDir);
    } else if (sdk_volum.type == kHostDir) {
        volum->set_type(::baidu::galaxy::proto::kHostDir);
    } else {
        fprintf(stderr, "volum type must be kEmptyDir or kHostDir\n");
        return false;
    }

    if (sdk_volum.medium == kSsd) {
        volum->set_medium(baidu::galaxy::proto::kSsd);
    } else if (sdk_volum.medium == kDisk) {
        volum->set_medium(::baidu::galaxy::proto::kDisk);
    } else if (sdk_volum.medium == kBfs) {
        volum->set_medium(baidu::galaxy::proto::kBfs);
    } else if (sdk_volum.medium == kTmpfs) {
        volum->set_medium(::baidu::galaxy::proto::kTmpfs);
    } else {
        fprintf(stderr, "volum medium must be kDisk, kBfs, kTmpfs\n");
        return false;
    }

    volum->set_source_path(sdk_volum.source_path); //不需要赋值

    std::string dest_path = Strim(sdk_volum.dest_path);
    if (dest_path.empty()) {
        fprintf(stderr, "volum dest_path must not be empty\n");
        return false;
    }
    volum->set_dest_path(dest_path);
    volum->set_readonly(sdk_volum.readonly);
    volum->set_exclusive(sdk_volum.exclusive);
    //volum->set_use_symlink(sdk_volum.use_symlink);//暂时不支持，默认为false
    volum->set_use_symlink(false);
    return true;
}

bool FillCpuRequired(const CpuRequired& sdk_cpu, 
                        ::baidu::galaxy::proto::CpuRequired* cpu) {
    if (sdk_cpu.milli_core <= 0) {
        fprintf(stderr, "cpu millicores must be greater than 0\n");
        return false;
    }
    cpu->set_milli_core(sdk_cpu.milli_core);
    cpu->set_excess(sdk_cpu.excess);
    return true;
}

bool FillMemRequired(const MemoryRequired& sdk_mem, 
                        ::baidu::galaxy::proto::MemoryRequired* mem) {
    if (sdk_mem.size <= 0) {
        fprintf(stderr, "memory size must be greater than 0\n");
        return false;
    }
    mem->set_size(sdk_mem.size);
    mem->set_excess(sdk_mem.excess);
    return true;
}

bool FillTcpthrotRequired(const TcpthrotRequired& sdk_tcp,
                        ::baidu::galaxy::proto::TcpthrotRequired* tcp) {
    if (sdk_tcp.recv_bps_quota <= 0) {
        fprintf(stderr, "tcp recv_bps_quota must be greater than 0\n");
        return false;
    }
    tcp->set_recv_bps_quota(sdk_tcp.recv_bps_quota);
    tcp->set_recv_bps_excess(sdk_tcp.recv_bps_excess);

    if (sdk_tcp.send_bps_quota <= 0) {
        fprintf(stderr, "tcp send_bps_quota must be greater than 0\n");
        return false;
    }
    tcp->set_send_bps_quota(sdk_tcp.send_bps_quota);
    tcp->set_send_bps_excess(sdk_tcp.send_bps_excess);
    return true;
}

bool FillBlkioRequired(const BlkioRequired& sdk_blk,
                        ::baidu::galaxy::proto::BlkioRequired* blk) {
    if (sdk_blk.weight <= 0 || sdk_blk.weight >= 1000) {
        fprintf(stderr, "blkio weight must be in 0~1000\n");
        return false;
    }
    blk->set_weight(sdk_blk.weight);
    return true;
}

bool ValidatePort(const std::vector<std::string>& vec_ports) {

    bool ok = true;
    for (size_t i = 1; i < vec_ports.size(); ++i) {
        if ((vec_ports[i].compare("dynamic") != 0 && vec_ports[i].compare(vec_ports[i-1]) == 0)
                || (vec_ports[i-1].compare("dynamic") == 0
                     && vec_ports[i].compare("dynamic") != 0)) {
            fprintf(stderr, "ports are not correct in task, ports must be serial\n");
            ok = false;
            break;
        }

        if (vec_ports[i-1].compare("dynamic") != 0 && vec_ports[i].compare("dynamic") != 0) {
            int int_port = atoi(vec_ports[i-1].c_str());
            if (int_port < 1025 || int_port > 9999) {
                fprintf(stderr, "port %s is error, must be in 1025~9999\n", vec_ports[i-1].c_str());
                ok =  false;
                break;
            }
            ++int_port;
            if (vec_ports[i].compare(::baidu::common::NumToString(int_port)) != 0) {
                fprintf(stderr, "ports are not correct in task, ports must be serial:%s\n", vec_ports[i].c_str());
                ok =  false;
                break;
            }
        }
    }
    return ok;
}

bool FillPortRequired(const PortRequired& sdk_port, ::baidu::galaxy::proto::PortRequired* port) {
    std::string port_name = Strim(sdk_port.port_name);
    if (port_name.empty()) {
        fprintf(stderr, "port_name must not be empty in port\n");
        return false;
    }
    port->set_port_name(port_name);

    std::string str_port = Strim(sdk_port.port);
    if (str_port.empty()) {
        fprintf(stderr, "port must not be empty in port, it could be \"dynamic\"or specific port such as \"8080\" \n");
        return false;
    }
    port->set_port(str_port);
    port->set_real_port(sdk_port.real_port);//可以不用，有resman分配
    return true;
}

bool FillCgroup(const Cgroup& sdk_cgroup, 
                ::baidu::galaxy::proto::Cgroup* cgroup,  
                std::vector<std::string>& vec_cgroups_ports) {

    std::vector<std::string> vec_port_names;
    std::vector<std::string> vec_ports;
    
    if (!FillCpuRequired(sdk_cgroup.cpu, cgroup->mutable_cpu())) {
        return false;
    }

    if (!FillMemRequired(sdk_cgroup.memory, cgroup->mutable_memory())) {
        return false;
    }
    
    if (!FillTcpthrotRequired(sdk_cgroup.tcp_throt, cgroup->mutable_tcp_throt())) {
        return false;
    }
    
    if (!FillBlkioRequired(sdk_cgroup.blkio, cgroup->mutable_blkio())) {
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < sdk_cgroup.ports.size(); ++i) {
        ::baidu::galaxy::proto::PortRequired* port = cgroup->add_ports();
        if (!FillPortRequired(sdk_cgroup.ports[i], port)) {
            ok = false;
            break;
        }

        std::vector<std::string> ::iterator it = find(vec_port_names.begin(), 
                                                      vec_port_names.end(), 
                                                      Strim(sdk_cgroup.ports[i].port_name));
        if (it != vec_port_names.end()) {
            fprintf(stderr, "port_name in ports cannot be repeated\n");
            ok = false;
            break;
        }
        vec_port_names.push_back(Strim(sdk_cgroup.ports[i].port_name));

        //端口号不可重复
        if (sdk_cgroup.ports[i].port.compare("dynamic") != 0) {
            it = find(vec_cgroups_ports.begin(), vec_cgroups_ports.end(), Strim(sdk_cgroup.ports[i].port));
            if (it != vec_cgroups_ports.end()) {
                fprintf(stderr, "port in ports cannot be repeated\n");
                ok = false;
                break;
            }
        }
        vec_ports.push_back(Strim(sdk_cgroup.ports[i].port));
        vec_cgroups_ports.push_back(Strim(sdk_cgroup.ports[i].port));
    }

    if (!ok) {
        return false;
    }

    ok = ValidatePort(vec_ports);
    return ok;
}

bool FillContainerDescription(const ContainerDescription& sdk_container, 
                                ::baidu::galaxy::proto::ContainerDescription* container) {

    if (sdk_container.priority == kJobMonitor) {
        container->set_priority(::baidu::galaxy::proto::kJobMonitor);
    } else if (sdk_container.priority == kJobService) {
        container->set_priority(::baidu::galaxy::proto::kJobService);
    } else if (sdk_container.priority == kJobBatch) {
        container->set_priority(::baidu::galaxy::proto::kJobBatch);
    } else if (sdk_container.priority == kJobBestEffort) {
        container->set_priority(::baidu::galaxy::proto::kJobBestEffort);
    } else {
        fprintf(stderr, "job type must be kJobService, kJobBatch, kJobBestEffort\n");
        return false;
    }

    std::string run_user = Strim(sdk_container.run_user);
    if (run_user.empty()) {
        fprintf(stderr, "run_user must not be empty\n");
        return false;
    }
    
    //container->set_run_user(run_user);
    container->set_run_user("galaxy");
    /*if (sdk_container.version.empty()) {
        fprintf(stderr, "version must not be empty\n");
        return false;
    }*/
    container->set_version(Strim(sdk_container.version));

    /*if (sdk_container.tag.empty()) {
        fprintf(stderr, "tag must not be empty\n");
        return false;
    }*/
    container->set_tag(Strim(sdk_container.tag));

    container->set_cmd_line(sdk_container.cmd_line); //不用

    if (sdk_container.max_per_host <= 0) {
        fprintf(stderr, "max_per_host must be greater than 0\n");
        return false;
    }
    container->set_max_per_host(sdk_container.max_per_host);

    if (sdk_container.pool_names.size() == 0) {
        fprintf(stderr, "pools size is 0\n");
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < sdk_container.pool_names.size(); ++i) {
        if (sdk_container.pool_names[i].empty()) {
            fprintf(stderr, "pool[%d] must not be empty\n", (int)i);
            ok = false;
            break;
        }
        container->add_pool_names(sdk_container.pool_names[i]);
    }
    if (!ok) {
        return false;
    }

    if(sdk_container.container_type == kVolumContainer) {
        container->set_container_type(::baidu::galaxy::proto::kVolumContainer);
    } else if (sdk_container.container_type == kNormalContainer) {
        container->set_container_type(::baidu::galaxy::proto::kNormalContainer);
    } else {
        fprintf(stderr, "container type must be kVolumContainer or kNormalContainer\n");
        return false;
    }
    
    if (!FillVolumRequired(sdk_container.workspace_volum, container->mutable_workspace_volum())) {
        return false;
    }

    //重复值检测
    std::vector<std::string> vec_des_path;
    vec_des_path.push_back(sdk_container.workspace_volum.dest_path);

    for (size_t i = 0; i < sdk_container.data_volums.size(); ++i) {
        ::baidu::galaxy::proto::VolumRequired* volum = container->add_data_volums();
        if(!FillVolumRequired(sdk_container.data_volums[i], volum)) {
            ok = false;
            break;
        }
        //重复值检测
        std::vector<std::string> ::iterator it = find(vec_des_path.begin(), 
                                                      vec_des_path.end(), 
                                                      Strim(sdk_container.data_volums[i].dest_path));
        if (it != vec_des_path.end()) {
            fprintf(stderr, "dest_path in volums cannot be repeated\n");
            ok = false;
            break;
        }
        vec_des_path.push_back(Strim(sdk_container.data_volums[i].dest_path));
    }
    if (!ok) {
        return false;
    }

    if(sdk_container.container_type == kVolumContainer) {
        return true; //shared volum
    }

    if (sdk_container.cgroups.size() == 0) {
        fprintf(stderr, "task size is zero\n");
        return false;
    }

    //std::vector<std::string> cgroups_vec_port_names; //端口名重复检测
    std::vector<std::string> cgroups_vec_ports; //端口号重复检测
    for (uint32_t i = 0; i < sdk_container.cgroups.size(); ++i) {
        ::baidu::galaxy::proto::Cgroup* cgroup = container->add_cgroups();
        if(!FillCgroup(sdk_container.cgroups[i], cgroup, cgroups_vec_ports)) {
            ok = false;
            break;
        }
        cgroup->set_id(::baidu::common::NumToString(i));
    }
    
    return ok;
}

bool FillPackage(const Package& sdk_package, ::baidu::galaxy::proto::Package* package) {
    std::string source_path = Strim(sdk_package.source_path);
    if (source_path.empty()) {
        fprintf(stderr, "package source_path must not be empty\n");
        return false;
    }
    package->set_source_path(source_path);

    std::string dest_path = Strim(sdk_package.dest_path);
    if (dest_path.empty()) {
        fprintf(stderr, "package dest_path must not be empty\n");
        return false;
    }
    package->set_dest_path(dest_path);

    std::string version = Strim(sdk_package.version);
    if (version.empty()) {
        fprintf(stderr, "package version must not be empty\n");
        return false;
    }
    package->set_version(version);
    return true;
}

bool FillImagePackage(const ImagePackage& sdk_image, 
                        ::baidu::galaxy::proto::ImagePackage* image) {
    std::string start_cmd = Strim(sdk_image.start_cmd);
    if (start_cmd.empty()) {
        fprintf(stderr, "package start_cmd must not be empty\n");
        return false;
    }
    image->set_start_cmd(start_cmd);
    image->set_stop_cmd(Strim(sdk_image.stop_cmd));
    image->set_health_cmd(Strim(sdk_image.health_cmd));
    if (!FillPackage(sdk_image.package, image->mutable_package())) {
        return false;
    }
    return true;
}

bool FilldataPackage(const DataPackage& sdk_data, ::baidu::galaxy::proto::DataPackage* data) {
    bool ok = true;
    std::string reload_cmd = Strim(sdk_data.reload_cmd);
    if (sdk_data.packages.size() > 0 && reload_cmd.empty()) {
        fprintf(stderr, "package reload_cmd must not be empty if size of packages in data_package is greater than 0\n");
        return false;
    }
    data->set_reload_cmd(reload_cmd);
    for (size_t i = 0; i < sdk_data.packages.size(); ++i) {
        ::baidu::galaxy::proto::Package* package = data->add_packages();
        ok = FillPackage(sdk_data.packages[i], package);
        if (!ok) {
            break;
        }
    }
    return ok;
}

bool FillService(const Service& sdk_service, 
                 ::baidu::galaxy::proto::Service* service) {
    std::string service_name = Strim(sdk_service.service_name);
    if (service_name.empty()) {
        fprintf(stderr, "service service_name must not be empty\n");
        return false;
    }

    service->set_service_name(service_name);
    
    /*if (sdk_service.port_name.empty()) {
        fprintf(stderr, "service port_name must not be empty\n");
        return false;
    }*/

    std::string token = Strim(sdk_service.token);
    if (sdk_service.use_bns && token.empty()) {
        fprintf(stderr, "service token must not be empty\n");
        return false; 
    }

    service->set_port_name(Strim(sdk_service.port_name));
    service->set_use_bns(sdk_service.use_bns);
    service->set_tag(Strim(sdk_service.tag));
    service->set_health_check_type(Strim(sdk_service.health_check_type));
    service->set_health_check_script(Strim(sdk_service.health_check_script));
    service->set_token(token);

    return true;
}

bool FillTaskDescription(const TaskDescription& sdk_task,
        ::baidu::galaxy::proto::TaskDescription* task,
        std::vector<std::string>& vec_tasks_ports,
        std::vector<std::string>& vec_service_names) {
    
    std::vector<std::string> vec_port_names;
    std::vector<std::string> vec_ports;
    bool ok = true;
    if (!FillCpuRequired(sdk_task.cpu, task->mutable_cpu())) {
        return false;
    }

    if (!FillMemRequired(sdk_task.memory, task->mutable_memory())) {
        return false;
    }

    std::vector<std::string> vec_task_port_names; //用于判断service中的port_name是否在ports中
    for (size_t i = 0; i < sdk_task.ports.size(); ++i) {
        ::baidu::galaxy::proto::PortRequired* port = task->add_ports();
        ok = FillPortRequired(sdk_task.ports[i], port);
        if (!ok) {
            break;
        }

        std::vector<std::string> ::iterator it = find(vec_port_names.begin(), 
                                                      vec_port_names.end(), 
                                                      Strim(sdk_task.ports[i].port_name));
        if (it != vec_port_names.end()) {
            fprintf(stderr, "port_name in ports cannot be repeated\n");
            ok = false;
            break;
        }
        vec_port_names.push_back(Strim(sdk_task.ports[i].port_name));
        vec_task_port_names.push_back(Strim(sdk_task.ports[i].port_name));
        
        //端口号不可重复
        if (sdk_task.ports[i].port.compare("dynamic") != 0) {
            it = find(vec_tasks_ports.begin(), vec_tasks_ports.end(), sdk_task.ports[i].port);
            if (it != vec_tasks_ports.end()) {
                fprintf(stderr, "port in ports cannot be repeated\n");
                ok = false;
                break;
            }
        }
        vec_ports.push_back(Strim(sdk_task.ports[i].port));
        vec_tasks_ports.push_back(Strim(sdk_task.ports[i].port));
    }

    if (!ok) {
        return false;
    }

    ok = ValidatePort(vec_ports);
    if (!ok) {
        return false;
    }

    if (!FillTcpthrotRequired(sdk_task.tcp_throt, task->mutable_tcp_throt())) {
        return false;
    }

    if (!FillBlkioRequired(sdk_task.blkio, task->mutable_blkio())) {
        return false;
    }

    if (!FillImagePackage(sdk_task.exe_package, task->mutable_exe_package())) {
        return false;
    }
    
    if (!FilldataPackage(sdk_task.data_package, task->mutable_data_package())) {
        return false;
    }

    std::vector<std::string> vec_service_port_name; //检测service port_name是否有重复
    for (size_t i = 0; i < sdk_task.services.size(); ++i) {
        ::baidu::galaxy::proto::Service* service = task->add_services();
        ok = FillService(sdk_task.services[i], service);
        if (!ok) {
            break;
        }

        //检测service_name是否有重复
        std::vector<std::string> ::iterator it = find(vec_service_names.begin(),
                                                      vec_service_names.end(),
                                                      Strim(sdk_task.services[i].service_name));
        if (it != vec_service_names.end()) {
            fprintf(stderr, "service_name in service[%d] must not be repeated\n", (int)i);
            ok = false;
            break;
        }
        vec_service_names.push_back(Strim(sdk_task.services[i].service_name));

        if (sdk_task.services[i].port_name.empty()) {
            continue;
        }
    
        //检测service中port_name是否有重复
        it = find(vec_service_port_name.begin(), vec_service_port_name.end(), Strim(sdk_task.services[i].port_name));
        if (it != vec_service_port_name.end()) {
            fprintf(stderr, "port_name in service[%d] must not be repeated\n", (int)i);
            ok = false;
            break;
        }
        
        //检测使用的端口名称是否在本task的port中
        it = find(vec_task_port_names.begin(), vec_task_port_names.end(), Strim(sdk_task.services[i].port_name));
        if (it == vec_task_port_names.end()) {
            fprintf(stderr, "port_name in service[%d] is not existed in task ports\n", (int)i);
            ok = false;
            break;
        }
        vec_service_port_name.push_back(Strim(sdk_task.services[i].port_name));
    }
    return ok;
}

bool FillPodDescription(const PodDescription& sdk_pod,
                            ::baidu::galaxy::proto::PodDescription* pod) {
    if (!FillVolumRequired(sdk_pod.workspace_volum, pod->mutable_workspace_volum())) {
        return false;
    }

    //重复值检测
    std::vector<std::string> vec_des_path;
    vec_des_path.push_back(sdk_pod.workspace_volum.dest_path);
    
    bool ok = true;
    for (size_t i = 0; i < sdk_pod.data_volums.size(); ++i) {
        ::baidu::galaxy::proto::VolumRequired* volum = pod->add_data_volums();
        ok = FillVolumRequired(sdk_pod.data_volums[i], volum);
        if (!ok) {
            break;
        }
        
        //重复值检测
        std::vector<std::string> ::iterator it = find(vec_des_path.begin(), 
                                                      vec_des_path.end(), 
                                                      Strim(sdk_pod.data_volums[i].dest_path));
        if (it != vec_des_path.end()) {
            fprintf(stderr, "dest_path in volums cannot be repeated\n");
            ok = false;
            break;
        }
        vec_des_path.push_back(Strim(sdk_pod.data_volums[i].dest_path));
    }
    if (!ok) {
        return false;
    }

    if (sdk_pod.tasks.size() == 0) {
        fprintf(stderr, "task size is zero\n");
        return false;
    }

    //std::vector<std::string> tasks_vec_port_names; //端口名重复检测
    std::vector<std::string> tasks_vec_ports; //端口号重复检测
    std::vector<std::string> vec_service_names; //service name重复值检测
    for (uint32_t i = 0; i < sdk_pod.tasks.size(); ++i) {
        ::baidu::galaxy::proto::TaskDescription* task = pod->add_tasks(); 
        ok = FillTaskDescription(sdk_pod.tasks[i], task, tasks_vec_ports, 
                                 vec_service_names);

        if (!ok) {
            break;
        }
        task->set_id(::baidu::common::NumToString(i));
    }
    return ok;
}

bool FillDeploy(const Deploy& sdk_deploy, ::baidu::galaxy::proto::Deploy* deploy) {
    if (sdk_deploy.replica < 0 || sdk_deploy.replica >= 10000) {
        fprintf(stderr, "deploy replica must be greater than or equal to 0 and less than 10000\n");
        return false;
    }
    deploy->set_replica(sdk_deploy.replica);

    if (sdk_deploy.step < 0U) {
        fprintf(stderr, "deploy step must be greater or and equal to 0 \n");
        return false;
    }
    deploy->set_step(sdk_deploy.step);

    if (sdk_deploy.interval < 0U || sdk_deploy.interval > 3600) {
        fprintf(stderr, "deploy interval must be greater than or equal to 0 and less than 3600\n");
        return false;
    }
    deploy->set_interval(sdk_deploy.interval);

    if (sdk_deploy.max_per_host <= 0U || sdk_deploy.max_per_host >= 30) {
        fprintf(stderr, "deploy max_per_host must be greater than 0 and less than 30\n");
        return false;
    }
    deploy->set_max_per_host(sdk_deploy.max_per_host);
    
    deploy->set_tag(Strim(sdk_deploy.tag));

    if (sdk_deploy.pools.size() == 0) {
        fprintf(stderr, "deploy pools size is 0\n");
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < sdk_deploy.pools.size(); ++i) {
        std::string pool = Strim(sdk_deploy.pools[i]);
        if (pool.empty()) {
            fprintf(stderr, "deploy pools[%d] must not be empty\n", (int)i);
            ok = false;
            break;
        }
        deploy->add_pools(pool);
    }
    return ok;
}

bool FillJobDescription(const JobDescription& sdk_job,
                        ::baidu::galaxy::proto::JobDescription* job) {
    std::string name = Strim(sdk_job.name);
    if (name.empty()) {
        fprintf(stderr, "job name must not be empty\n");
        return false;
    }
    job->set_name(name);

    //job->set_version(sdk_job.version); //不再需要

    if (sdk_job.type == kJobMonitor) {
        job->set_priority(::baidu::galaxy::proto::kJobMonitor);
    } else if (sdk_job.type == kJobService) {
        job->set_priority(::baidu::galaxy::proto::kJobService);
    } else if (sdk_job.type == kJobBatch) {
        job->set_priority(::baidu::galaxy::proto::kJobBatch);
    } else if (sdk_job.type == kJobBestEffort) {
        job->set_priority(::baidu::galaxy::proto::kJobBestEffort);
    } else {
        fprintf(stderr, "job type must be kJobService, kJobBatch, kJobBestEffort\n");
        return false;
    }
    
    for (size_t i = 0; i < sdk_job.volum_jobs.size(); ++i) {
        std::string volum_job = Strim(sdk_job.volum_jobs[i]);
        if (volum_job.empty()) {
            fprintf(stderr, "volum_jobs[%d] must not be empty\n", (int)i);
            return false;
        }
        job->add_volum_jobs(volum_job); 
    }


    if (!FillDeploy(sdk_job.deploy, job->mutable_deploy())) {
        return false;
    }
    //job->set_run_user(sdk_job.run_user);
    job->set_run_user("galaxy");
    if (!FillPodDescription(sdk_job.pod, job->mutable_pod())) {
        return false;
    }
    return true;
}

bool FillGrant(const Grant& sdk_grant, ::baidu::galaxy::proto::Grant* grant) {
    std::string pool = Strim(sdk_grant.pool);
    if (pool.empty()) {
        fprintf(stderr, "pool must not be empty\n");
        return false;
    } 
    grant->set_pool(pool);

    if (sdk_grant.action == kActionAdd) {
        grant->set_action(::baidu::galaxy::proto::kActionAdd);
    } else if (sdk_grant.action == kActionRemove) {
        grant->set_action(::baidu::galaxy::proto::kActionRemove);
    } else if (sdk_grant.action == kActionSet) {
        grant->set_action(::baidu::galaxy::proto::kActionSet);
    } else if (sdk_grant.action == kActionClear) {
        grant->set_action(::baidu::galaxy::proto::kActionClear);
    } else {
        fprintf(stderr, "action must be kActionAdd, kActionRemove, kActionSet or kActionClear\n");
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < sdk_grant.authority.size(); ++i) {
        switch (sdk_grant.authority[i]) {
        case kAuthorityCreateContainer:
            grant->add_authority(::baidu::galaxy::proto::kAuthorityCreateContainer);
            break;
        case kAuthorityRemoveContainer:
            grant->add_authority(::baidu::galaxy::proto::kAuthorityRemoveContainer);
            break;
        case kAuthorityUpdateContainer:
            grant->add_authority(::baidu::galaxy::proto::kAuthorityUpdateContainer);
            break;
        case kAuthorityListContainer:
            grant->add_authority(::baidu::galaxy::proto::kAuthorityListContainer);
            break;
        case kAuthoritySubmitJob:
            grant->add_authority(::baidu::galaxy::proto::kAuthoritySubmitJob);
            break;
        case kAuthorityRemoveJob:
            grant->add_authority(::baidu::galaxy::proto::kAuthorityRemoveJob);
            break;
        case kAuthorityUpdateJob:
            grant->add_authority(::baidu::galaxy::proto::kAuthorityUpdateJob);
            break;
        case kAuthorityListJobs:
            grant->add_authority(::baidu::galaxy::proto::kAuthorityListJobs);
            break;
        default:
            ok = false;
        }
        if (!ok) {
            break;
        }
    }
    return ok;
}

void PbJobDescription2SdkJobDescription(const ::baidu::galaxy::proto::JobDescription& pb_job, JobDescription* job) {
    job->name = pb_job.name();
    if (pb_job.priority() == ::baidu::galaxy::proto::kJobMonitor) {
        job->type = kJobMonitor;
    } else if (pb_job.priority() == ::baidu::galaxy::proto::kJobService) {
        job->type = kJobService;
    } else if (pb_job.priority() == ::baidu::galaxy::proto::kJobBatch) {
        job->type = kJobBatch;
    } else if (pb_job.priority() == ::baidu::galaxy::proto::kJobBestEffort) {
        job->type = kJobBestEffort;
    }
    job->version = pb_job.version();
    job->run_user = pb_job.run_user();
    
    for (int i = 0; i < pb_job.volum_jobs().size(); ++i) {
        job->volum_jobs.push_back(pb_job.volum_jobs(i)); 
    }

    job->deploy.replica = pb_job.deploy().replica();
    job->deploy.step = pb_job.deploy().step();
    job->deploy.interval = pb_job.deploy().interval();
    job->deploy.max_per_host = pb_job.deploy().max_per_host();
    job->deploy.tag = pb_job.deploy().tag();
    job->deploy.update_break_count = pb_job.deploy().update_break_count(); 
    for (int i = 0; i < pb_job.deploy().pools().size(); ++i) {
        job->deploy.pools.push_back(pb_job.deploy().pools(i));
    }
    
    job->pod.workspace_volum.size = pb_job.pod().workspace_volum().size();
    job->pod.workspace_volum.type = (VolumType)pb_job.pod().workspace_volum().type();
    job->pod.workspace_volum.medium = (VolumMedium)pb_job.pod().workspace_volum().medium();
    job->pod.workspace_volum.source_path = pb_job.pod().workspace_volum().source_path();
    job->pod.workspace_volum.dest_path = pb_job.pod().workspace_volum().dest_path();
    job->pod.workspace_volum.readonly = pb_job.pod().workspace_volum().readonly();
    job->pod.workspace_volum.exclusive = pb_job.pod().workspace_volum().exclusive();
    job->pod.workspace_volum.use_symlink = pb_job.pod().workspace_volum().use_symlink();

    for (int i = 0; i < pb_job.pod().data_volums().size(); ++i) {
        VolumRequired volum;
        volum.size = pb_job.pod().data_volums(i).size();
        volum.type = (VolumType)pb_job.pod().data_volums(i).type();
        volum.medium = (VolumMedium)pb_job.pod().data_volums(i).medium();
        volum.source_path = pb_job.pod().data_volums(i).source_path();
        volum.dest_path = pb_job.pod().data_volums(i).dest_path();
        volum.readonly = pb_job.pod().data_volums(i).readonly();
        volum.exclusive = pb_job.pod().data_volums(i).exclusive();
        volum.use_symlink = pb_job.pod().data_volums(i).use_symlink();
        job->pod.data_volums.push_back(volum);
    }

    for (int i = 0; i < pb_job.pod().tasks().size(); ++i) {
        TaskDescription task;
        task.id = pb_job.pod().tasks(i).id();
        task.cpu.milli_core = pb_job.pod().tasks(i).cpu().milli_core();
        task.cpu.excess = pb_job.pod().tasks(i).cpu().excess();
        task.memory.size = pb_job.pod().tasks(i).memory().size();
        task.memory.excess = pb_job.pod().tasks(i).memory().excess();
        task.tcp_throt.recv_bps_quota = pb_job.pod().tasks(i).tcp_throt().recv_bps_quota();
        task.tcp_throt.recv_bps_excess = pb_job.pod().tasks(i).tcp_throt().recv_bps_excess();
        task.tcp_throt.send_bps_quota = pb_job.pod().tasks(i).tcp_throt().send_bps_quota();
        task.tcp_throt.send_bps_excess = pb_job.pod().tasks(i).tcp_throt().send_bps_excess();
        task.blkio.weight = pb_job.pod().tasks(i).blkio().weight();
        for (int j = 0; j < pb_job.pod().tasks(i).ports().size(); ++j) {
            PortRequired port;
            port.port_name = pb_job.pod().tasks(i).ports(j).port_name();
            port.port = pb_job.pod().tasks(i).ports(j).port();
            port.real_port = pb_job.pod().tasks(i).ports(j).real_port();
            task.ports.push_back(port);
        }
        task.exe_package.start_cmd = pb_job.pod().tasks(i).exe_package().start_cmd();
        task.exe_package.stop_cmd = pb_job.pod().tasks(i).exe_package().stop_cmd();
        task.exe_package.package.source_path = pb_job.pod().tasks(i).exe_package().package().source_path();
        task.exe_package.package.dest_path = pb_job.pod().tasks(i).exe_package().package().dest_path();
        task.exe_package.package.version = pb_job.pod().tasks(i).exe_package().package().version();

        task.data_package.reload_cmd = pb_job.pod().tasks(i).data_package().reload_cmd();
        for (int j = 0; j < pb_job.pod().tasks(i).data_package().packages().size(); ++j) {
            Package package;
            package.source_path = pb_job.pod().tasks(i).data_package().packages(j).source_path();
            package.dest_path = pb_job.pod().tasks(i).data_package().packages(j).dest_path();
            package.version = pb_job.pod().tasks(i).data_package().packages(j).version();
            task.data_package.packages.push_back(package);
        }
        for (int j = 0; j < pb_job.pod().tasks(i).services().size(); ++j) {
            Service service;
            service.service_name = pb_job.pod().tasks(i).services(j).service_name(); 
            service.port_name = pb_job.pod().tasks(i).services(j).port_name(); 
            service.use_bns = pb_job.pod().tasks(i).services(j).use_bns(); 
            service.tag = pb_job.pod().tasks(i).services(j).tag();
            service.health_check_type = pb_job.pod().tasks(i).services(j).health_check_type();
            service.health_check_script = pb_job.pod().tasks(i).services(j).health_check_script();
            //service.token = pb_job.pod().tasks(i).services(j).token(); 
            task.services.push_back(service);
        }
        job->pod.tasks.push_back(task);
    }
}

} // end namespace sdk
} // end namespace galaxy
} // end namespace baidu

#endif  // GALAXY_SDK_UTIL_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
