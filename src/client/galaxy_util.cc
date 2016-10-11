// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <cstring>
#include <stdio.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include "string_util.h"
#include "galaxy_util.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filewritestream.h"

namespace baidu {
namespace galaxy {
namespace client {

//初始化字符串映射表
std::string StringAuthority(const ::baidu::galaxy::sdk::Authority& authority) {
    std::string result;
    switch(authority) {
    case ::baidu::galaxy::sdk::kAuthorityCreateContainer:
        result = "kAuthorityCreateContainer";
        break;
    case ::baidu::galaxy::sdk::kAuthorityRemoveContainer:
        result = "kAuthorityRemoveContainer";
        break;
    case ::baidu::galaxy::sdk::kAuthorityUpdateContainer:
        result = "kAuthorityUpdateContainer";
        break;
    case ::baidu::galaxy::sdk::kAuthorityListContainer:
        result = "kAuthorityListContainer";
        break;
    case ::baidu::galaxy::sdk::kAuthoritySubmitJob:
        result = "kAuthoritySubmitJob";
        break;
    case ::baidu::galaxy::sdk::kAuthorityRemoveJob:
        result = "kAuthorityRemoveJob";
        break;
    case ::baidu::galaxy::sdk::kAuthorityUpdateJob:
        result = "kAuthorityUpdateJob";
        break;
    case ::baidu::galaxy::sdk::kAuthorityListJobs:
        result = "kAuthorityListJobs";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringAuthorityAction(const ::baidu::galaxy::sdk::AuthorityAction& action) {
    std::string result;
    switch(action) {
    case ::baidu::galaxy::sdk::kActionAdd:
        result = "kActionAdd";
        break;
    case ::baidu::galaxy::sdk::kActionRemove:
        result = "kActionRemove";
        break;
    case ::baidu::galaxy::sdk::kActionSet:
        result = "kActionSet";
        break;
    case ::baidu::galaxy::sdk::kActionClear:
        result = "kActionClear";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringContainerType(const ::baidu::galaxy::sdk::ContainerType& type) {
    std::string result;
    switch(type) {
    case ::baidu::galaxy::sdk::kNormalContainer:
        result = "normal";
        break;
    case ::baidu::galaxy::sdk::kVolumContainer:
        result = "volum";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringVolumMedium(const ::baidu::galaxy::sdk::VolumMedium& medium) {
    std::string result;
    switch(medium) {
    case ::baidu::galaxy::sdk::kSsd:
        result = "kSsd";
        break;
    case ::baidu::galaxy::sdk::kDisk:
        result = "kDisk";
        break;
    case ::baidu::galaxy::sdk::kBfs:
        result = "kBfs";
        break;
    case ::baidu::galaxy::sdk::kTmpfs:
        result = "kTmpfs";
        break;
    default:
        result = "";
    }
    return result;

}
std::string StringVolumType(const ::baidu::galaxy::sdk::VolumType& type) {
    std::string result;
    switch(type) {
    case ::baidu::galaxy::sdk::kEmptyDir:
        result = "kEmptyDir";
        break;
    case ::baidu::galaxy::sdk::kHostDir:
        result = "kHostDir";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringJobType(const ::baidu::galaxy::sdk::JobType& type) {

    std::string result;
    switch(type) {
    case ::baidu::galaxy::sdk::kJobMonitor:
        result = "kJobMonitor";
        break;
    case ::baidu::galaxy::sdk::kJobService:
        result = "kJobService";
        break;
    case ::baidu::galaxy::sdk::kJobBatch:
        result = "kJobBatch";
        break;
    case ::baidu::galaxy::sdk::kJobBestEffort:
        result = "kJobBestEffort";
        break;
    default:
        result = "";
    }
    return result;

}

std::string StringJobStatus(const ::baidu::galaxy::sdk::JobStatus& status) {
    std::string result;
    switch(status) {
    case ::baidu::galaxy::sdk::kJobPending:
        result = "Pending";
        break;
    case ::baidu::galaxy::sdk::kJobRunning:
        result = "Running";
        break;
    case ::baidu::galaxy::sdk::kJobFinished:
        result = "Finished";
        break;
    case ::baidu::galaxy::sdk::kJobDestroying:
        result = "Destroying";
        break;
    case ::baidu::galaxy::sdk::kJobUpdating:
        result = "Updating";
        break;
    case ::baidu::galaxy::sdk::kJobUpdatePaused:
        result = "UpdatePaused";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringPodStatus(const ::baidu::galaxy::sdk::PodStatus& status) {
    std::string result;
    switch(status) {
    case ::baidu::galaxy::sdk::kPodPending:
        result = "Pending";
        break;
    case ::baidu::galaxy::sdk::kPodReady:
        result = "Ready";
        break;
    case ::baidu::galaxy::sdk::kPodDeploying:
        result = "Deploying";
        break;
    case ::baidu::galaxy::sdk::kPodStarting:
        result = "Starting";
        break;
    case ::baidu::galaxy::sdk::kPodServing:
        result = "Serving";
        break;
    case ::baidu::galaxy::sdk::kPodFailed:
        result = "Failed";
        break;
    case ::baidu::galaxy::sdk::kPodFinished:
        result = "Finished";
        break;
    case ::baidu::galaxy::sdk::kPodRunning:
        result = "Running";
        break;
    case ::baidu::galaxy::sdk::kPodStopping:
        result = "Stoping";
        break;
    case ::baidu::galaxy::sdk::kPodTerminated:
        result = "Terminated";
        break;
    default:
        result = "";
    }
    return result;

}

std::string StringTaskStatus(const ::baidu::galaxy::sdk::TaskStatus& status) {
    std::string result;
    switch(status) {
    case ::baidu::galaxy::sdk::kTaskPending:
        result = "kTaskPending";
        break;
    case ::baidu::galaxy::sdk::kTaskDeploying:
        result = "kTaskDeploying";
        break;
    case ::baidu::galaxy::sdk::kTaskStarting:
        result = "kTaskStarting";
        break;
    case ::baidu::galaxy::sdk::kTaskServing:
        result = "kTaskServing";
        break;
    case ::baidu::galaxy::sdk::kTaskFailed:
        result = "kTaskFailed";
        break;
    case ::baidu::galaxy::sdk::kTaskFinished:
        result = "kTaskFinished";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringContainerStatus(const ::baidu::galaxy::sdk::ContainerStatus& status) {
    std::string result;
    switch(status) {
    case ::baidu::galaxy::sdk::kContainerPending:
        result = "Pending";
        break;
    case ::baidu::galaxy::sdk::kContainerAllocating:
        result = "Allocating";
        break;
    case ::baidu::galaxy::sdk::kContainerReady:
        result = "Ready";
        break;
    case ::baidu::galaxy::sdk::kContainerFinish:
        result = "Finish";
        break;
    case ::baidu::galaxy::sdk::kContainerError:
        result = "Error";
        break;
    case ::baidu::galaxy::sdk::kContainerDestroying:
        result = "Destroying";
        break;
    case ::baidu::galaxy::sdk::kContainerTerminated:
        result = "Terminated";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringContainerGroupStatus(const ::baidu::galaxy::sdk::ContainerGroupStatus& status) {
    std::string result;
    switch(status) {
    case ::baidu::galaxy::sdk::kContainerGroupNormal:
        result = "Normal";
        break;
    case ::baidu::galaxy::sdk::kContainerGroupTerminated:
        result = "Terminated";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringStatus(const ::baidu::galaxy::sdk::Status& status) {
    std::string result;
    switch(status) {
    case ::baidu::galaxy::sdk::kOk:
        result = "kOk";
        break;
    case ::baidu::galaxy::sdk::kError:
        result = "kError";
        break;
    case ::baidu::galaxy::sdk::kTerminate:
        result = "kTerminate";
        break;
    case ::baidu::galaxy::sdk::kAddAgentFail:
        result = "kAddAgentFail";
        break;
    case ::baidu::galaxy::sdk::kDeny:
        result = "kDeny";
        break;
    case ::baidu::galaxy::sdk::kJobNotFound:
        result = "kJobNotFound";
        break;
    case ::baidu::galaxy::sdk::kCreateContainerGroupFail:
        result = "kCreateContainerGroupFail";
        break;
    case ::baidu::galaxy::sdk::kRemoveContainerGroupFail:
        result = "kRemoveContainerGroupFail";
        break;
    case ::baidu::galaxy::sdk::kUpdateContainerGroupFail:
        result = "kUpdateContainerGroupFail";
        break;
    case ::baidu::galaxy::sdk::kRemoveAgentFail:
        result = "kRemoveAgentFail";
        break;
    case ::baidu::galaxy::sdk::kCreateTagFail:
        result = "kCreateTagFail";
        break;
    case ::baidu::galaxy::sdk::kAddAgentToPoolFail:
        result = "kAddAgentToPoolFail";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringAgentStatus(const ::baidu::galaxy::sdk::AgentStatus& status) {
    std::string result;
    switch(status) {
    case ::baidu::galaxy::sdk::kAgentUnknown:
        result = "Unknown";
        break;
    case ::baidu::galaxy::sdk::kAgentAlive:
        result = "Alive";
        break;
    case ::baidu::galaxy::sdk::kAgentDead:
        result = "Dead";
        break;
    case ::baidu::galaxy::sdk::kAgentOffline:
        result = "Offline";
        break;
    default:
        result = "";
    }
    return result;
}

std::string StringResourceError(const ::baidu::galaxy::sdk::ResourceError& error) {
    std::string result;
    switch(error) {
    case ::baidu::galaxy::sdk::kResOk:
        result = "kResOk";
        break;
    case ::baidu::galaxy::sdk::kNoCpu:
        result = "kNoCpu";
        break;
    case ::baidu::galaxy::sdk::kNoMemory:
        result = "kNoMemory";
        break;
    case ::baidu::galaxy::sdk::kNoMedium:
        result = "kNoMedium";
        break;
    case ::baidu::galaxy::sdk::kNoDevice:
        result = "kNoDevice";
        break;
    case ::baidu::galaxy::sdk::kNoPort:
        result = "kNoPort";
        break;
    case ::baidu::galaxy::sdk::kPortConflict:
        result = "kPortConflict";
        break;
    case ::baidu::galaxy::sdk::kTagMismatch:
        result = "kTagMismatch";
        break;
    case ::baidu::galaxy::sdk::kNoMemoryForTmpfs:
        result = "kNoMemoryForTmpfs";
        break;
    case ::baidu::galaxy::sdk::kPoolMismatch:
        result = "kPoolMismatch";
        break;
    case ::baidu::galaxy::sdk::kTooManyPods:
        result = "kTooManyPods";
        break;
    case ::baidu::galaxy::sdk::kNoVolumContainer:
        result = "kNoVolumContainer";
        break;
    default:
        result = "";
    }
    return result;

}

std::string StringBool(bool flag) {
    std::string sflag;
    if (flag) {
        sflag = "true";
    } else {
        sflag = "false";
    }
    return sflag;
}

std::string FormatDate(uint64_t datetime) {
    if (datetime < 100) {
        return "-";
    }

    char buf[100];
    time_t time = datetime / 1000000;
    struct tm *tmp;
    tmp = localtime(&time);
    strftime(buf, 100, "%F %X", tmp);
    std::string ret(buf);
    return ret;
}

bool GetHostname(std::string* hostname) {
    char buf[100];
    if (gethostname(buf, 100) != 0) {
        fprintf(stderr, "gethostname failed\n");
        return false;
    }
    *hostname = buf;
    return true;
}

//单位转换 1K => 1024
int UnitStringToByte(const std::string& input, int64_t* output) {
    if (output == NULL) {
        return -1;
    }

    std::map<char, int32_t> subfix_table;
    subfix_table['K'] = 1;
    subfix_table['M'] = 2;
    subfix_table['G'] = 3;
    subfix_table['T'] = 4;
    subfix_table['P'] = 5;
    subfix_table['E'] = 6;

    int64_t num = 0;
    char subfix = 0;
    int32_t shift = 0;
    int32_t matched = sscanf(input.c_str(), "%ld%c", &num, &subfix);
    if (matched <= 0) {
        fprintf(stderr, "unit sscanf failed\n");
        return -1;
    }

    if (matched == 2) {
        std::map<char, int32_t>::iterator it = subfix_table.find(subfix);
        if (it == subfix_table.end()) {
            fprintf(stderr, "unit is error, it must be in [K, M, G, T, P, E]\n");
            return -1;
        }
        shift = it->second;
    }

    while (shift > 0) {
        num *= 1024;
        shift--;
    }
    *output = num;
    return 0;
}

//单位转换 1024 => 1K
std::string HumanReadableString(int64_t num) {
    static const int max_shift = 6;
    static const char* const prefix[max_shift + 1] = {"", "K", "M", "G", "T", "P", "E"};
    int shift = 0;
    double v = num;
    while ((num>>=10) > 0 && shift < max_shift) {
        v /= 1024;
        shift++;
    }
    return ::baidu::common::NumToString(v) + prefix[shift];
}

//读取endpoint
bool LoadAgentEndpointsFromFile(const std::string& file_name, std::vector<std::string>* agents) {
    const int LINE_BUF_SIZE = 1024;
    char line_buf[LINE_BUF_SIZE];
    std::ifstream fin(file_name.c_str(), std::ifstream::in);        
    if (!fin.is_open()) {
        fprintf(stderr, "open %s failed\n", file_name.c_str());
        return false; 
    }
    
    bool ret = true;
    int i = 0;
    while (!fin.eof() && fin.good()) {
        fin.getline(line_buf, LINE_BUF_SIZE);     
        if (fin.gcount() == LINE_BUF_SIZE) {
            fprintf(stderr, "line buffer size overflow\n");     
            ret = false;
            break;
        } else if (fin.gcount() == 0 || strlen(line_buf) == 0) {
            continue; 
        }
        fprintf(stdout, "endpoint %ld: %s\n", fin.gcount(), line_buf);
        // NOTE string size should == strlen
        std::string agent_endpoint(line_buf, strlen(line_buf));
        agents->push_back(agent_endpoint);
        ++i;
    }

    if (!ret) {
        fin.close();
        return false;
    }

    fin.close(); 
    return true;
}

bool GenerateJson(int num_tasks, int num_data_volums, int num_ports, 
                  int num_data_packages, int num_services, const std::string& jobname) {

    std::string name = jobname;
    if (name.empty()) {
        name = "example";
    }

    if (num_data_volums != num_tasks) {
        num_data_volums = num_tasks;
    }

    rapidjson::Document document;
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
   
    //设置临时字符串使用
    rapidjson::Value obj_str(rapidjson::kStringType);
    std::string str;

    //根节点
    rapidjson::Value root(rapidjson::kObjectType);
    
    obj_str.SetString(name.c_str(), allocator);
    root.AddMember("name", obj_str, allocator);
    root.AddMember("type", "kJobService", allocator);
    root.AddMember("volum_jobs", "", allocator);
    //root.AddMember("version", "1.0.0", allocator);
    //root.AddMember("run_user", "galaxy", allocator);

    //deploy节点
    rapidjson::Value deploy(rapidjson::kObjectType);
    deploy.AddMember("replica", 1, allocator);
    deploy.AddMember("step", 1, allocator);
    deploy.AddMember("interval", 1, allocator);
    deploy.AddMember("max_per_host", 1, allocator);
    deploy.AddMember("tag", "", allocator);
    deploy.AddMember("pools", "example,test", allocator);

    root.AddMember("deploy", deploy, allocator);

    //pod节点
    rapidjson::Value pod(rapidjson::kObjectType);

    rapidjson::Value workspace_volum(rapidjson::kObjectType);
    workspace_volum.AddMember("size", "300M", allocator);
    workspace_volum.AddMember("type", "kEmptyDir", allocator);
    workspace_volum.AddMember("medium", "kDisk", allocator);
    workspace_volum.AddMember("dest_path", "/home/work", allocator);
    workspace_volum.AddMember("readonly", false, allocator);
    workspace_volum.AddMember("exclusive", false, allocator);
    workspace_volum.AddMember("use_symlink", false, allocator);
    
    pod.AddMember("workspace_volum", workspace_volum, allocator);

    rapidjson::Value data_volums(rapidjson::kArrayType);
    for (int i = 0; i < num_data_volums; ++i) {
        str = "/home/data/" + ::baidu::common::NumToString(i);
        obj_str.SetString(str.c_str(), allocator);

        rapidjson::Value data_volum(rapidjson::kObjectType);
        data_volum.AddMember("size", "800M", allocator);
        data_volum.AddMember("type", "kEmptyDir", allocator);
        data_volum.AddMember("medium", "kDisk", allocator);
        data_volum.AddMember("dest_path", obj_str, allocator);
        data_volum.AddMember("readonly", false, allocator);
        data_volum.AddMember("exclusive", false, allocator);
        data_volum.AddMember("use_symlink", true, allocator);

        data_volums.PushBack(data_volum, allocator);
    }
    if (num_data_volums > 0) {
        pod.AddMember("data_volums", data_volums, allocator);
    }

    rapidjson::Value tasks(rapidjson::kArrayType);
    
    for (int i = 0; i < num_tasks; ++i) {

        rapidjson::Value cpu(rapidjson::kObjectType);
        cpu.AddMember("millicores", 1000, allocator); 
        cpu.AddMember("excess", false, allocator); 

        rapidjson::Value mem(rapidjson::kObjectType);
        mem.AddMember("size", "800M", allocator);
        mem.AddMember("excess", false, allocator);

        rapidjson::Value tcp(rapidjson::kObjectType);
        tcp.AddMember("recv_bps_quota", "30M", allocator);
        tcp.AddMember("recv_bps_excess", false, allocator);
        tcp.AddMember("send_bps_quota", "30M", allocator);
        tcp.AddMember("send_bps_excess", false, allocator);

        rapidjson::Value blkio(rapidjson::kObjectType);
        blkio.AddMember("weight", 500, allocator);

        rapidjson::Value ports(rapidjson::kArrayType);
        for (int j = 0; j < num_ports; ++j) {
            rapidjson::Value port(rapidjson::kObjectType);
            str = name + "_port" + ::baidu::common::NumToString(i) + ::baidu::common::NumToString(j);
            obj_str.SetString(str.c_str(), allocator);
            port.AddMember("name", obj_str, allocator);

            str = "123" + ::baidu::common::NumToString(i) + ::baidu::common::NumToString(j);
            obj_str.SetString(str.c_str(), allocator);
            port.AddMember("port", obj_str, allocator);
            
            ports.PushBack(port, allocator);
        }

        
        rapidjson::Value package(rapidjson::kObjectType);
        
        str = "ftp://***.baidu.com/home/users/***/exec/" + ::baidu::common::NumToString(i) + "/linkbase.tar.gz";;
        obj_str.SetString(str.c_str(), allocator);
        package.AddMember("source_path", obj_str, allocator);

        str = "/home/work/exec/" + ::baidu::common::NumToString(i);
        obj_str.SetString(str.c_str(), allocator);
        package.AddMember("dest_path", obj_str, allocator);
        package.AddMember("version", "1.0.0", allocator);

        rapidjson::Value exec_package(rapidjson::kObjectType);
        exec_package.AddMember("start_cmd", "sh app_start.sh", allocator);
        exec_package.AddMember("stop_cmd", "", allocator);
        exec_package.AddMember("health_cmd", "", allocator);
        exec_package.AddMember("package", package, allocator);

        rapidjson::Value data_packages(rapidjson::kArrayType);
        for (int j = 0; j < num_data_packages; ++j) {

            str = "ftp://***.baidu.com/home/users/***/data/" + ::baidu::common::NumToString(i)
                  + ::baidu::common::NumToString(j) + "/linkbase.dict.tar.gz";
            obj_str.SetString(str.c_str(), allocator);

            rapidjson::Value package(rapidjson::kObjectType);
            package.AddMember("source_path", obj_str, allocator);

            str = "/home/data/"  + ::baidu::common::NumToString(i) + "/" + ::baidu::common::NumToString(j);
            obj_str.SetString(str.c_str(), allocator);
            package.AddMember("dest_path", obj_str, allocator);
            package.AddMember("version", "1.0.0", allocator);

            data_packages.PushBack(package, allocator);

        }

        rapidjson::Value data_package(rapidjson::kObjectType);
        data_package.AddMember("reload_cmd", "", allocator);
        data_package.AddMember("packages", data_packages, allocator);

        rapidjson::Value services(rapidjson::kArrayType);
        for (int j = 0; j < num_services; ++j) {
            rapidjson::Value service(rapidjson::kObjectType);
            str = name + "_service" + ::baidu::common::NumToString(i) + ::baidu::common::NumToString(j);
            obj_str.SetString(str.c_str(), allocator);
            service.AddMember("service_name", obj_str, allocator);

            str = name + "_port" + ::baidu::common::NumToString(i) + ::baidu::common::NumToString(j);
            obj_str.SetString(str.c_str(), allocator);
            service.AddMember("port_name", obj_str, allocator);
            service.AddMember("use_bns", false, allocator);
            service.AddMember("tag", "", allocator);
            service.AddMember("health_check_type", "", allocator);
            service.AddMember("health_check_script", "", allocator);
            service.AddMember("token", "", allocator);
            
            services.PushBack(service, allocator);
        }

        rapidjson::Value task(rapidjson::kObjectType);
        task.AddMember("cpu", cpu, allocator);
        task.AddMember("mem", mem, allocator);
        task.AddMember("tcp", tcp, allocator);
        task.AddMember("blkio", blkio, allocator);
        
        if (num_ports > 0) {
            task.AddMember("ports", ports, allocator);
        }

        task.AddMember("exec_package", exec_package, allocator);
        
        if (num_data_packages > 0) {
            task.AddMember("data_package", data_package, allocator);
        }

        if (num_services > 0) {
            task.AddMember("services", services, allocator);
        }

        tasks.PushBack(task, allocator);
        
    }

    if (num_tasks > 0) {
        pod.AddMember("tasks", tasks, allocator);
    }
    root.AddMember("pod", pod, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    root.Accept(writer);
    std::string str_json = buffer.GetString();
    fprintf(stdout, "%s\n", str_json.c_str());
    return true;
}

int GetLineNumber(FILE *fd, size_t offset) {
    int line = 0;
    char ch;
    fseek(fd, 0, 0); //back to begin
    bool pline = true;
    do {
        if (ftell(fd) == 0) {
            pline = false;
            fprintf(stderr, "%d: ", line + 1);
        }
        ch = fgetc(fd);
        if (ch == '\n') {
            pline = true;
            ++line;
        }
        
        fprintf(stderr, "%c", ch);
        if (pline) {
            pline = false;
            fprintf(stderr, "%d: ", line + 1);
        }

        if (ftell(fd) >= (long)offset) {
            if (ch != '\n') {
                ++line;
            }
            break;
        }
    } while (ch != EOF);
    
    return line;
}


} // end namespace client
} // end namespace galaxy
} // end namespace baidu
/* vim: set ts=4 sw=4 sts=4 tw=100 */
