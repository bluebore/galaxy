// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <map>
#include <boost/algorithm/string.hpp>
#include "rapidjson/error/en.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "galaxy_util.h"

namespace baidu {
namespace galaxy {
namespace client {

int ParseDeploy(const rapidjson::Value& deploy_json, ::baidu::galaxy::sdk::Deploy* deploy) {
    //deploy config:replica
    if (!deploy_json.HasMember("replica") || deploy_json["replica"].GetInt() < 0) {
        fprintf(stderr, "replica is required in deploy\n");
        return -1;
    }
    deploy->replica = deploy_json["replica"].GetInt();

    //deploy config:step
    if (!deploy_json.HasMember("step") || deploy_json["step"].GetInt() < 0) {
        fprintf(stderr, "step is required in deploy\n");
        return -1;
    }
    deploy->step = deploy_json["step"].GetInt();

    //deploy config:interval
    if (!deploy_json.HasMember("interval") || deploy_json["interval"].GetInt() < 0) {
        fprintf(stderr, "interval is required in deploy\n");
        return -1;
    }
    deploy->interval = deploy_json["interval"].GetInt();

    //deploy config:max_per_host
    if (!deploy_json.HasMember("max_per_host") || deploy_json["max_per_host"].GetInt() <= 0) {
        fprintf(stderr, "max_per_host is required in deploy\n");
        return -1;
    }
    deploy->max_per_host = deploy_json["max_per_host"].GetInt();
    
    //deploy config:tag
    if (deploy_json.HasMember("tag")) {
        deploy->tag = deploy_json["tag"].GetString();
        boost::trim(deploy->tag);
    }

    if (!deploy_json.HasMember("pools")) {
        fprintf(stderr, "pools is needed in deploy\n");
        return -1;
    }
    std::string str_pools = deploy_json["pools"].GetString();
    boost::trim(str_pools);

    std::vector<std::string> pools;
    ::baidu::common::SplitString(str_pools, ",", &pools);
    if (pools.size() == 0) {
        fprintf(stderr, "pools are needed in deploy\n");
        return -1;
    }
    deploy->pools.assign(pools.begin(), pools.end());
    return 0;
}

int ParseVolum(const rapidjson::Value& volum_json, ::baidu::galaxy::sdk::VolumRequired* volum) {
    int ok = 0;
    //size
    if (!volum_json.HasMember("size")) {
        fprintf(stderr, "size is required in volum\n");
        return -1;
    }
    
    ok = UnitStringToByte(volum_json["size"].GetString(), &volum->size);
    if (ok != 0) {
        return -1;
    }

    //type
    if (!volum_json.HasMember("type")) {
        volum->type = ::baidu::galaxy::sdk::kEmptyDir;
    } else {
        std::string type = volum_json["type"].GetString();
        boost::trim(type);
        if (type.compare("kEmptyDir") == 0) {
            volum->type = ::baidu::galaxy::sdk::kEmptyDir;
        } else if (type.compare("kHostDir") == 0) {
            volum->type = ::baidu::galaxy::sdk::kHostDir;
        } else {
            fprintf(stderr, "type must be [kEmptyDir, kHostDir] in volum\n");
            return -1;
        }
    }

    //medium
    if (!volum_json.HasMember("medium")) {
        //volum->medium = ::baidu::galaxy::sdk::kDisk;
        fprintf(stderr, "medium is needed in volum");
        return -1;
    } else {
        std::string medium = volum_json["medium"].GetString();
        boost::trim(medium);
        if (medium.compare("kSsd") == 0) {
            volum->medium = ::baidu::galaxy::sdk::kSsd;
        } else if (medium.compare("kDisk") == 0) {
            volum->medium = ::baidu::galaxy::sdk::kDisk;
        } else if (medium.compare("kBfs") == 0) {
            volum->medium = ::baidu::galaxy::sdk::kBfs;
        } else if (medium.compare("kTmpfs") == 0) {
            volum->medium = ::baidu::galaxy::sdk::kTmpfs;
        } else {
            fprintf(stderr, "medium  must be [kSsd, kDisk, kBfs, kTmpfs] in volum\n");
            return -1;
        } 
    }

    //source_path
    /*if (!volum_json.HasMember("source_path")) {
        fprintf(stderr, "source_path is needed in volum");
        return -1;
    }
    volum->source_path = volum_json["source_path"].GetString();*/
    
    //dest_path
    if (!volum_json.HasMember("dest_path")) {
        fprintf(stderr, "dest_path is required in volum\n");
        return -1;
    }
    volum->dest_path = volum_json["dest_path"].GetString();
    boost::trim(volum->dest_path);
    if (volum->dest_path.empty()) {
        fprintf(stderr, "dest_path cannot be empty in volum\n");
        return -1;
    }

    //readonly
    if (!volum_json.HasMember("readonly")) {
        volum->readonly = false;
    } else {
        volum->readonly = volum_json["readonly"].GetBool();
    }

    //exclusive
    if (!volum_json.HasMember("exclusive")) {
        volum->exclusive = false;
    } else {
        volum->exclusive = volum_json["exclusive"].GetBool();
    }

    //use_symlink
    if (!volum_json.HasMember("use_symlink")) {
        volum->use_symlink = false;;
    } else {
        volum->use_symlink = volum_json["use_symlink"].GetBool();
    }

    return 0;

}

int ParseCpu(const rapidjson::Value& cpu_json, ::baidu::galaxy::sdk::CpuRequired* cpu) {
    if (!cpu_json.HasMember("millicores")) {
        fprintf(stderr, "milli_cores is required in cpu\n");
        return -1;
    }
    cpu->milli_core = cpu_json["millicores"].GetInt();

    if (!cpu_json.HasMember("excess")) {
        cpu->excess = false;
    } else {
        cpu->excess = cpu_json["excess"].GetBool();
    }

    return 0;

}

int ParseMem(const rapidjson::Value& mem_json, ::baidu::galaxy::sdk::MemoryRequired* mem) {
    int ok = 0;
    if (!mem_json.HasMember("size")) {
        fprintf(stderr, "size is required in mem\n");
        return -1;
    }
    
    ok = UnitStringToByte(mem_json["size"].GetString(), &mem->size);
    if (ok != 0) {
        return -1;
    }

    if (!mem_json.HasMember("excess")) {
        mem->excess = false;
    } else {
        mem->excess = mem_json["excess"].GetBool();
    }

    return 0;

}

int ParsePackage(const rapidjson::Value& pack_json, ::baidu::galaxy::sdk::Package* pack) {
    if (!pack_json.HasMember("source_path")) {
        fprintf(stderr, "source_path is required in package\n");
        return -1;
    }
    pack->source_path = pack_json["source_path"].GetString();
    boost::trim(pack->source_path);

    if (!pack_json.HasMember("dest_path")) {
        fprintf(stderr, "dest_path is required in package\n");
        return -1;
    }
    pack->dest_path = pack_json["dest_path"].GetString();
    boost::trim(pack->dest_path);

    if (!pack_json.HasMember("version")) {
        fprintf(stderr, "version is required in package\n");
        return -1;
    }
    pack->version = pack_json["version"].GetString();
    boost::trim(pack->version);

    return 0;

}

int ParseImagePackage(const rapidjson::Value& image_json, ::baidu::galaxy::sdk::ImagePackage* image) {
    if (!image_json.HasMember("start_cmd")) {
        fprintf(stderr, "start_cmd is required in exec_package\n");
        return -1;
    }
    image->start_cmd = image_json["start_cmd"].GetString();
    boost::trim(image->start_cmd);
    
    if (image_json.HasMember("stop_cmd")) {
        image->stop_cmd = image_json["stop_cmd"].GetString();
        boost::trim(image->stop_cmd);
    }

    if (image_json.HasMember("health_cmd")) {
        image->health_cmd = image_json["health_cmd"].GetString();
        boost::trim(image->health_cmd);
    }
    
    if (!image_json.HasMember("package")) {
        fprintf(stderr, "package is required in exec_package\n");
        return -1;
    }
    const rapidjson::Value& pack_json = image_json["package"];
    ::baidu::galaxy::sdk::Package& pack = image->package;
    return ParsePackage(pack_json, &pack);
}

int ParseDataPackage(const rapidjson::Value& data_json, ::baidu::galaxy::sdk::DataPackage* data) {
    int ok = 0;
    if (!data_json.HasMember("reload_cmd")) {
        fprintf(stderr, "reload_cmd is required in data_package\n");
        return -1;
    }
    data->reload_cmd = data_json["reload_cmd"].GetString();
    boost::trim(data->reload_cmd);
    if (!data_json.HasMember("packages")) {
        fprintf(stderr, "packages is required in data_package\n");
        return -1;
    }
    
    const rapidjson::Value& packages_json = data_json["packages"];
    if (packages_json.Size() <= 0) {
        fprintf(stderr, "size of packages is zero in data_package\n");
        return -1;
    }

    std::vector< ::baidu::galaxy::sdk::Package>& packs = data->packages;
    for (rapidjson::SizeType i = 0; i < packages_json.Size(); ++i) {
        ::baidu::galaxy::sdk::Package pack;
        ok = ParsePackage(packages_json[i], &pack);
        if (ok != 0) {
            break;
        }
        packs.push_back(pack);
    }

    if (ok != 0) {
        fprintf(stderr, "packages is error in data_package\n");
        return -1;
    }

    return 0;
}

int ParsePort(const rapidjson::Value& port_json, ::baidu::galaxy::sdk::PortRequired* port) {
    if (!port_json.HasMember("name")) {
        fprintf(stderr, "port_name is required in port\n");
        return -1;
    }
    port->port_name = port_json["name"].GetString();
    boost::trim(port->port_name);
    
    if (!port_json.HasMember("port")) {
        fprintf(stderr, "port is required in port\n");
        return -1;
    }
    port->port = port_json["port"].GetString();
    boost::trim(port->port);
    return 0;

}

int ParseService(const rapidjson::Value& service_json, ::baidu::galaxy::sdk::Service* service) {
    if (!service_json.HasMember("service_name")) {
        fprintf(stderr, "service_name is needed in service\n");
        return -1;
    }
    service->service_name = service_json["service_name"].GetString();
    boost::trim(service->service_name);

    /*if (!service_json.HasMember("port_name")) {
        fprintf(stderr, "port_name is needed in service\n");
        return -1;
    }
    service->port_name = service_json["port_name"].GetString();
    boost::trim(service->port_name);*/

    if (service_json.HasMember("port_name")) {
        service->port_name = service_json["port_name"].GetString();
        boost::trim(service->port_name);
    }

    if (!service_json.HasMember("use_bns")) {
        service->use_bns = false;
    } else {
        service->use_bns = service_json["use_bns"].GetBool();
    }

    if (service_json.HasMember("tag")) {
        service->tag = service_json["tag"].GetString();
    }
    
    if (service_json.HasMember("health_check_type")) {
        service->health_check_type = service_json["health_check_type"].GetString();
    }

    if (service_json.HasMember("health_check_script")) {
        service->health_check_script = service_json["health_check_script"].GetString();
    }
    
    if (service_json.HasMember("token")) {
        service->token = service_json["token"].GetString();
    }

    return 0;

}

int ParseTcpthrot(const rapidjson::Value& tcp_json, ::baidu::galaxy::sdk::TcpthrotRequired* tcp) {
    if (!tcp_json.HasMember("recv_bps_quota")) {
        fprintf(stderr, "recv_bps_quota is needed in tcpthrot\n");
        return -1;
    }

    int ok = UnitStringToByte(tcp_json["recv_bps_quota"].GetString(), &tcp->recv_bps_quota);
    if (ok != 0) {
        return -1;
    }
    
    if (!tcp_json.HasMember("send_bps_quota")) {
        fprintf(stderr, "send_bps_quota is needed in tcpthrot\n");
        return -1;
    }

    ok = UnitStringToByte(tcp_json["send_bps_quota"].GetString(), &tcp->send_bps_quota);
    if (ok != 0) {
        return -1;
    }

    if (!tcp_json.HasMember("recv_bps_excess")) {
        tcp->recv_bps_excess = false;
    } else {
        tcp->recv_bps_excess = tcp_json["recv_bps_excess"].GetBool();
    }

    if (!tcp_json.HasMember("send_bps_excess")) {
        tcp->send_bps_excess = false;
    } else {
        tcp->send_bps_excess = tcp_json["send_bps_excess"].GetBool();
    }

    return 0;
}

int ParseBlkio(const rapidjson::Value& blkio_json, ::baidu::galaxy::sdk::BlkioRequired* blkio) {
    if (!blkio_json.HasMember("weight")) {
        fprintf(stderr, "weight is needed in blkio\n");
        return -1;
    }
    blkio->weight = blkio_json["weight"].GetInt();
    return 0;
}

int ParseTask(const rapidjson::Value& task_json, ::baidu::galaxy::sdk::TaskDescription* task) {
    int ok = 0;

    time_t timestamp;
    time(&timestamp);
    //task->id = baidu::common::NumToString(timestamp);

    if (!task_json.HasMember("cpu")) {
        fprintf(stderr, "cpu is required in task\n");
        return -1;
    }
    ::baidu::galaxy::sdk::CpuRequired& cpu = task->cpu;
    ok = ParseCpu(task_json["cpu"], &cpu);
    if (ok != 0) {
        fprintf(stderr, "cpu is errror in task cpu\n");
        return -1;
    }

    if (!task_json.HasMember("mem")) {
        fprintf(stderr, "mem is required in task mem\n");
        return -1;
    }

    ::baidu::galaxy::sdk::MemoryRequired& mem = task->memory;
    ok = ParseMem(task_json["mem"], &mem);
    if (ok != 0) {
        fprintf(stderr, "mem is errror in task mem\n");
        return -1;
    }

    if (!task_json.HasMember("tcp")) {
        fprintf(stderr, "tcp is required in task tcp\n");
        return -1;
    }

    ::baidu::galaxy::sdk::TcpthrotRequired& tcp = task->tcp_throt;
    ok = ParseTcpthrot(task_json["tcp"], &tcp);
    if (ok != 0) {
        fprintf(stderr, "tcp is errror in task tcp\n");
        return -1;
    }

    if (!task_json.HasMember("blkio")) {
        fprintf(stderr, "blkio is required in task blkio\n");
        return -1;
    }

    ::baidu::galaxy::sdk::BlkioRequired& blkio = task->blkio;
    ok = ParseBlkio(task_json["blkio"], &blkio);
    if (ok != 0) {
        fprintf(stderr, "blkio is errror in task blkio\n");
        return -1;
    }
    
    std::vector< ::baidu::galaxy::sdk::PortRequired>& ports = task->ports;
    if (task_json.HasMember("ports")) {
        const rapidjson::Value& ports_json = task_json["ports"];
        std::vector<std::string> vec_ports; //端口连续性校验
        for (rapidjson::SizeType i = 0; i < ports_json.Size(); ++i) {
            ::baidu::galaxy::sdk::PortRequired port;
            ok = ParsePort(ports_json[i], &port);
            if (ok != 0) {
                break;
            }
            vec_ports.push_back(port.port);
            ports.push_back(port);
        }

        if (ok != 0) {
            return -1;
        }

        for (size_t i = 1; i < vec_ports.size(); ++i) {
            if ((vec_ports[i].compare("dynamic") != 0 && vec_ports[i].compare(vec_ports[i-1]) == 0)
                    || (vec_ports[i-1].compare("dynamic") == 0 
                        && vec_ports[i].compare("dynamic") != 0)) {
                fprintf(stderr, "ports are not correct in task, ports must be serial\n");
                return -1;
            }

           if (vec_ports[i-1].compare("dynamic") != 0 && vec_ports[i].compare("dynamic") != 0) {
               int int_port = atoi(vec_ports[i-1].c_str());
               if (int_port == 0) {
                   fprintf(stderr, "port %s is error\n", vec_ports[i-1].c_str());
                   return -1;
               }
               ++int_port;
               if (vec_ports[i].compare(::baidu::common::NumToString(int_port)) != 0) {
                   fprintf(stderr, "ports are not correct in task, ports must be serial\n");
                   return -1;
               }
           }
        }
    }

    if (!task_json.HasMember("exec_package")) {
        fprintf(stderr, "exec_package is needed in task\n");
        return -1;
    }

    ::baidu::galaxy::sdk::ImagePackage& exec_package = task->exe_package;
    ok = ParseImagePackage(task_json["exec_package"], &exec_package);
    if (ok != 0) {
        fprintf(stderr, "data_package is needed in task\n");
        return -1;
    }

    if (task_json.HasMember("data_package")) {
        ::baidu::galaxy::sdk::DataPackage& data_package = task->data_package;
        ok = ParseDataPackage(task_json["data_package"], &data_package);
        if (ok != 0) {
            fprintf(stderr, "data_package is needed in task\n");
            return -1;
        }
    }

    std::vector< ::baidu::galaxy::sdk::Service>& services = task->services;
    
    if (task_json.HasMember("services")) {
        const rapidjson::Value& servers_json = task_json["services"];
        for(rapidjson::SizeType i = 0; i < servers_json.Size(); ++i) {
            ::baidu::galaxy::sdk::Service service;
            ok = ParseService(servers_json[i], &service);
            if (ok != 0) {
                break;
            }
            services.push_back(service);
        }
    
        if (ok != 0) {
            return -1;
        }
    } 

    return 0;
}

int ParsePod(const rapidjson::Value& pod_json, ::baidu::galaxy::sdk::PodDescription* pod, bool jump_task = false) {
    int ok = 0;
    if (!pod_json.HasMember("workspace_volum")) {
        fprintf(stderr, "workspace_volum is required in pod\n");
        return -1;
    }

    ::baidu::galaxy::sdk::VolumRequired& workspace_volum =pod->workspace_volum;
    const rapidjson::Value& pod_workspace_volum_json = pod_json["workspace_volum"];

    std::vector<std::string> vec_des_path;
    ok = ParseVolum(pod_workspace_volum_json, &workspace_volum);
    if (ok != 0) {
        return -1;
    }
    
    vec_des_path.push_back(workspace_volum.dest_path);

    std::vector< ::baidu::galaxy::sdk::VolumRequired>& data_volums = pod->data_volums;
    if (pod_json.HasMember("data_volums")) {
        const rapidjson::Value& data_volums_json = pod_json["data_volums"];
        for(rapidjson::SizeType i = 0; i < data_volums_json.Size(); ++i) {
            ::baidu::galaxy::sdk::VolumRequired data_volum;
            ok = ParseVolum(data_volums_json[i], &data_volum);
            if (ok != 0) {
                break;
            }
            //重复值检测
            std::vector<std::string> ::iterator it = find(vec_des_path.begin(), vec_des_path.end(), data_volum.dest_path);
            if (it != vec_des_path.end()) {
                ok = -1;
                fprintf(stderr, "dest_path in volums cannot be repeated\n");
                break;
            }
            vec_des_path.push_back(data_volum.dest_path);
            data_volums.push_back(data_volum);
        }
    }

    if (ok != 0) {
        return -1;
    }

    if (jump_task) {
        return 0;
    }

    std::vector< ::baidu::galaxy::sdk::TaskDescription>& tasks = pod->tasks;
    if (!pod_json.HasMember("tasks")) {
        fprintf(stderr, "tasks is required in pod\n");
        return -1;
    }

    const rapidjson::Value& tasks_json = pod_json["tasks"];
    if (tasks_json.Size() == 0) {
        fprintf(stderr, "size of tasks must be larger than 0 in pod\n");
        return -1;
    }

    for (rapidjson::SizeType i = 0; i < tasks_json.Size(); ++i) {
        ::baidu::galaxy::sdk::TaskDescription task;
        ok = ParseTask(tasks_json[i], &task);
        if (ok != 0) {
            break;
        }
        tasks.push_back(task);
    }

    if (ok != 0) {
        //fprintf(stderr, "task[%d] is error in pod\n", i + 1);
        return -1;
    }

    return 0;

}

int ParseDocument(const rapidjson::Document& doc, ::baidu::galaxy::sdk::JobDescription* job, bool jump_task = false) {
    int ok = 0;
    //name
    if (!doc.HasMember("name")) {
        fprintf(stderr, "name is required in config\n");
        return -1;
    }
    job->name = doc["name"].GetString();
    boost::trim(job->name);

    //type
    if (!doc.HasMember("type")) {
        fprintf(stderr, "type is required in config\n");
        return -1;
    }
    
    std::string type = doc["type"].GetString();
    boost::trim(type);
    if (type.compare("kJobMonitor") == 0) {
        job->type = ::baidu::galaxy::sdk::kJobMonitor;
    }else if (type.compare("kJobService") == 0) {
        job->type = ::baidu::galaxy::sdk::kJobService;
    } else if (type.compare("kJobBatch") == 0) {
        job->type = ::baidu::galaxy::sdk::kJobBatch;
    } else if (type.compare("kJobBestEffort") == 0) {
        job->type = ::baidu::galaxy::sdk::kJobBestEffort;
    } else {
        fprintf(stderr, "type is error, it must be [kJobMonitor,kJobService,kJobBatch,kJobBestEffort]\n");
        return -1;
    }

    //version
    /*if (!doc.HasMember("version")) {
        fprintf(stderr, "version is required in config\n");
        return -1;
    }
    job->version = doc["version"].GetString();
    boost::trim(job->version);*/

    //run_user
    /*if (!doc.HasMember("run_user")) {
        fprintf(stderr, "run_user is required in config\n");
        return -1;
    }
    job->run_user = doc["run_user"].GetString();
    boost::trim(job->run_user);*/
    job->run_user = "galaxy";

    if (doc.HasMember("volum_jobs")) {
        std::string str_jobs = doc["volum_jobs"].GetString();
        boost::trim(str_jobs);

        std::vector<std::string> volum_jobs;
        ::baidu::common::SplitString(str_jobs, ",", &volum_jobs);
        if (volum_jobs.size() == 0) {
            fprintf(stderr, "volum_jobs are needed in config, if you don't want this item, please wipe off\n");
            return -1;
        }   
    job->volum_jobs.assign(volum_jobs.begin(), volum_jobs.end());
    }

    ::baidu::galaxy::sdk::Deploy& deploy = job->deploy;

    //deploy
    if (!doc.HasMember("deploy")) {
        fprintf(stderr, "deploy is required in config\n");
        return -1;
    }
    const rapidjson::Value& deploy_json = doc["deploy"];

    ok = ParseDeploy(deploy_json, &deploy);
    if (ok != 0) {
        fprintf(stderr, "deploy config error\n");
        return -1;
    }
    
    //pod
    if (!doc.HasMember("pod")) {
        fprintf(stderr, "pod is required in config\n");
        return -1;
    }
    const rapidjson::Value& pod_json = doc["pod"];

    ::baidu::galaxy::sdk::PodDescription& pod = job->pod;
    
    ok = ParsePod(pod_json, &pod, jump_task);
    return ok;
}

int BuildJobFromConfig(const std::string& conf, ::baidu::galaxy::sdk::JobDescription* job, bool jump_task) {
    FILE *fd = fopen(conf.c_str(), "r");
    if (fd == NULL) {
        fprintf(stderr, "\nopen file [%s] error: value [%d]-->[%s]\n\n", conf.c_str(), errno, strerror(errno));
        return -1;
    }
    char buf[5120];
    rapidjson::FileReadStream frs(fd, buf, sizeof(buf));
    rapidjson::Document doc;
    doc.ParseStream<0>(frs);
    
    if (!doc.IsObject()) {
        fprintf(stderr, "\ninvalid config file, [%s] is not a correct json format file\n", conf.c_str());
        if (doc.HasParseError()) {
            int line = GetLineNumber(fd, doc.GetErrorOffset());
            fprintf(stderr, "\n\n[%s] error: %s\n", conf.c_str(), rapidjson::GetParseError_En(doc.GetParseError()));
            fprintf(stderr, "at overview offset [%zu], at line number [%d]\n\n", doc.GetErrorOffset(), line);
        }
        fclose(fd);
        return -1;
    }
    fclose(fd);
    return ParseDocument(doc, job, jump_task);
}

} //end namespace client
} //end namespace galaxy
} //end namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
