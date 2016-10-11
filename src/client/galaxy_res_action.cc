// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <tprinter.h>
#include "galaxy_res_action.h"

DEFINE_string(nexus_root, "/galaxy3", "root prefix on nexus");
DEFINE_string(nexus_addr, "", "nexus server list");
DEFINE_string(resman_path, "/resman", "resman path on nexus");
DEFINE_string(appmaster_path, "/appmaster", "appmaster path on nexus");
DEFINE_string(username, "default", "username");
DEFINE_string(token, "default", "token");

namespace baidu {
namespace galaxy {
namespace client {

ResAction::ResAction() : resman_(NULL) { 
    user_.user =  FLAGS_username;
    user_.token = FLAGS_token;
}

ResAction::~ResAction() {
    if (NULL != resman_) {
        delete resman_;
    }
}

bool ResAction::Init() {
    std::string path = FLAGS_nexus_root + FLAGS_resman_path;
    resman_ = ::baidu::galaxy::sdk::ResourceManager::ConnectResourceManager(FLAGS_nexus_addr, path);
    if (resman_ == NULL) {
        return false;
    }
    return true;
}

bool ResAction::CreateContainerGroup(const std::string& json_file, const std::string& container_type) {
    if (json_file.empty()) {
        fprintf(stderr, "json_file and jobid are needed\n");
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }
    
    ::baidu::galaxy::sdk::CreateContainerGroupRequest request;
    ::baidu::galaxy::sdk::CreateContainerGroupResponse response;

    baidu::galaxy::sdk::JobDescription job;
    int ok = 0;
    if (container_type.compare("normal") == 0) {
        request.desc.container_type = ::baidu::galaxy::sdk::kNormalContainer;
        ok = BuildJobFromConfig(json_file, &job);
    } else if (container_type.compare("volum") == 0) {
        ok = BuildJobFromConfig(json_file, &job, true);
        request.desc.container_type = ::baidu::galaxy::sdk::kVolumContainer;
    } else {
        fprintf(stderr, "container_type must be normal or volum\n");
        return false;
    }
    
    if (ok != 0) {
        return false;
    }

    request.user = user_;
    request.replica = job.deploy.replica;
    request.name = job.name;
    request.desc.priority = job.type;
    request.desc.run_user = user_.user;
    request.desc.version = job.version;
    request.desc.volum_jobs.assign(job.volum_jobs.begin(), job.volum_jobs.end()); //attemp new
    request.desc.max_per_host = job.deploy.max_per_host;
    request.desc.workspace_volum = job.pod.workspace_volum;
    request.desc.data_volums.assign(job.pod.data_volums.begin(), job.pod.data_volums.end());
    //request.desc.cmd_line = "sh appworker.sh";
    request.desc.tag = job.deploy.tag;
    request.desc.pool_names.assign(job.deploy.pools.begin(), job.deploy.pools.end());

    if (container_type.compare("normal") == 0) {
        for (uint32_t i = 0; i < job.pod.tasks.size(); ++i) {
            ::baidu::galaxy::sdk::Cgroup cgroup;
            time_t timestamp;
            time(&timestamp);
            cgroup.cpu = job.pod.tasks[i].cpu;
            cgroup.memory = job.pod.tasks[i].memory;
            cgroup.tcp_throt = job.pod.tasks[i].tcp_throt;
            cgroup.blkio = job.pod.tasks[i].blkio;
        
            for (uint32_t j = 0; j < job.pod.tasks[i].ports.size(); ++j) {
                ::baidu::galaxy::sdk::PortRequired port;
                port.port_name = job.pod.tasks[i].ports[j].port_name;
                port.port = job.pod.tasks[i].ports[j].port;
                port.real_port = job.pod.tasks[i].ports[j].real_port;
                cgroup.ports.push_back(port);
            }

            request.desc.cgroups.push_back(cgroup);
        }
    }

    bool ret = resman_->CreateContainerGroup(request, &response);
    if (ret) {
        printf("Create container group %s\n", response.id.c_str());
    } else {
        printf("Create container group failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::UpdateContainerGroup(const std::string& json_file, const std::string& id, const std::string& container_type) {
    if (json_file.empty() || id.empty()) {
        fprintf(stderr, "json_file and id are needed\n");
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }
    
    ::baidu::galaxy::sdk::UpdateContainerGroupRequest request;
    ::baidu::galaxy::sdk::UpdateContainerGroupResponse response;
    
    ::baidu::galaxy::sdk::JobDescription job;
    int ok = 0;
    if (container_type.compare("normal") == 0) {
        request.desc.container_type = ::baidu::galaxy::sdk::kNormalContainer;
        ok = BuildJobFromConfig(json_file, &job);
    } else if (container_type.compare("volum") == 0) {
        request.desc.container_type = ::baidu::galaxy::sdk::kVolumContainer;
        ok = BuildJobFromConfig(json_file, &job, true);
    } else {
        fprintf(stderr, "container_type must be normal or volum\n");
        return false;
    }
    
    if (ok != 0) {
        return false;
    }

    request.user = user_;
    request.replica = job.deploy.replica;
    request.id = id;
    request.interval = job.deploy.interval;
    request.desc.max_per_host = job.deploy.max_per_host;
    //request.name = job.name;
    request.desc.priority = job.type;
    request.desc.run_user = user_.user;
    request.desc.version = job.version;
    request.desc.volum_jobs.assign(job.volum_jobs.begin(), job.volum_jobs.end()); //attemp new
    request.desc.workspace_volum = job.pod.workspace_volum;
    request.desc.data_volums.assign(job.pod.data_volums.begin(), job.pod.data_volums.end());
    //request.desc.cmd_line = "sh appworker.sh";
    request.desc.tag = job.deploy.tag;
    request.desc.pool_names.assign(job.deploy.pools.begin(), job.deploy.pools.end());

   
    if (container_type.compare("normal") == 0) {
        for (uint32_t i = 0; i < job.pod.tasks.size(); ++i) {
            ::baidu::galaxy::sdk::Cgroup cgroup;
            cgroup.cpu = job.pod.tasks[i].cpu;
            cgroup.memory = job.pod.tasks[i].memory;
            cgroup.tcp_throt = job.pod.tasks[i].tcp_throt;
            cgroup.blkio = job.pod.tasks[i].blkio;
        
            for (uint32_t j = 0; j < job.pod.tasks[i].ports.size(); ++j) {
                ::baidu::galaxy::sdk::PortRequired port;
                port.port_name = job.pod.tasks[i].ports[j].port_name;
                port.port = job.pod.tasks[i].ports[j].port;
                port.real_port = job.pod.tasks[i].ports[j].real_port;
                cgroup.ports.push_back(port);
            }

            request.desc.cgroups.push_back(cgroup);
        }
    }

    bool ret = resman_->UpdateContainerGroup(request, &response);
    if (ret) {
        printf("Update container group %s\n", id.c_str());
    } else {
        printf("Update container group failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::RemoveContainerGroup(const std::string& id) {
    if (id.empty()) {
        fprintf(stderr, "id is needed\n");
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }
    ::baidu::galaxy::sdk::RemoveContainerGroupRequest request;
    ::baidu::galaxy::sdk::RemoveContainerGroupResponse response;
    request.user = user_;
    request.id = id;

    bool ret = resman_->RemoveContainerGroup(request, &response);
    if (ret) {
        printf("Remove container group %s\n", id.c_str());
    } else {
        printf("Remove container group failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::ListContainerGroups(const std::string& soptions) {
    if(!this->Init()) {
        return false;
    }

    std::vector<std::string> options;
    ::baidu::common::SplitString(soptions, ",", &options);

    ::baidu::galaxy::sdk::ListContainerGroupsRequest request;
    ::baidu::galaxy::sdk::ListContainerGroupsResponse response;
    request.user = user_;

    bool ret = resman_->ListContainerGroups(request, &response);
    if (ret) {
        std::string array_headers[6] = {"", "id", "replica", "type", "user", "r/a/p/d"};
        std::vector<std::string> headers(array_headers, array_headers + 6);
        if (find(options.begin(), options.end(), "cpu") != options.end()) {
            headers.push_back("cpu(a/u)");
        }
        if (find(options.begin(), options.end(), "mem") != options.end()) {
            headers.push_back("mem(a/u)");
        }
        if (find(options.begin(), options.end(), "volums") != options.end()) {
            headers.push_back("volums(med/a/u)");
        }
        if (options.size() == 0) {
            headers.push_back("cpu(a/u)");
            headers.push_back("mem(a/u)");
            headers.push_back("volums(med/a/u)");
        }
        headers.push_back("create_time");
        headers.push_back("update_time");
        ::baidu::common::TPrinter containers(headers.size());
        containers.AddRow(headers);
        for (uint32_t i = 0; i < response.containers.size(); ++i) {
            std::string sstatus = ::baidu::common::NumToString(response.containers[i].ready) + "/" +
                                  ::baidu::common::NumToString(response.containers[i].allocating) + "/" +
                                  ::baidu::common::NumToString(response.containers[i].pending) + "/" +
                                  ::baidu::common::NumToString(response.containers[i].destroying);

            std::string scpu;
            if (options.size() == 0 || find(options.begin(), options.end(), "cpu") != options.end()) {
                    scpu = ::baidu::common::NumToString(response.containers[i].cpu.assigned / 1000.0) + "/" +
                           ::baidu::common::NumToString(response.containers[i].cpu.used / 1000.0);
            }

            std::string smem;
            if (options.size() == 0 || find(options.begin(), options.end(), "mem") != options.end()) {
                smem = HumanReadableString(response.containers[i].memory.assigned) + "/" +
                       HumanReadableString(response.containers[i].memory.used);
            }

            std::vector<std::string> values;
            if (options.size() == 0 || find(options.begin(), options.end(), "volums") != options.end()) {
                for (size_t j = 0; j < response.containers[i].volums.size(); ++j) {
                    values.clear();
                    std::string svolums;
                    svolums = StringVolumMedium(response.containers[i].volums[j].medium) + "/"
                              + HumanReadableString(response.containers[i].volums[j].volum.assigned) + "/"
                              + HumanReadableString(response.containers[i].volums[j].volum.used);
                            //+ response.containers[i].volums[j].device_path;
                    if (j == 0) {
                        values.push_back(baidu::common::NumToString(i));
                        values.push_back(response.containers[i].id);
                        values.push_back(::baidu::common::NumToString(response.containers[i].replica));
                        values.push_back(StringContainerType(response.containers[i].container_type));
                        values.push_back(response.containers[i].user_name);
                        values.push_back(sstatus);
                        if (!scpu.empty()) {
                            values.push_back(scpu);
                        }
                        if (!smem.empty()) {
                            values.push_back(smem);
                        };
                        values.push_back(svolums);
                        values.push_back(FormatDate(response.containers[i].submit_time));
                        values.push_back(FormatDate(response.containers[i].submit_time));
                    } else {
                        int base_size = sizeof(array_headers) / sizeof(std::string);
                        for (int base_it = 0; base_it < base_size; ++base_it) {
                            values.push_back("");
                        }

                        if (!scpu.empty()) {
                            values.push_back("");
                        }
                        if (!smem.empty()) {
                            values.push_back("");
                        };
                        values.push_back(svolums);
                        values.push_back("");
                        values.push_back("");
                    }
                    containers.AddRow(values);
                }

                if (response.containers[i].volums.size() == 0) {
                    values.push_back(baidu::common::NumToString(i));
                    values.push_back(response.containers[i].id);
                    values.push_back(::baidu::common::NumToString(response.containers[i].replica));
                    values.push_back(StringContainerType(response.containers[i].container_type));
                    values.push_back(response.containers[i].user_name);
                    values.push_back(sstatus);
                    if (!scpu.empty()) {
                        values.push_back(scpu);
                    }
                    if (!smem.empty()) {
                        values.push_back(smem);
                    };
                    values.push_back("");
                    values.push_back(FormatDate(response.containers[i].submit_time));
                    values.push_back(FormatDate(response.containers[i].submit_time));
                    containers.AddRow(values);
                }
            }

            if (options.size() != 0 && find(options.begin(), options.end(), "volums") == options.end()) {
                values.push_back(baidu::common::NumToString(i));
                values.push_back(response.containers[i].id);
                values.push_back(::baidu::common::NumToString(response.containers[i].replica));
                values.push_back(StringContainerType(response.containers[i].container_type));
                values.push_back(response.containers[i].user_name);
                values.push_back(sstatus);
                if (!scpu.empty()) {
                    values.push_back(scpu);
                }
                if (!smem.empty()) {
                    values.push_back(smem);
                };
                values.push_back(FormatDate(response.containers[i].submit_time));
                values.push_back(FormatDate(response.containers[i].submit_time));
                containers.AddRow(values);
            }
        }
        printf("%s\n", containers.ToString().c_str());
    } else {
        printf("List container group failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::ShowAgent(const std::string& endpoint, const std::string& soptions) {
    if (endpoint.empty()) {
        fprintf(stderr, "endpoint is needed\n");
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }

    std::vector<std::string> options;
    ::baidu::common::SplitString(soptions, ",", &options);

    ::baidu::galaxy::sdk::ShowAgentRequest request;
    ::baidu::galaxy::sdk::ShowAgentResponse response;
    request.user = user_;
    request.endpoint = endpoint;

    bool ret = resman_->ShowAgent(request, &response);
    if (ret) {
        printf("containers infomation\n");
        std::string array_headers[5] = {"", "id", "endpoint", "status", "last_error"};
        std::vector<std::string> headers(array_headers, array_headers + 5);
        if (find(options.begin(), options.end(), "cpu") != options.end()) {
            headers.push_back("cpu(a/u)");
        }
        if (find(options.begin(), options.end(), "mem") != options.end()) {
            headers.push_back("mem(a/u)");
        }
        if (find(options.begin(), options.end(), "volums") != options.end()) {
            headers.push_back("vol(medium/a/u)");
        }
        if (options.size() == 0) {
            headers.push_back("cpu(a/u)");
            headers.push_back("mem(a/u)");
            headers.push_back("vol(medium/a/u)");
        }
        ::baidu::common::TPrinter containers(headers.size());
        containers.AddRow(headers);
        for (uint32_t i = 0; i < response.containers.size(); ++i) {
            //size_t pos = response.containers[i].id.rfind("."); 
            //std::string id(response.containers[i].id, pos + 1, response.containers[i].id.size()- (pos + 1));

            std::string scpu;
            if (options.size() == 0 || find(options.begin(), options.end(), "cpu") != options.end()) {
                //std::string scpu = ::baidu::common::NumToString(response.containers[i].cpu.total / 1000.0) + "/" +
                scpu = ::baidu::common::NumToString(response.containers[i].cpu.assigned / 1000.0) + "/" +
                       ::baidu::common::NumToString(response.containers[i].cpu.used / 1000.0);
            }

            std::string smem;
            if (options.size() == 0 || find(options.begin(), options.end(), "mem") != options.end()) {
            //std::string smem = HumanReadableString(response.containers[i].memory.total) + "/" +
                smem = HumanReadableString(response.containers[i].memory.assigned) + "/" +
                       HumanReadableString(response.containers[i].memory.used);
            }
            
            std::vector<std::string> values;      
            if (options.size() == 0 || find(options.begin(), options.end(), "volums") != options.end()) {
                for (uint32_t j = 0; j < response.containers[i].volums.size(); ++j) {
                    values.clear();
                    std::string svolums;
                    //svolums =  "vol_" + ::baidu::common::NumToString(j) + " " 
                    svolums =  StringVolumMedium(response.containers[i].volums[j].medium) + "/" 
                                //+ HumanReadableString(response.containers[i].volums[j].volum.total) + "/"
                                + HumanReadableString(response.containers[i].volums[j].volum.assigned) + "/"
                                + HumanReadableString(response.containers[i].volums[j].volum.used) + " "
                                + response.containers[i].volums[j].device_path;
                    if (j == 0) {
                        values.push_back(::baidu::common::NumToString(i));
                        values.push_back(response.containers[i].id);
                        values.push_back(response.containers[i].endpoint);
                        values.push_back(StringContainerStatus(response.containers[i].status));
                        values.push_back(StringResourceError(response.containers[i].last_res_err));
                        if (!scpu.empty()) {
                            values.push_back(scpu);
                        }
                        if (!smem.empty()) {
                            values.push_back(smem);
                        }
                        values.push_back(svolums);

                    } else {
                        int base_size = sizeof(array_headers) / sizeof(std::string);
                        for (int base_it = 0; base_it < base_size; ++base_it) {
                            values.push_back("");
                        }

                        if (!scpu.empty()) {
                            values.push_back("");
                        }
                        if (!smem.empty()) {
                            values.push_back("");
                        }
                        values.push_back(svolums);
                    }
                    containers.AddRow(values);
                }

                if (response.containers[i].volums.size() == 0) {
                    values.push_back(::baidu::common::NumToString(i));
                    values.push_back(response.containers[i].id);
                    values.push_back(response.containers[i].endpoint);
                    values.push_back(StringContainerStatus(response.containers[i].status));
                    values.push_back(StringResourceError(response.containers[i].last_res_err));
                    if (!scpu.empty()) {
                        values.push_back(scpu);
                    }
                    if (!smem.empty()) {
                        values.push_back(smem);
                    }
                    values.push_back("");
                    containers.AddRow(values);
                }
            }
            if (options.size() != 0 && find(options.begin(), options.end(), "volums") == options.end()) {
                values.push_back(::baidu::common::NumToString(i));
                values.push_back(response.containers[i].id);
                values.push_back(response.containers[i].endpoint);
                values.push_back(StringContainerStatus(response.containers[i].status));
                values.push_back(StringResourceError(response.containers[i].last_res_err));
                if (!scpu.empty()) {
                    values.push_back(scpu);
                }
                if (!smem.empty()) {
                    values.push_back(smem);
                }
                containers.AddRow(values);
            }
        }
        printf("%s\n", containers.ToString().c_str());
    } else {
        printf("Show container group failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return false;
}

bool ResAction::ShowContainerGroup(const std::string& id) {
    if (id.empty()) {
        fprintf(stderr, "id is needed\n");
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }
    ::baidu::galaxy::sdk::ShowContainerGroupRequest request;
    ::baidu::galaxy::sdk::ShowContainerGroupResponse response;
    request.user = user_;
    request.id = id;

    bool ret = resman_->ShowContainerGroup(request, &response);
    if (ret) {
        printf("base infomation\n");
        ::baidu::common::TPrinter base(8);
        base.AddRow(8, "user", "version", "priority", "type", "cmd_line", "max_per_host", "tag", "pools");
        std::string pools; 
        for (size_t i = 0; i < response.desc.pool_names.size(); ++i) {
            pools += response.desc.pool_names[i];
            if (i != response.desc.pool_names.size() - 1) {
                pools += ",";
            }
        }
        base.AddRow(8,  response.desc.run_user.c_str(),
                        response.desc.version.c_str(),
                        StringJobType((::baidu::galaxy::sdk::JobType)response.desc.priority).c_str(),
                        StringContainerType(response.desc.container_type).c_str(),
                        response.desc.cmd_line.c_str(),
                        ::baidu::common::NumToString(response.desc.max_per_host).c_str(),
                        response.desc.tag.c_str(),
                        pools.c_str()
                   );

        printf("%s\n", base.ToString().c_str());

        printf("workspace volum infomation\n");
        ::baidu::common::TPrinter workspace_volum(7);
        workspace_volum.AddRow(7, "size", "type", "medium", "dest_path", "readonly", "exclusive", "use_symlink");
        workspace_volum.AddRow(7, HumanReadableString(response.desc.workspace_volum.size).c_str(),
                                  StringVolumType(response.desc.workspace_volum.type).c_str(),
                                  StringVolumMedium(response.desc.workspace_volum.medium).c_str(),
                                  response.desc.workspace_volum.dest_path.c_str(),
                                  StringBool(response.desc.workspace_volum.readonly).c_str(),
                                  StringBool(response.desc.workspace_volum.exclusive).c_str(),
                                  StringBool(response.desc.workspace_volum.use_symlink).c_str()
                               );
        printf("%s\n", workspace_volum.ToString().c_str());


        printf("data volums infomation\n");
         ::baidu::common::TPrinter data_volums(9);
         data_volums.AddRow(9, "", "size", "type", "medium", "source_path", "dest_path", "readonly", "exclusive", "use_symlink");

        for (uint32_t i = 0; i < response.desc.data_volums.size(); ++i) {
            data_volums.AddRow(9, ::baidu::common::NumToString(i).c_str(),
                                  HumanReadableString(response.desc.data_volums[i].size).c_str(),
                                  StringVolumType(response.desc.data_volums[i].type).c_str(),
                                  StringVolumMedium(response.desc.data_volums[i].medium).c_str(),
                                  response.desc.data_volums[i].source_path.c_str(),
                                  response.desc.data_volums[i].dest_path.c_str(),
                                  StringBool(response.desc.data_volums[i].readonly).c_str(),
                                  StringBool(response.desc.data_volums[i].exclusive).c_str(),
                                  StringBool(response.desc.data_volums[i].use_symlink).c_str()
                              );
        }
        printf("%s\n", data_volums.ToString().c_str());
        
        printf("cgroups infomation\n");
        ::baidu::common::TPrinter cgroups(11);
        cgroups.AddRow(11, "", "id", "cpu_cores", "cpu_excess", "mem_size", "mem_excess", "tcp_recv_bps", "tcp_recv_excess", "tcp_send_bps", "tcp_send_excess", "blk_weight");

        for (uint32_t i = 0; i < response.desc.cgroups.size(); ++i) {
            cgroups.AddRow(11, ::baidu::common::NumToString(i).c_str(),
                               response.desc.cgroups[i].id.c_str(),
                               ::baidu::common::NumToString(response.desc.cgroups[i].cpu.milli_core / 1000.0).c_str(),
                               StringBool(response.desc.cgroups[i].cpu.excess).c_str(),
                               HumanReadableString(response.desc.cgroups[i].memory.size).c_str(),
                               StringBool(response.desc.cgroups[i].memory.excess).c_str(),
                               HumanReadableString(response.desc.cgroups[i].tcp_throt.recv_bps_quota).c_str(),
                               StringBool(response.desc.cgroups[i].tcp_throt.recv_bps_excess).c_str(),
                               HumanReadableString(response.desc.cgroups[i].tcp_throt.send_bps_quota).c_str(),
                               StringBool(response.desc.cgroups[i].tcp_throt.send_bps_excess).c_str(),
                               ::baidu::common::NumToString(response.desc.cgroups[i].blkio.weight).c_str()
                            );
        }
        printf("%s\n", cgroups.ToString().c_str());
                
        printf("containers infomation\n");
        ::baidu::common::TPrinter containers(8);
        containers.AddRow(8, "", "id", "endpoint", "status", "last_error", "cpu(a/u)", "mem(a/u)", "volums(id/medium/a/u)");
        for (uint32_t i = 0; i < response.containers.size(); ++i) {
            size_t pos = response.containers[i].id.rfind("."); 
            std::string id(response.containers[i].id, pos + 1, response.containers[i].id.size()- (pos + 1));

            //std::string scpu = ::baidu::common::NumToString(response.containers[i].cpu.total / 1000.0) + "/" +
            std::string scpu = ::baidu::common::NumToString(response.containers[i].cpu.assigned / 1000.0) + "/" +
                               ::baidu::common::NumToString(response.containers[i].cpu.used / 1000.0);
             
            //std::string smem = HumanReadableString(response.containers[i].memory.total) + "/" +
            std::string smem = HumanReadableString(response.containers[i].memory.assigned) + "/" +
                               HumanReadableString(response.containers[i].memory.used);
                   
            for (uint32_t j = 0; j < response.containers[i].volums.size(); ++j) {
                std::string svolums;
                svolums +=  "vol_" + ::baidu::common::NumToString(j) + " " 
                            + StringVolumMedium(response.containers[i].volums[j].medium) + " " 
                            //+ HumanReadableString(response.containers[i].volums[j].volum.total) + "/"
                            + HumanReadableString(response.containers[i].volums[j].volum.assigned) + "/"
                            + HumanReadableString(response.containers[i].volums[j].volum.used) + " "
                            + response.containers[i].volums[j].device_path;
                if (j == 0) {
                    containers.AddRow(8, ::baidu::common::NumToString(i).c_str(),
                                         id.c_str(),
                                         response.containers[i].endpoint.c_str(),
                                         StringContainerStatus(response.containers[i].status).c_str(),
                                         StringResourceError(response.containers[i].last_res_err).c_str(),
                                         scpu.c_str(),
                                         smem.c_str(),
                                         svolums.c_str()
                                     );

                } else {
                    containers.AddRow(8, "",
                                         "",
                                         "",
                                         "",
                                         "",
                                         "",
                                         "",
                                         svolums.c_str()
                                     );
                }
            }

            if (response.containers[i].volums.size() == 0) {
                containers.AddRow(8, ::baidu::common::NumToString(i).c_str(),
                                     id.c_str(),
                                     response.containers[i].endpoint.c_str(),
                                     ::baidu::common::NumToString(response.containers[i].status).c_str(),
                                     StringResourceError(response.containers[i].last_res_err).c_str(),
                                     scpu.c_str(),
                                     smem.c_str(),
                                     ""
                                 );
            }
        }
        printf("%s\n", containers.ToString().c_str());

    } else {
        printf("Show container group failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;

}

bool ResAction::AddAgent(const std::string& pool, const std::string& endpoint) {
    if (pool.empty() || endpoint.empty()) {
        return false;
    }
    if(!this->Init()) {
        return false;
    }
    ::baidu::galaxy::sdk::AddAgentRequest request;
    ::baidu::galaxy::sdk::AddAgentResponse response;
    request.user = user_;
    request.endpoint = endpoint;
    request.pool = pool;

    bool ret = resman_->AddAgent(request, &response);
    if (ret) {
        printf("Add agent successfully\n");
    } else {
        printf("Add agent failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::RemoveAgent(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::RemoveAgentRequest request;
    ::baidu::galaxy::sdk::RemoveAgentResponse response;
    request.user = user_;
    request.endpoint = endpoint;

    bool ret = resman_->RemoveAgent(request, &response);
    if (ret) {
        printf("Remove agent successfully\n");
    } else {
        printf("remove agent failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }

    return ret;

}

bool ResAction::ListAgents(const std::string& soptions) {
    if(!this->Init()) {
        return false;
    }
    
    std::vector<std::string> options;
    ::baidu::common::SplitString(soptions, ",", &options);


    ::baidu::galaxy::sdk::ListAgentsRequest request;
    ::baidu::galaxy::sdk::ListAgentsResponse response;
    request.user = user_;

    bool ret = resman_->ListAgents(request, &response);
    if (ret) {
        std::string array_headers[6] = {"", "endpoint", "status", "pool", "tags", "containers"};
        std::vector<std::string> headers(array_headers, array_headers + 6);
        if (find(options.begin(), options.end(), "cpu") != options.end()) {
            headers.push_back("cpu(t/a/u)");
        }
        if (find(options.begin(), options.end(), "mem") != options.end()) {
            headers.push_back("mem(t/a/u)");
        }
        if (find(options.begin(), options.end(), "volums") != options.end()) {
            headers.push_back("vol(med/t/a/u/path)");
        }
        if (options.size() == 0) {
            headers.push_back("cpu(t/a/u)");
            headers.push_back("mem(t/a/u)");
            headers.push_back("vol(med/t/a/u/path)");
        }
        ::baidu::common::TPrinter agents(headers.size());
        agents.AddRow(headers);
        for (uint32_t i = 0; i < response.agents.size(); ++i) {
            std::string tags;
            for (uint32_t j = 0; j < response.agents[i].tags.size(); ++j) {
                tags += response.agents[i].tags[j];
                if (j != response.agents[i].tags.size() - 1) {
                    tags += ",";
                }
            }

            std::string scpu;

            if (options.size() == 0 || find(options.begin(), options.end(), "cpu") != options.end()) {
                scpu = ::baidu::common::NumToString(response.agents[i].cpu.total / 1000.0) + "/" +
                       ::baidu::common::NumToString(response.agents[i].cpu.assigned / 1000.0) + "/" +
                       ::baidu::common::NumToString(response.agents[i].cpu.used / 1000.0);
            }

            std::string smem;
            if (options.size() == 0 || find(options.begin(), options.end(), "mem") != options.end()) {

                smem = HumanReadableString(response.agents[i].memory.total) + "/" +
                       HumanReadableString(response.agents[i].memory.assigned) + "/" +
                       HumanReadableString(response.agents[i].memory.used);
            }
            
            std::vector<std::string> values;
            if (options.size() == 0 || find(options.begin(), options.end(), "volums") != options.end()) {
                for (uint32_t j = 0; j < response.agents[i].volums.size(); ++j) {
                    values.clear();
                    std::string svolums;
                    svolums +=  "vol_" + ::baidu::common::NumToString(j) + " "
                                + StringVolumMedium(response.agents[i].volums[j].medium) + " "
                                + HumanReadableString(response.agents[i].volums[j].volum.total) + "/"
                                + HumanReadableString(response.agents[i].volums[j].volum.assigned) + "/"
                                + HumanReadableString(response.agents[i].volums[j].volum.used) + " "
                                + response.agents[i].volums[j].device_path;
                    if (j == 0) {
                        values.push_back(::baidu::common::NumToString(i));
                        values.push_back(response.agents[i].endpoint);
                        values.push_back(StringAgentStatus(response.agents[i].status));
                        values.push_back(response.agents[i].pool);
                        values.push_back(tags);
                        values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                        if (!scpu.empty()) {
                            values.push_back(scpu);
                        }
                        if (!smem.empty()) {
                            values.push_back(smem);
                        }
                        values.push_back(svolums);
                    } else {
                        int base_size = sizeof(array_headers) / sizeof(std::string);
                        for (int base_it = 0; base_it < base_size; ++base_it) {
                            values.push_back("");
                        }

                        if (!scpu.empty()) {
                            values.push_back("");
                        }
                        if (!smem.empty()) {
                            values.push_back("");
                        }
                        values.push_back(svolums);
                    }
                    agents.AddRow(values);
                }

                if (response.agents[i].volums.size() == 0) {
                    values.push_back(::baidu::common::NumToString(i));
                    values.push_back(response.agents[i].endpoint);
                    values.push_back(StringAgentStatus(response.agents[i].status));
                    values.push_back(response.agents[i].pool);
                    values.push_back(tags.c_str());
                    values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                    if (!scpu.empty()) {
                        values.push_back(scpu);
                    }
                    if (!smem.empty()) {
                        values.push_back(smem);
                    }
                    values.push_back("");
                    agents.AddRow(values);
                }
            }
            
            if (options.size() != 0 && find(options.begin(), options.end(), "volums") == options.end()) {
                values.push_back(::baidu::common::NumToString(i));
                values.push_back(response.agents[i].endpoint);
                values.push_back(StringAgentStatus(response.agents[i].status));
                values.push_back(response.agents[i].pool);
                values.push_back(tags.c_str());
                values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                if (!scpu.empty()) {
                    values.push_back(scpu);
                }
                if (!smem.empty()) {
                    values.push_back(smem);
                }
                agents.AddRow(values);
            }
        }
        printf("%s\n", agents.ToString().c_str());

    } else {
        printf("List agents failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }

    return ret;

}

bool ResAction::ListAgentsByTag(const std::string& tag, const std::string& pool, const std::string& soptions) {
    if (tag.empty()) {
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }

    std::vector<std::string> options;
    ::baidu::common::SplitString(soptions, ",", &options);

    ::baidu::galaxy::sdk::ListAgentsByTagRequest request;
    ::baidu::galaxy::sdk::ListAgentsByTagResponse response;
    request.user = user_;
    request.tag = tag;

    bool ret = resman_->ListAgentsByTag(request, &response);

    if (ret) {
        std::string array_headers[6] = {"", "endpoint", "status", "pool", "tags", "containers"};
        std::vector<std::string> headers(array_headers, array_headers + 6);
        if (find(options.begin(), options.end(), "cpu") != options.end()) {
            headers.push_back("cpu(t/a/u)");
        }
        if (find(options.begin(), options.end(), "mem") != options.end()) {
            headers.push_back("mem(t/a/u)");
        }
        if (find(options.begin(), options.end(), "volums") != options.end()) {
            headers.push_back("vol(med/t/a/u/path)");
        }
        if (options.size() == 0) {
            headers.push_back("cpu(t/a/u)");
            headers.push_back("mem(t/a/u)");
            headers.push_back("vol(med/t/a/u/path)");
        }
        ::baidu::common::TPrinter agents(headers.size());
        agents.AddRow(headers);
        int64_t count = 0;
        for (uint32_t i = 0; i < response.agents.size(); ++i) {
            if (!pool.empty() && pool.compare(response.agents[i].pool) != 0) {
                continue;
            }
            std::string tags;
            for (uint32_t j = 0; j < response.agents[i].tags.size(); ++j) {
                tags += response.agents[i].tags[j];
                if (j != response.agents[i].tags.size() - 1) {
                    tags += ",";
                }
            }

            std::string scpu;

            if (options.size() == 0 || find(options.begin(), options.end(), "cpu") != options.end()) {
                scpu = ::baidu::common::NumToString(response.agents[i].cpu.total / 1000.0) + "/" +
                       ::baidu::common::NumToString(response.agents[i].cpu.assigned / 1000.0) + "/" +
                       ::baidu::common::NumToString(response.agents[i].cpu.used / 1000.0);
            }

            std::string smem;
            if (options.size() == 0 || find(options.begin(), options.end(), "mem") != options.end()) {

                smem = HumanReadableString(response.agents[i].memory.total) + "/" +
                       HumanReadableString(response.agents[i].memory.assigned) + "/" +
                       HumanReadableString(response.agents[i].memory.used);
            }
            
            std::vector<std::string> values;
            if (options.size() == 0 || find(options.begin(), options.end(), "volums") != options.end()) {
                for (uint32_t j = 0; j < response.agents[i].volums.size(); ++j) {
                    values.clear();
                    std::string svolums;
                    svolums +=  "vol_" + ::baidu::common::NumToString(j) + " "
                                + StringVolumMedium(response.agents[i].volums[j].medium) + " "
                                + HumanReadableString(response.agents[i].volums[j].volum.total) + "/"
                                + HumanReadableString(response.agents[i].volums[j].volum.assigned) + "/"
                                + HumanReadableString(response.agents[i].volums[j].volum.used) + " "
                                + response.agents[i].volums[j].device_path;
                    if (j == 0) {
                        values.push_back(::baidu::common::NumToString(count));
                        values.push_back(response.agents[i].endpoint);
                        values.push_back(StringAgentStatus(response.agents[i].status));
                        values.push_back(response.agents[i].pool);
                        values.push_back(tags);
                        values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                        if (!scpu.empty()) {
                            values.push_back(scpu);
                        }
                        if (!smem.empty()) {
                            values.push_back(smem);
                        }
                        values.push_back(svolums);
                        ++count;
                    } else {
                        int base_size = sizeof(array_headers) / sizeof(std::string);
                        for (int base_it = 0; base_it < base_size; ++base_it) {
                            values.push_back("");
                        }

                        if (!scpu.empty()) {
                            values.push_back("");
                        }
                        if (!smem.empty()) {
                            values.push_back("");
                        }
                        values.push_back(svolums);
                    }
                    agents.AddRow(values);
                }

                if (response.agents[i].volums.size() == 0) {
                    values.push_back(::baidu::common::NumToString(count));
                    values.push_back(response.agents[i].endpoint);
                    values.push_back(StringAgentStatus(response.agents[i].status));
                    values.push_back(response.agents[i].pool);
                    values.push_back(tags.c_str());
                    values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                    if (!scpu.empty()) {
                        values.push_back(scpu);
                    }
                    if (!smem.empty()) {
                        values.push_back(smem);
                    }
                    values.push_back("");
                    agents.AddRow(values);
                    ++count;
                }
            }
            
            if (options.size() != 0 && find(options.begin(), options.end(), "volums") == options.end()) {
                values.push_back(::baidu::common::NumToString(count));
                values.push_back(response.agents[i].endpoint);
                values.push_back(StringAgentStatus(response.agents[i].status));
                values.push_back(response.agents[i].pool);
                values.push_back(tags.c_str());
                values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                if (!scpu.empty()) {
                    values.push_back(scpu);
                }
                if (!smem.empty()) {
                    values.push_back(smem);
                }
                agents.AddRow(values);
                ++count;
            }
        }
        printf("%s\n", agents.ToString().c_str());

    } else {
        printf("List agents failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }

    return ret;
}


bool ResAction::ListAgentsByPool(const std::string& pool, const std::string& soptions) {
    if (pool.empty()) {
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }

    std::vector<std::string> options;
    ::baidu::common::SplitString(soptions, ",", &options);


    ::baidu::galaxy::sdk::ListAgentsByPoolRequest request;
    ::baidu::galaxy::sdk::ListAgentsByPoolResponse response;
    request.user = user_;
    request.pool = pool;

    bool ret = resman_->ListAgentsByPool(request, &response);

    if (ret) {
        std::string array_headers[6] = {"", "endpoint", "status", "pool", "tags", "containers"};
        std::vector<std::string> headers(array_headers, array_headers + 6);
        if (find(options.begin(), options.end(), "cpu") != options.end()) {
            headers.push_back("cpu(t/a/u)");
        }
        if (find(options.begin(), options.end(), "mem") != options.end()) {
            headers.push_back("mem(t/a/u)");
        }
        if (find(options.begin(), options.end(), "volums") != options.end()) {
            headers.push_back("vol(med/t/a/u/path)");
        }
        if (options.size() == 0) {
            headers.push_back("cpu(t/a/u)");
            headers.push_back("mem(t/a/u)");
            headers.push_back("vol(med/t/a/u/path)");
        }
        ::baidu::common::TPrinter agents(headers.size());
        agents.AddRow(headers);
        for (uint32_t i = 0; i < response.agents.size(); ++i) {
            std::string tags;
            for (uint32_t j = 0; j < response.agents[i].tags.size(); ++j) {
                tags += response.agents[i].tags[j];
                if (j != response.agents[i].tags.size() - 1) {
                    tags += ",";
                }
            }

            std::string scpu;

            if (options.size() == 0 || find(options.begin(), options.end(), "cpu") != options.end()) {
                scpu = ::baidu::common::NumToString(response.agents[i].cpu.total / 1000.0) + "/" +
                       ::baidu::common::NumToString(response.agents[i].cpu.assigned / 1000.0) + "/" +
                       ::baidu::common::NumToString(response.agents[i].cpu.used / 1000.0);
            }

            std::string smem;
            if (options.size() == 0 || find(options.begin(), options.end(), "mem") != options.end()) {

                smem = HumanReadableString(response.agents[i].memory.total) + "/" +
                       HumanReadableString(response.agents[i].memory.assigned) + "/" +
                       HumanReadableString(response.agents[i].memory.used);
            }
            
            std::vector<std::string> values;
            if (options.size() == 0 || find(options.begin(), options.end(), "volums") != options.end()) {
                for (uint32_t j = 0; j < response.agents[i].volums.size(); ++j) {
                    values.clear();
                    std::string svolums;
                    svolums +=  "vol_" + ::baidu::common::NumToString(j) + " "
                                + StringVolumMedium(response.agents[i].volums[j].medium) + " "
                                + HumanReadableString(response.agents[i].volums[j].volum.total) + "/"
                                + HumanReadableString(response.agents[i].volums[j].volum.assigned) + "/"
                                + HumanReadableString(response.agents[i].volums[j].volum.used) + " "
                                + response.agents[i].volums[j].device_path;
                    if (j == 0) {
                        values.push_back(::baidu::common::NumToString(i));
                        values.push_back(response.agents[i].endpoint);
                        values.push_back(StringAgentStatus(response.agents[i].status));
                        values.push_back(response.agents[i].pool);
                        values.push_back(tags);
                        values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                        if (!scpu.empty()) {
                            values.push_back(scpu);
                        }
                        if (!smem.empty()) {
                            values.push_back(smem);
                        }
                        values.push_back(svolums);
                    } else {
                        int base_size = sizeof(array_headers) / sizeof(std::string);
                        for (int base_it = 0; base_it < base_size; ++base_it) {
                            values.push_back("");
                        }

                        if (!scpu.empty()) {
                            values.push_back("");
                        }
                        if (!smem.empty()) {
                            values.push_back("");
                        }
                        values.push_back(svolums);
                    }
                    agents.AddRow(values);
                }

                if (response.agents[i].volums.size() == 0) {
                    values.push_back(::baidu::common::NumToString(i));
                    values.push_back(response.agents[i].endpoint);
                    values.push_back(StringAgentStatus(response.agents[i].status));
                    values.push_back(response.agents[i].pool);
                    values.push_back(tags.c_str());
                    values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                    if (!scpu.empty()) {
                        values.push_back(scpu);
                    }
                    if (!smem.empty()) {
                        values.push_back(smem);
                    }
                    values.push_back("");
                    agents.AddRow(values);
                }
            }
            
            if (options.size() != 0 && find(options.begin(), options.end(), "volums") == options.end()) {
                values.push_back(::baidu::common::NumToString(i));
                values.push_back(response.agents[i].endpoint);
                values.push_back(StringAgentStatus(response.agents[i].status));
                values.push_back(response.agents[i].pool);
                values.push_back(tags.c_str());
                values.push_back(::baidu::common::NumToString(response.agents[i].total_containers));
                if (!scpu.empty()) {
                    values.push_back(scpu);
                }
                if (!smem.empty()) {
                    values.push_back(smem);
                }
                agents.AddRow(values);
            }
        }
        printf("%s\n", agents.ToString().c_str());

    } else {
        printf("List agents failed for reason %s:%s\n", 
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }

    return ret;
}

bool ResAction::EnterSafeMode() {

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::EnterSafeModeRequest request;
    ::baidu::galaxy::sdk::EnterSafeModeResponse response;
    request.user = user_;

    bool ret = resman_->EnterSafeMode(request, &response);
    if (ret) {
        printf("Enter safemode successfully");
    } else {
        printf("Enter safemode failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::LeaveSafeMode() {
    
    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::LeaveSafeModeRequest request;
    ::baidu::galaxy::sdk::LeaveSafeModeResponse response;
    request.user = user_;

    bool ret = resman_->LeaveSafeMode(request, &response);
    if (ret) {
        printf("Leave safemode successfully");
    } else {
        printf("Leave safemode failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::OnlineAgent(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::OnlineAgentRequest request;
    ::baidu::galaxy::sdk::OnlineAgentResponse response;
    request.user = user_;
    request.endpoint = endpoint; 

    bool ret = resman_->OnlineAgent(request, &response);
    if (ret) {
        printf("Online agent successfully");
    } else {
        printf("Online agent failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;

}
bool ResAction::OfflineAgent(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::OfflineAgentRequest request;
    ::baidu::galaxy::sdk::OfflineAgentResponse response;
    request.user = user_;
    request.endpoint = endpoint; 

    bool ret = resman_->OfflineAgent(request, &response);
    if (ret) {
        printf("Offline agent successfully");
    } else {
        printf("Offline agent failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

struct resource {
        ::baidu::galaxy::sdk::Resource cpu;
        ::baidu::galaxy::sdk::Resource memory;
        std::map< ::baidu::galaxy::sdk::VolumMedium, ::baidu::galaxy::sdk::Resource> volums;
};

bool ResAction::Status() {
    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::StatusRequest request;
    ::baidu::galaxy::sdk::StatusResponse response;
    request.user = user_;
    
    std::string resman_endpoint;
    std::string appmaster_endpoint;
    std::string path = FLAGS_nexus_root + FLAGS_appmaster_path;
    bool ret = resman_->MasterEndpoint(path, &appmaster_endpoint, &resman_endpoint);
    if (!ret) {
        fprintf(stderr, "get master endpoint failed\n");
        return false;
    }
    
    ret = resman_->Status(request, &response);
    if (!ret) {
        printf("Get Status failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
        return false;

    }
    
    ::baidu::galaxy::sdk::ListAgentsRequest list_request;
    ::baidu::galaxy::sdk::ListAgentsResponse list_response;
    list_request.user = user_;

    ret = resman_->ListAgents(list_request, &list_response);
    if (!ret) {
        printf("List Agents failed for reason %s:%s\n",
                StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
        return false;
    }

    printf("master infomation\n");
    baidu::common::TPrinter master(2);
    master.AddRow(2, "master", "addr");
    master.AddRow(2, "appmaster", appmaster_endpoint.c_str());
    master.AddRow(2, "resman", resman_endpoint.c_str());
    printf("%s\n", master.ToString().c_str());

    printf("cluster agent infomation\n");
    ::baidu::common::TPrinter agent(3); 
    agent.AddRow(3, "total", "alive", "dead");
    agent.AddRow(3, baidu::common::NumToString(response.total_agents).c_str(), 
                    baidu::common::NumToString(response.alive_agents).c_str(),
                    baidu::common::NumToString(response.dead_agents).c_str());
    printf("%s\n", agent.ToString().c_str());

    printf("cluster cpu infomation\n");
    ::baidu::common::TPrinter cpu(3);
    cpu.AddRow(3, "total", "assigned", "used");
    cpu.AddRow(3, ::baidu::common::NumToString(response.cpu.total / 1000.0).c_str(), 
                    ::baidu::common::NumToString(response.cpu.assigned / 1000.0).c_str(),
                    ::baidu::common::NumToString(response.cpu.used / 1000.0).c_str());
    printf("%s\n", cpu.ToString().c_str());

    printf("cluster memory infomation\n");
    ::baidu::common::TPrinter mem(3);
    mem.AddRow(3, "total", "assigned", "used");
    mem.AddRow(3, HumanReadableString(response.memory.total).c_str(), 
                  HumanReadableString(response.memory.assigned).c_str(),
                  HumanReadableString(response.memory.used).c_str());
    printf("%s\n", mem.ToString().c_str());

    printf("cluster volumes infomation\n");
    ::baidu::common::TPrinter volum(6);
    volum.AddRow(6, "", "medium", "total", "assigned", "used", "device_path");
    for (uint32_t i = 0; i < response.volum.size(); ++i) {
        volum.AddRow(6, ::baidu::common::NumToString(i).c_str(),  
                        StringVolumMedium(response.volum[i].medium).c_str(), 
                        //::baidu::common::NumToString(response.volum[i].medium).c_str(), 
                        HumanReadableString(response.volum[i].volum.total).c_str(),
                        HumanReadableString(response.volum[i].volum.assigned).c_str(),
                        HumanReadableString(response.volum[i].volum.used).c_str(),
                        response.volum[i].device_path.c_str());
    }
    printf("%s\n", volum.ToString().c_str());
    
    printf("cluster containers infomation\n");
    ::baidu::common::TPrinter other(3);
    other.AddRow(3, "total_groups", "total_containers", "in_safe_mode");
    other.AddRow(3, ::baidu::common::NumToString(response.total_groups).c_str(), 
                    ::baidu::common::NumToString(response.total_containers).c_str(),
                    StringBool(response.in_safe_mode).c_str());
    printf("%s\n", other.ToString().c_str());

    std::map<std::string, resource> resource_stat;

    for (uint32_t i = 0; i < list_response.agents.size(); ++i) {
        std::string temp_pool = list_response.agents[i].pool;

        bool no_repeat = false;
        if (resource_stat.count(temp_pool) == 0) {
            no_repeat = true;
        }

        resource& resource_temp = resource_stat[temp_pool];
        if (no_repeat) {
            //cpu
            resource_temp.cpu.total = 0;
            resource_temp.cpu.assigned = 0;
            resource_temp.cpu.used = 0;
            //mem
            resource_temp.memory.total = 0; 
            resource_temp.memory.assigned = 0;
            resource_temp.memory.used = 0;
        }

        //cpu
        resource_temp.cpu.total += list_response.agents[i].cpu.total;
        resource_temp.cpu.assigned += list_response.agents[i].cpu.assigned;
        resource_temp.cpu.used += list_response.agents[i].cpu.used;
        
        //mem
        resource_temp.memory.total += list_response.agents[i].memory.total;
        resource_temp.memory.assigned += list_response.agents[i].memory.assigned;
        resource_temp.memory.used += list_response.agents[i].memory.used;
        //disk
        for (uint32_t j = 0; j < list_response.agents[i].volums.size(); ++j) {
            ::baidu::galaxy::sdk::VolumMedium media_type = list_response.agents[i].volums[j].medium;
            ::baidu::galaxy::sdk::Resource& volum_temp = resource_temp.volums[media_type];

            if (no_repeat) {
                volum_temp.total = 0;
                volum_temp.assigned = 0;
                volum_temp.used = 0;
            }

            volum_temp.total += list_response.agents[i].volums[j].volum.total;
            volum_temp.assigned += list_response.agents[i].volums[j].volum.assigned;
            volum_temp.used += list_response.agents[i].volums[j].volum.used;
        }

    }

    printf("cluster pools infomation\n");
    ::baidu::common::TPrinter pool(7);
    pool.AddRow(7, "", "name", "total", "alive", "cpu(t/a/u)", "mem(t/a/u)", "vol(t/a/u)");
    for (uint32_t i = 0; i < response.pools.size(); ++i) {
        std::string temp_pool = response.pools[i].name;
        std::string cpu_stat = ::baidu::common::NumToString(resource_stat[temp_pool].cpu.total/1000.0) + "/" +
                               ::baidu::common::NumToString(resource_stat[temp_pool].cpu.assigned/1000.0) + "/" +
                               ::baidu::common::NumToString(resource_stat[temp_pool].cpu.used/1000.0);
        std::string mem_stat = HumanReadableString(resource_stat[temp_pool].memory.total) + "/" +
                               HumanReadableString(resource_stat[temp_pool].memory.assigned) + "/" +
                               HumanReadableString(resource_stat[temp_pool].memory.used);

        std::map< ::baidu::galaxy::sdk::VolumMedium, ::baidu::galaxy::sdk::Resource> ::iterator it
                    = resource_stat[temp_pool].volums.begin();
        for (; it != resource_stat[temp_pool].volums.end(); ++it) {
            const ::baidu::galaxy::sdk::Resource& volum_temp = it->second;
            std::string volum_stat = StringVolumMedium(it->first) + " " +
                                     HumanReadableString(volum_temp.total) + "/" +
                                     HumanReadableString(volum_temp.assigned) + "/" +
                                     HumanReadableString(volum_temp.used);
            if (it == resource_stat[temp_pool].volums.begin()) {
                pool.AddRow(7, ::baidu::common::NumToString(i).c_str(),
                               response.pools[i].name.c_str(), 
                               ::baidu::common::NumToString(response.pools[i].total_agents).c_str(),
                               ::baidu::common::NumToString(response.pools[i].alive_agents).c_str(),
                               cpu_stat.c_str(),
                               mem_stat.c_str(),
                               volum_stat.c_str());
            } else {
                pool.AddRow(7, "",
                               "", 
                               "",
                               "",
                               "",
                               "",
                               volum_stat.c_str());

            }
                                        
        }

        if (resource_stat[temp_pool].volums.size() == 0) {

            pool.AddRow(7, ::baidu::common::NumToString(i).c_str(),
                           response.pools[i].name.c_str(), 
                           ::baidu::common::NumToString(response.pools[i].total_agents).c_str(),
                           ::baidu::common::NumToString(response.pools[i].alive_agents).c_str(),
                           cpu_stat.c_str(),
                           mem_stat.c_str(),
                           "");
                        
        }
    }
    printf("%s\n", pool.ToString().c_str());
    return true;

}

bool ResAction::CreateTag(const std::string& tag, const std::string& file) {

    if (tag.empty()) {
        return false;
    }

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::CreateTagRequest request;
    ::baidu::galaxy::sdk::CreateTagResponse response;
    request.user = user_;
    request.tag = tag;

    if (!LoadAgentEndpointsFromFile(file, &request.endpoint)) {
        printf("load endpoint file failed\n");
        return false;
    }

    bool ret = resman_->CreateTag(request, &response);
    if (ret) {
        printf("Create tag successfully\n");
    } else {
        printf("Create tag failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;

}

bool ResAction::ListTags() {

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::ListTagsRequest request;
    ::baidu::galaxy::sdk::ListTagsResponse response;
    request.user = user_;

    bool ret = resman_->ListTags(request, &response);
    if (ret) {
        ::baidu::common::TPrinter tags(2);
        tags.AddRow(2, "", "tag");
        for (uint32_t i = 0; i < response.tags.size(); ++i) {
            tags.AddRow(2, ::baidu::common::NumToString(i).c_str(),
                           response.tags[i].c_str()
                       );
        }
        printf("%s\n", tags.ToString().c_str());

    } else {
        printf("List tags failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::GetPoolByAgent(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::GetPoolByAgentRequest request;
    ::baidu::galaxy::sdk::GetPoolByAgentResponse response;
    request.user = user_;
    request.endpoint = endpoint;

    bool ret = resman_->GetPoolByAgent(request, &response);
    if (ret) {
        printf("%s Pool is %s\n", endpoint.c_str(), response.pool.c_str());
    } else {
        printf("Get Pool failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;

}

bool ResAction::AddUser(const std::string& user, const std::string& token) {
    if (user.empty() || token.empty()) {
        return false;
    }
    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::AddUserRequest request;
    ::baidu::galaxy::sdk::AddUserResponse response;
    request.admin = user_;
    request.user.user  = user;
    request.user.token = token;

    bool ret = resman_->AddUser(request, &response);
    if (ret) {
        printf("Add User Success\n");
    } else {
        printf("Add User failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;

}

bool ResAction::RemoveUser(const std::string& user) {
    if (user.empty()) {
        fprintf(stderr, "user is needed\n");
        return false;
    }
    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::RemoveUserRequest request;
    ::baidu::galaxy::sdk::RemoveUserResponse response;
    request.admin = user_;
    request.user.user  = user;

    bool ret = resman_->RemoveUser(request, &response);
    if (ret) {
        printf("Remove User Success\n");
    } else {
        printf("Remove User failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::ListUsers() {
    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::ListUsersRequest request;
    ::baidu::galaxy::sdk::ListUsersResponse response;
    request.user = user_;

    bool ret = resman_->ListUsers(request, &response);
    
    if (ret) {
        ::baidu::common::TPrinter users(2);
        users.AddRow(2, "", "user");
        for (uint32_t i = 0; i < response.user.size(); ++i) {
            users.AddRow(2, ::baidu::common::NumToString(i).c_str(),
                            response.user[i].c_str()
                        );
        }
        printf("%s\n", users.ToString().c_str());

    } else {
        printf("List users failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::ShowUser(const std::string& user) {
    if (user.empty()) {
        return false;
    }

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::ShowUserRequest request;
    ::baidu::galaxy::sdk::ShowUserResponse response;
    request.admin = user_;
    request.user.user = user;
    request.user.token = "default";

    bool ret = resman_->ShowUser(request, &response);
    
    if (ret) {
        printf("authority infomation\n");
        ::baidu::common::TPrinter grants(3);
        grants.AddRow(3, "", "pool", "authority");
        for (uint32_t i = 0; i < response.grants.size(); ++i) {
            for (uint32_t j = 0; j < response.grants[i].authority.size(); ++j) {
                if (j == 0) {
                    grants.AddRow(3, ::baidu::common::NumToString(i).c_str(),
                                    response.grants[i].pool.c_str(),
                                    StringAuthority(response.grants[i].authority[j]).c_str()
                                );
                } else {
                    grants.AddRow(3, "",
                                    "",
                                    StringAuthority(response.grants[i].authority[j]).c_str()
                                );
                }
            }
            if (response.grants[i].authority.size() == 0) {
                grants.AddRow(3, ::baidu::common::NumToString(i).c_str(),
                                response.grants[i].pool.c_str(),
                                ""
                            );
            }
        }
        printf("%s\n", grants.ToString().c_str());

        printf("quota infomation\n");
        ::baidu::common::TPrinter quota(5);
        quota.AddRow(5, "cpu", "memory", "disk", "ssd", "replica");
        quota.AddRow(5, ::baidu::common::NumToString(response.quota.millicore / 1000.0).c_str(),
                        HumanReadableString(response.quota.memory).c_str(),
                        HumanReadableString(response.quota.disk).c_str(),
                        HumanReadableString(response.quota.ssd).c_str(),
                        ::baidu::common::NumToString(response.quota.replica).c_str()
                    );
        printf("%s\n", quota.ToString().c_str());

        printf("jobs assigned quota infomation\n");
        ::baidu::common::TPrinter assign(5);
        assign.AddRow(5, "cpu", "memory", "disk", "ssd", "replica");
        assign.AddRow(5, ::baidu::common::NumToString(response.assigned.millicore / 1000.0).c_str(),
                        HumanReadableString(response.assigned.memory).c_str(),
                        HumanReadableString(response.assigned.disk).c_str(),
                        HumanReadableString(response.assigned.ssd).c_str(),
                        ::baidu::common::NumToString(response.assigned.replica).c_str()
                    );
        printf("%s\n", assign.ToString().c_str());

    } else {
        printf("Show users failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::GrantUser(const std::string& user, 
                          const std::string& pool, 
                          const std::string& opration,
                          const std::string& authority) {
    
    if (user.empty() || pool.empty()) {
        fprintf(stderr, "user and pool is needed\n");
        return false;
    }

    if(!this->Init()) {
        return false;
    }
        
    ::baidu::galaxy::sdk::Grant grant;

    if (opration.compare("add") == 0) {
        grant.action = ::baidu::galaxy::sdk::kActionAdd; 
    } else if (opration.compare("remove") == 0){
        grant.action = ::baidu::galaxy::sdk::kActionRemove;
    } else if (opration.compare("set") == 0) { 
        grant.action = ::baidu::galaxy::sdk::kActionSet;
    } else if (opration.compare("clear") == 0) {
        grant.action = ::baidu::galaxy::sdk::kActionClear;
    } else {
        fprintf(stderr, "action must be in [add|remove|set|clear]\n");
        return false;
    }

    std::vector<std::string> authorities;
    ::baidu::common::SplitString(authority, ",", &authorities);

    for (size_t i = 0; i < authorities.size(); ++i) {
        ::baidu::galaxy::sdk::Authority kAuthority;
        
        if (authorities[i].compare("create_container") == 0) {
            kAuthority = ::baidu::galaxy::sdk::kAuthorityCreateContainer;
        } else if (authorities[i].compare("remove_container") == 0) {
            kAuthority = ::baidu::galaxy::sdk::kAuthorityRemoveContainer;
        } else if (authorities[i].compare("update_container") == 0) {
            kAuthority = ::baidu::galaxy::sdk::kAuthorityUpdateContainer;
        } else if (authorities[i].compare("list_containers") == 0) {
            kAuthority = ::baidu::galaxy::sdk::kAuthorityListContainer;
        }else if (authorities[i].compare("submit_job") == 0) {
            kAuthority = ::baidu::galaxy::sdk::kAuthoritySubmitJob;
        }else if (authorities[i].compare("remove_job") == 0) {
            kAuthority = ::baidu::galaxy::sdk::kAuthorityRemoveJob;
        }else if (authorities[i].compare("update_job") == 0) {
            kAuthority = ::baidu::galaxy::sdk::kAuthorityUpdateJob;
        }else if (authorities[i].compare("list_jobs") == 0) {
            kAuthority = ::baidu::galaxy::sdk::kAuthorityListJobs;
        }else {
            return false;
        } 
        grant.authority.push_back(kAuthority);
    }
    grant.pool = pool;

    ::baidu::galaxy::sdk::GrantUserRequest request;
    ::baidu::galaxy::sdk::GrantUserResponse response;
    request.admin = user_;
    request.user.user  = user;
    request.grant = grant;

    bool ret = resman_->GrantUser(request, &response);
    if (ret) {
        printf("Grant User Success\n");
    } else {
        printf("Grant User failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::AssignQuota(const std::string& user,
                 uint32_t millicores,
                 const std::string& memory,
                 const std::string& disk,
                 const std::string& ssd,
                 uint32_t replica
                 ) {

    if (user.empty()) {
        return false;
    }
    if(!this->Init()) {
        return false;
    }
    
    if (millicores <= 0 || replica <= 0) {
        printf("millicores and replica must be larger than 0\n");
        return false;
    }

    ::baidu::galaxy::sdk::Quota quota;
    quota.millicore = millicores;
    quota.replica = replica;
    if (UnitStringToByte(memory, &quota.memory) != 0) {
        return false;
    }

    if (UnitStringToByte(disk, &quota.disk) != 0) {
        return false;
    }

    if (UnitStringToByte(ssd, &quota.ssd) != 0) {
        return false;
    }

    ::baidu::galaxy::sdk::AssignQuotaRequest request;
    ::baidu::galaxy::sdk::AssignQuotaResponse response;
    request.admin = user_;
    request.user.user = user;
    request.quota = quota;

    bool ret = resman_->AssignQuota(request, &response);
    if (ret) {
        printf("Assign quota Success\n");
    } else {
        printf("Assign quota failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
} 

bool ResAction::Preempt(const std::string& container_group_id, const std::string& endpoint) {

    if (container_group_id.empty() || endpoint.empty()) {
        fprintf(stderr, "container_group_id and endpoint are needed\n");
        return false;
    }
    
    if(!this->Init()) {
        return false;
    }

    baidu::galaxy::sdk::PreemptRequest request;
    baidu::galaxy::sdk::PreemptResponse response;
    request.user = user_;
    request.container_group_id = container_group_id;
    request.endpoint = endpoint;
    bool ret = resman_->Preempt(request, &response);
    if (ret) {
        printf("Preempt %s\n success", container_group_id.c_str());
    } else {
        printf("Preempt %s failed for reason %s:%s\n", container_group_id.c_str(), 
                StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::GetTagsByAgent(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::GetTagsByAgentRequest request;
    ::baidu::galaxy::sdk::GetTagsByAgentResponse response;
    request.user = user_;
    request.endpoint = endpoint;

    bool ret = resman_->GetTagsByAgent(request, &response);
    if (ret) {
        ::baidu::common::TPrinter tags(2);
        tags.AddRow(2, "", "tag");
        for (uint32_t i = 0; i < response.tags.size(); ++i) {
            tags.AddRow(2, ::baidu::common::NumToString(i).c_str(),
                           response.tags[i].c_str()
                       );
        }
        printf("%s\n", tags.ToString().c_str());
    } else {
        printf("Get Tags failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::AddAgentToPool(const std::string& endpoint, const std::string& pool) {
    if (endpoint.empty() || pool.empty()) {
        return false;
    }

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::AddAgentToPoolRequest request;
    ::baidu::galaxy::sdk::AddAgentToPoolResponse response;
    request.user = user_;
    request.endpoint = endpoint;
    request.pool =  pool;

    bool ret = resman_->AddAgentToPool(request, &response);
    if (ret) {
        printf("Set agent %s to pool %s successfully\n", endpoint.c_str(), pool.c_str());
    } else {
        printf("Set agent failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

bool ResAction::RemoveAgentFromPool(const std::string& endpoint, const std::string& pool) {
    if (endpoint.empty() || pool.empty()) {
        return false;
    }

    if(!this->Init()) {
        return false;
    }

    ::baidu::galaxy::sdk::RemoveAgentFromPoolRequest request;
    ::baidu::galaxy::sdk::RemoveAgentFromPoolResponse response;
    request.user = user_;
    request.endpoint = endpoint;
    //request.pool = pool;

    bool ret = resman_->RemoveAgentFromPool(request, &response);
    if (ret) {
        printf("Remove agent %s from pool %s successfully\n", endpoint.c_str(), pool.c_str());
    } else {
        printf("Remove agent failed for reason %s:%s\n",
                    StringStatus(response.error_code.status).c_str(), response.error_code.reason.c_str());
    }
    return ret;
}

} // end namespace client
} // end namespace galaxy
} // end namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
