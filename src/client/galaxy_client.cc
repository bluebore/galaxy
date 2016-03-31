// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sdk/galaxy.h"

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include <time.h>
#include <gflags/gflags.h>
#include <tprinter.h>
#include <string_util.h>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "master/master_watcher.h"
#include "ins_sdk.h"

DEFINE_string(nexus_servers, "", "server list of nexus, e.g abc.com:1234,def.com:5342");
DEFINE_string(nexus_root_path, "/baidu/galaxy", "root path of galaxy cluster on nexus, e.g /ps/galaxy");
DEFINE_string(master_path, "/master", "master path on nexus");
DEFINE_string(f, "", "specify config file ,job config file or label config file");
DEFINE_string(n, "", "specify job name to query pods");
DEFINE_string(j, "", "specify job id");
DEFINE_string(l, "", "add a label to agent");
DEFINE_string(p, "", "specify pod id");
DEFINE_string(e, "", "specify agent endpoint");
DEFINE_int32(d, 0, "specify delay time to query");
DEFINE_int32(cli_server_port, 8775, "cli server listen port");
DECLARE_string(flagfile);

const std::string kGalaxyUsage = "galaxy client.\n"
                                 "Usage:\n"
                                 "    galaxy submit -f <jobconfig>\n" 
                                 "    galaxy jobs \n"
                                 "    galaxy agents\n"
                                 "    galaxy pods -j <jobid>\n"
                                 "    galaxy pods -e <endpoint>\n"
                                 "    galaxy tasks -j <jobid>\n"
                                 "    galaxy tasks -e <endpoint>\n"
                                 "    galaxy kill -j <jobid>\n"
                                 "    galaxy update -j <jobid> -f <jobconfig>\n"
                                 "    galaxy label -l <label> -f <lableconfig>\n"
                                 "    galaxy preempt -f <config>\n"
                                 "    galaxy offline -e <endpoint>\n"
                                 "    galaxy online -e <endpoint>\n"
                                 "    galaxy status \n"
                                 "    galaxy enter safemode \n"
                                 "    galaxy leave safemode \n"
                                 "Options:\n"
                                 "    -f config    Specify config file ,eg job config file , label config file.\n"
                                 "    -j jobid     Specify job id to kill or update.\n"
                                 "    -d delay     Specify delay in second to update infomation.\n"
                                 "    -l label     Add label to list of agents.\n"
                                 "    -e agent     Specify endpoint.\n"
                                 "    -n name      Specify job name to query pods.\n";
std::string FormatDate(int64_t datetime) {
    if (datetime < 100) {
        return "-";
    }
    char buffer[100];
    time_t time = datetime / 1000000;
    struct tm *tmp;
    tmp = localtime(&time);
    strftime(buffer, 100, "%F %X", tmp);
    std::string ret(buffer);
    return ret;
}

int ReadableStringToInt(const std::string& input, int64_t* output) {
    if (output == NULL) {
        return -1;
    }
    std::map<char, int32_t> subfix_table;
    subfix_table['K'] = 1;
    subfix_table['M'] = 2;
    subfix_table['G'] = 3;
    subfix_table['T'] = 4;
    subfix_table['B'] = 5;
    subfix_table['Z'] = 6;
    int64_t num = 0;
    char subfix = 0;
    int32_t shift = 0;
    int32_t matched = sscanf(input.c_str(), "%ld%c", &num, &subfix);
    if (matched <= 0) {
        return -1;
    }
    if (matched == 2) {
        std::map<char, int32_t>::iterator it = subfix_table.find(subfix);
        if (it == subfix_table.end()) {
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

bool LoadAgentEndpointsFromFile(
        const std::string& file_name, 
        std::vector<std::string>* agents) {
    const int LINE_BUF_SIZE = 1024;
    char line_buf[LINE_BUF_SIZE];
    std::ifstream fin(file_name.c_str(), std::ifstream::in);        
    if (!fin.is_open()) {
        fprintf(stderr, "open %s failed\n", file_name.c_str());
        return false; 
    }
    
    bool ret = true;
    while (fin.good()) {
        fin.getline(line_buf, LINE_BUF_SIZE);     
        if (fin.gcount() == LINE_BUF_SIZE) {
            fprintf(stderr, "line buffer size overflow\n");     
            ret = false;
            break;
        } else if (fin.gcount() == 0) {
            continue; 
        }
        fprintf(stdout, "label %s\n", line_buf);
        // NOTE string size should == strlen
        std::string agent_endpoint(line_buf, strlen(line_buf));
        agents->push_back(agent_endpoint);
    }
    if (!ret) {
        fin.close();
        return false;
    }

    if (fin.fail()) {
        fin.close(); 
        return false;
    }
    fin.close(); 
    return true;
}

int LabelAgent() {
    std::vector<std::string> agent_endpoints;
    if (LoadAgentEndpointsFromFile(FLAGS_f, &agent_endpoints)) {
        return -1;     
    }
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    if (!galaxy->LabelAgents(FLAGS_l, agent_endpoints)) {
        return -1; 
    }
    return 0;
}

void ReadBinary(const std::string& file, std::string* binary) {
    FILE* fp = fopen(file.c_str(), "rb");
    if (fp == NULL) {
        fprintf(stderr, "Read binary fail\n");
        return;
    }
    char buf[1024];
    int len = 0;
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        binary->append(buf, len);
    }
    fclose(fp);
}

int BuildPreemptFromConfig(const std::string& config, ::baidu::galaxy::PreemptPropose* propose) {
    FILE* fd = fopen(config.c_str(), "r");
    char buffer[5120];
    rapidjson::FileReadStream frs(fd, buffer, sizeof(buffer));
    rapidjson::Document document;
    document.ParseStream<0>(frs);
    if (!document.IsObject()) {
        fprintf(stderr, "invalidate config file\n");
        return -1;
    }
    if (!document.HasMember("addr")) {
        fprintf(stderr, "addr is required \n");
        return -1;
    }
    propose->addr = document["addr"].GetString();

    if (!document.HasMember("pending_pod")) {
        fprintf(stderr, "pending_pod is required\n");
        return -1;
    } 
    const rapidjson::Value& pending_pod = document["pending_pod"];
    if (!pending_pod.HasMember("jobid")) {
        fprintf(stderr, "jobid is required in pending_pod\n");
        return -1;
    }

    if (!pending_pod.HasMember("podid")) {
        fprintf(stderr, "podid is required in pending_pod\n");
        return -1;
    }
    propose->pending_pod = std::make_pair(pending_pod["jobid"].GetString(),
                                          pending_pod["podid"].GetString());
    if (!document.HasMember("preempted_pods")) {
        fprintf(stderr, "preempted_pods is required\n");
        return -1;
    }
    const rapidjson::Value& preempted_pods = document["preempted_pods"];
    for (rapidjson::SizeType i = 0; i < preempted_pods.Size(); i++) {
        const rapidjson::Value& preempted_pod = preempted_pods[i];
        if (!preempted_pod.HasMember("jobid")) {
            fprintf(stderr, "job id is required");
            return -1;
        }
        if (!preempted_pod.HasMember("podid")) {
            fprintf(stderr, "pod id is required");
            return -1;
        }
        propose->preempted_pods.push_back(std::make_pair(
                    preempted_pod["jobid"].GetString(),
                    preempted_pod["podid"].GetString()
                    ));
    }
    return 0;
}

// build job description from json config
// TODO do some validations
int BuildJobFromConfig(const std::string& config, ::baidu::galaxy::JobDescription* job) {
    int ok = 0;
    FILE* fd = fopen(config.c_str(), "r");
    char buffer[5120];
    rapidjson::FileReadStream frs(fd, buffer, sizeof(buffer));
    rapidjson::Document document;
    document.ParseStream<0>(frs);
    if (!document.IsObject()) {
        fprintf(stderr, "invalidate config file\n");
        return -1;
    }
    fclose(fd);
    if (!document.HasMember("name")) {
        fprintf(stderr, "name is required in config\n");
        return -1;
    }
    job->job_name = document["name"].GetString();
    job->replica = document["replica"].GetInt();
    if (!document.HasMember("type")) {
        fprintf(stderr, "type is required in config\n");
        return -1;
    }
    job->type = document["type"].GetString();
    /*if (document.HasMember("priority")) {
        job->priority = document["priority"].GetInt();
    }*/
    if (document.HasMember("labels")) {
        job->label = document["labels"].GetString();
    }
    if (document.HasMember("deploy_step")) {
        job->deploy_step = document["deploy_step"].GetInt();
    }
    if (!document.HasMember("pod")) {
        fprintf(stderr, "pod is required in config\n");
        return -1;
    }
    ::baidu::galaxy::PodDescription& pod = job->pod;
    ::baidu::galaxy::ResDescription* res = &pod.requirement;
    const rapidjson::Value& pod_json = document["pod"];
    if (!pod_json.HasMember("requirement")) {
        fprintf(stderr, "requirement is required in pod\n");
        return -1;
    }
    if (!pod_json.HasMember("version")) {
        fprintf(stderr, "version is required in pod\n");
        return -1;
    }
    pod.version = pod_json["version"].GetString();
    const rapidjson::Value& pod_require = pod_json["requirement"];
    if (!pod_require.HasMember("millicores")) {
        fprintf(stderr, "millicores is required\n");
        return -1;
    }
    if (pod_json.HasMember("namespace_isolation")) {
        pod.namespace_isolation = pod_json["namespace_isolation"].GetBool();
    } else {
        pod.namespace_isolation = true;
    }
    res->millicores = pod_require["millicores"].GetInt();
    if (!pod_require.HasMember("memory")) {
        fprintf(stderr, "memory is required\n");
        return -1;
    }
    ok = ReadableStringToInt(pod_require["memory"].GetString(), &res->memory);
    if (ok != 0) {
        fprintf(stderr, "fail to parse pod memory %s\n", pod_require["memory"].GetString());
        return -1;
    }
    if (pod_require.HasMember("ports")) {
        const rapidjson::Value&  pod_ports = pod_require["ports"];
        for (rapidjson::SizeType i = 0; i < pod_ports.Size(); i++) {
            res->ports.push_back(pod_ports[i].GetInt());
        }
    }
    if (pod_require.HasMember("disks")) {
        const rapidjson::Value& pod_disks = pod_require["disks"];
        for (rapidjson::SizeType i = 0; i < pod_disks.Size(); i++) {
            ::baidu::galaxy::VolumeDescription vol;
            vol.quota = pod_disks[i]["quota"].GetInt64();
            vol.path = pod_disks[i]["path"].GetString();
            res->disks.push_back(vol);
        } 
    }
    if (pod_require.HasMember("ssds")) {
        const rapidjson::Value& pod_ssds = pod_require["ssds"];
        for (rapidjson::SizeType i = 0; i < pod_ssds.Size(); i++) {
            ::baidu::galaxy::VolumeDescription vol;
            vol.quota = pod_ssds[i]["quota"].GetInt64();
            vol.path = pod_ssds[i]["path"].GetString();
            res->ssds.push_back(vol);
        }
    }
    if (pod_require.HasMember("read_bytes_ps")) {
        ok = ReadableStringToInt(pod_require["read_bytes_ps"].GetString(), &res->read_bytes_ps);
        if (ok != 0) {
            fprintf(stderr, "fail to parse pod read_bytes_ps %s\n", pod_require["read_bytes_ps"].GetString());
            return -1;
        }
    }
    if (pod_require.HasMember("write_bytes_ps")) {
        ok = ReadableStringToInt(pod_require["write_bytes_ps"].GetString(), &res->write_bytes_ps);
        if (ok != 0) {
            fprintf(stderr, "fail to parse pod write_bytes_ps %s\n", pod_require["write_bytes_ps"].GetString());
            return -1;
        } 
    }
    if (pod_require.HasMember("read_io_ps")) {
        res->read_io_ps = pod_require["read_io_ps"].GetInt();
    }
    if (pod_require.HasMember("write_io_ps")) {
        res->write_io_ps = pod_require["write_io_ps"].GetInt();
    }

    std::vector< ::baidu::galaxy::TaskDescription>& tasks = pod.tasks;
    if (pod_json.HasMember("tasks")) {
        const rapidjson::Value& tasks_json = pod_json["tasks"];
        for (rapidjson::SizeType i = 0; i < tasks_json.Size(); i++) {
            ::baidu::galaxy::TaskDescription task;
            task.binary = tasks_json[i]["binary"].GetString();
            task.offset = i;
            task.source_type = tasks_json[i]["source_type"].GetString();
            if (task.source_type == "kSourceTypeBinary") {
                ReadBinary(tasks_json[i]["binary"].GetString(), &task.binary);
            }
            if (tasks_json[i].HasMember("start_command")) {
                task.start_cmd = tasks_json[i]["start_command"].GetString();
            }
            if (tasks_json[i].HasMember("stop_command")) {
                task.stop_cmd = tasks_json[i]["stop_command"].GetString();
            }
            task.mem_isolation_type = "kMemIsolationCgroup";
            if (tasks_json[i].HasMember("mem_isolation_type")) {
                task.mem_isolation_type = tasks_json[i]["mem_isolation_type"].GetString();
            }
            if (tasks_json[i].HasMember("envs")) {
                const rapidjson::Value& task_envs = tasks_json[i]["envs"];
                for (rapidjson::SizeType i = 0; i < task_envs.Size(); i++) {
                    task.envs.insert(task_envs[i].GetString());
                }
            }
            task.cpu_isolation_type= "kCpuIsolationHard";
            if (tasks_json[i].HasMember("cpu_isolation_type")) {
                task.cpu_isolation_type = tasks_json[i]["cpu_isolation_type"].GetString();
            }
            task.namespace_isolation = pod.namespace_isolation;

            res = &task.requirement;
            res->millicores = tasks_json[i]["requirement"]["millicores"].GetInt();
            ok = ReadableStringToInt(tasks_json[i]["requirement"]["memory"].GetString(), &res->memory);
            if (ok != 0) {
                fprintf(stderr, "fail to parse task memory %s\n", tasks_json[i]["requirement"]["memory"].GetString());
                return -1;
            }
            if (tasks_json[i]["requirement"].HasMember("ports")) {
                const rapidjson::Value& task_ports = tasks_json[i]["requirement"]["ports"];
                for (rapidjson::SizeType i = 0; i < task_ports.Size(); i++) {
                    res->ports.push_back(task_ports[i].GetInt());
                }
            }
            if (tasks_json[i]["requirement"].HasMember("disks")) {
                const rapidjson::Value& task_disks = tasks_json[i]["requirement"]["disks"];
                for (rapidjson::SizeType i = 0; i < task_disks.Size(); i++) { 
                    ::baidu::galaxy::VolumeDescription task_vol;
                    task_vol.quota = task_disks[i]["quota"].GetInt64();
                    task_vol.path = task_disks[i]["path"].GetString();
                    res->disks.push_back(task_vol);
                } 
            }
            if (tasks_json[i]["requirement"].HasMember("ssds")) {
                const rapidjson::Value& task_ssds = tasks_json[i]["requirement"]["ssds"];
                for (rapidjson::SizeType i = 0; i < task_ssds.Size(); i++){ 
                    ::baidu::galaxy::VolumeDescription task_vol;
                    task_vol.quota = task_ssds[i]["quota"].GetInt64();
                    task_vol.path = task_ssds[i]["path"].GetString();
                    res->ssds.push_back(task_vol);
                }
            }
            if (tasks_json[i]["requirement"].HasMember("read_bytes_ps")) {
                ok = ReadableStringToInt(tasks_json[i]["requirement"]["read_bytes_ps"].GetString(), &res->read_bytes_ps);
                if (ok != 0) {
                    fprintf(stderr, "fail to parse task read_bytes_ps %s\n", tasks_json[i]["requirement"]["read_bytes_ps"].GetString());
                    return -1;
                }
            }
            if (tasks_json[i]["requirement"].HasMember("write_bytes_ps")) {
                ok = ReadableStringToInt(tasks_json[i]["requirement"]["write_bytes_ps"].GetString(), &res->write_bytes_ps);
                if (ok != 0) {
                    fprintf(stderr, "fail to parse task write_bytes_ps %s\n", tasks_json[i]["requirement"]["write_bytes_ps"].GetString());
                    return -1;
                }
            }
            if (tasks_json[i]["requirement"].HasMember("read_io_ps")) {
                res->read_io_ps = tasks_json[i]["requirement"]["read_io_ps"].GetInt64();
            }
            if (tasks_json[i]["requirement"].HasMember("write_io_ps")) {
                res->write_io_ps = tasks_json[i]["requirement"]["write_io_ps"].GetInt64();
            }
            if (tasks_json[i]["requirement"].HasMember("io_weight")) {
                res->io_weight = tasks_json[i]["requirement"]["io_weight"].GetInt();
                if (res->io_weight < 10 || res->io_weight > 1000) {
                    fprintf(stderr, "invalid io_weight value %d, io_weight value should in range of [10 - 1000]\n",
                    tasks_json[i]["requirement"]["io_weight"].GetInt());
                    return -1;
                }
            }

            tasks.push_back(task);
        }
    }
    return 0;
 
}

int AddJob() { 
    if (FLAGS_f.empty()) {
        fprintf(stderr, "-f is required\n");
        return -1;
    }
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    baidu::galaxy::JobDescription job;
    int build_job_ok = BuildJobFromConfig(FLAGS_f, &job);
    if (build_job_ok != 0) {
        fprintf(stderr, "Fail to build job\n");
        return -1;
    }
    std::string jobid;
    bool ok = galaxy->SubmitJob(job, &jobid);
    if (!ok) {
        fprintf(stderr, "Submit job fail\n");
        return 1;
    }
    printf("Submit job %s\n", jobid.c_str());
    return 0;
}

int UpdateJob() {  
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    baidu::galaxy::JobDescription job;
    int build_job_ok = BuildJobFromConfig(FLAGS_f, &job);
    if (build_job_ok != 0) {
        fprintf(stderr, "Fail to build job\n");
        return -1;
    }
    bool ok = galaxy->UpdateJob(FLAGS_j, job);
    if (ok) {
        printf("Update job %s ok\n", FLAGS_j.c_str());
        return 0;
    }else {
        printf("Fail to update job %s\n", FLAGS_j.c_str());
        return 1;
    }
}


int ListAgent() { 
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    while (true) {
        std::vector<baidu::galaxy::NodeDescription> agents;
        baidu::common::TPrinter tp(12);
        tp.AddRow(12, "", "addr", "build", "state", "pods", "cpu_used", "cpu_assigned", "cpu_total", "mem_used", "mem_assigned", "mem_total", "labels");
        if (galaxy->ListAgents(&agents)) {
            for (uint32_t i = 0; i < agents.size(); i++) {
                std::vector<std::string> vs;
                vs.push_back(baidu::common::NumToString(i + 1));
                vs.push_back(agents[i].addr);
                vs.push_back(agents[i].build);
                vs.push_back(agents[i].state);
                vs.push_back(baidu::common::NumToString(agents[i].task_num));
                vs.push_back(baidu::common::NumToString(agents[i].cpu_used));
                vs.push_back(baidu::common::NumToString(agents[i].cpu_assigned));
                vs.push_back(baidu::common::NumToString(agents[i].cpu_share));
                vs.push_back(baidu::common::HumanReadableString(agents[i].mem_used));
                vs.push_back(baidu::common::HumanReadableString(agents[i].mem_assigned));
                vs.push_back(baidu::common::HumanReadableString(agents[i].mem_share));
                vs.push_back(agents[i].labels);
                tp.AddRow(vs);
            }
            printf("%s\n", tp.ToString().c_str());
        }else {
            printf("Listagent fail\n");
            return 1;
        }
        if (FLAGS_d <=0) {
            break;
        }else{
            ::sleep(FLAGS_d);
            ::system("clear");
        }
    }
    return 0;
}
int ShowTask() {
     if (FLAGS_j.empty() && FLAGS_e.empty()) {
        fprintf(stderr, "-j or -e option is required\n");
        return -1;
    }
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    while (true) {
        std::vector<baidu::galaxy::TaskInformation> tasks;
        if (!FLAGS_j.empty()) {
            bool ok = galaxy->GetTasksByJob(FLAGS_j, &tasks);
            if (!ok) {
                fprintf(stderr, "Fail to get tasks\n");
                return -1;
            }
        } else if (!FLAGS_e.empty()) {
            bool ok = galaxy->GetTasksByAgent(FLAGS_e, &tasks);
            if (!ok) {
                fprintf(stderr, "Fail to get tasks\n");
                return -1;
            }
        }
        baidu::common::TPrinter tp(9);
        tp.AddRow(9, "", "podid", "state", "cpu", "mem", "disk(r/w)","endpoint", "deploy","start");
        for (size_t i = 0; i < tasks.size(); i++) {
            std::vector<std::string> vs;
            vs.push_back(baidu::common::NumToString((int32_t)i + 1));
            vs.push_back(tasks[i].podid);
            vs.push_back(tasks[i].state);
            std::string cpu = baidu::common::NumToString(tasks[i].used.millicores);
            vs.push_back(cpu);
            std::string mem = baidu::common::HumanReadableString(tasks[i].used.memory);
            vs.push_back(mem);
            std::string disk_io = baidu::common::HumanReadableString(tasks[i].used.read_bytes_ps) +"/s" + " / " 
                                  + baidu::common::HumanReadableString(tasks[i].used.write_bytes_ps) +"/s";
            vs.push_back(disk_io);
            vs.push_back(tasks[i].endpoint);
            vs.push_back(FormatDate(tasks[i].deploy_time));
            vs.push_back(FormatDate(tasks[i].start_time));
            tp.AddRow(vs);
        }
        printf("%s\n", tp.ToString().c_str());
        if (FLAGS_d <=0) {
            break;
        }else{
            ::sleep(FLAGS_d);
            ::system("clear");
        }
    }
    return 0;
}

int ShowPod() {
    if (FLAGS_j.empty() && FLAGS_n.empty() && FLAGS_e.empty()) {
        fprintf(stderr, "-j ,-n or -e  option is required\n");
        return -1;
    }
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    while (true) {
        std::vector<baidu::galaxy::PodInformation> pods;
        if (!FLAGS_j.empty()) {
            bool ok = galaxy->ShowPod(FLAGS_j, &pods);
            if (!ok) {
                fprintf(stderr, "Fail to get pods\n");
                return -1;
            }
        } else if (!FLAGS_n.empty()) {
            bool ok = galaxy->GetPodsByName(FLAGS_n, &pods);
            if (!ok) {
                fprintf(stderr, "Fail to get pods\n");
                return -1;
            }
        } else if (!FLAGS_e.empty()) {
            bool ok = galaxy->GetPodsByAgent(FLAGS_e, &pods);
            if (!ok) {
                fprintf(stderr, "Fail to get pods\n");
                return -1;
            }

        } 
        baidu::common::TPrinter tp(11);
        tp.AddRow(11, "", "id", "state", "cpu(u/a)", "mem(u/a)", "disk(r/w)","endpoint", "version", "pending","sched","start");
        for (size_t i = 0; i < pods.size(); i++) {
            std::vector<std::string> vs;
            vs.push_back(baidu::common::NumToString((int32_t)i + 1));
            vs.push_back(pods[i].podid);
            vs.push_back(pods[i].state);
            std::string cpu = baidu::common::NumToString(pods[i].used.millicores) + "/" +\
                              baidu::common::NumToString(pods[i].assigned.millicores);
            vs.push_back(cpu);
            std::string mem = baidu::common::HumanReadableString(pods[i].used.memory) + "/" +\
                              baidu::common::HumanReadableString(pods[i].assigned.memory);
            vs.push_back(mem);
            std::string disk_io = baidu::common::HumanReadableString(pods[i].used.read_bytes_ps) +"/s" + " / " 
                                  + baidu::common::HumanReadableString(pods[i].used.write_bytes_ps) +"/s";
            vs.push_back(disk_io);
            vs.push_back(pods[i].endpoint);
            vs.push_back(pods[i].version);
            vs.push_back(FormatDate(pods[i].pending_time));
            vs.push_back(FormatDate(pods[i].sched_time));
            vs.push_back(FormatDate(pods[i].start_time));
            tp.AddRow(vs);
        }
        printf("%s\n", tp.ToString().c_str());
        if (FLAGS_d <=0) {
            break;
        }else{
            ::sleep(FLAGS_d);
            ::system("clear");
        }

    }
    return 0;
}

std::string GetAgentById(std::string job_id, std::string pod_id) {
    if (FLAGS_p.empty()) {
        fprintf(stderr, "-p option is required\n");
        return "";
    }
    if (FLAGS_j.empty()) {
        fprintf(stderr, "-j option is required\n");
        return "";
    }
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    std::vector<baidu::galaxy::PodInformation> pods;
    bool ok = galaxy->ShowPod(job_id, &pods);
    if (!ok) {
        fprintf(stderr, "Fail to query job\n");
        return "";
    }
    std::vector<baidu::galaxy::PodInformation>::iterator it = pods.begin();
    std::string agent_endpoint = "";
    for (; it != pods.end(); ++it) {
        if (it->podid == pod_id) {
            agent_endpoint = it->endpoint;
            break;
        }
    }
    if (agent_endpoint == "") {
        fprintf(stderr, "Fail to find pod\n");
        return "";
    }
    return agent_endpoint.substr(0, agent_endpoint.find_last_of(":"));
}

int AttachPod() {
    std::string agent_endpoint = GetAgentById(FLAGS_j, FLAGS_p);
    if (agent_endpoint == "") {
        return -1;
    }
    int sockfd = 0;
    const int BUFFER_LEN = 1024 * 10;
    char buffer[BUFFER_LEN];
    struct sockaddr_in serv_addr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Fail to create socket\n");
        return -1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(FLAGS_cli_server_port);

    if (inet_pton(AF_INET, agent_endpoint.c_str(), &serv_addr.sin_addr) < 0) {
        fprintf(stderr, "Wrong server address\n");
        return -1;
    }
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Fail to connect cli server\n");
        return -1;
    }
    
    struct termios temp_termios;
    struct termios orig_termios;
    ::signal(SIGINT, SIG_IGN);
    ::signal(SIGTERM, SIG_IGN);
    ::tcgetattr(0, &orig_termios);
    temp_termios = orig_termios;
    temp_termios.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
    temp_termios.c_cc[VTIME] = 1;   // 终端等待延迟时间（十分之一秒为单位）
    temp_termios.c_cc[VMIN] = 1;    // 终端接收字符数
    ::tcsetattr(0, TCSANOW, &temp_termios);
    
    fd_set fd_in;
    int ret = 0;
    write(sockfd, FLAGS_p.c_str(), 36); //send pod id

    fprintf(stderr, "send: %s\n", FLAGS_p.c_str());
    
    while (1) {
        FD_ZERO(&fd_in);
        FD_SET(sockfd, &fd_in);
        FD_SET(0, &fd_in);

        ret = ::select(sockfd + 1, &fd_in, NULL, NULL, NULL);
        if (ret < 0) {
            fprintf(stderr, "Select error\n");
            break;
        } else {
            if (FD_ISSET(0, &fd_in)) {
                ret = ::read(0, buffer, sizeof(buffer));
                if (ret > 0) {
                    write(sockfd, buffer, ret);
                } else if (ret < 0) {
                    fprintf(stderr, "Read error\n");
                    break;
                }
            }
            if (FD_ISSET(sockfd, &fd_in)) {
                ret = ::read(sockfd, buffer, sizeof(buffer));
                if (ret > 0) {
                    write(1, buffer, ret);
                } else if (ret < 0) {
                    fprintf(stderr, "Read error\n");
                } else {
                    fprintf(stdout, "Attach pod finished!\n");
                    break;
                }
            }
        }
    }

    close(sockfd);
    ::tcsetattr(0, TCSANOW, &orig_termios);
    if (ret < 0) {
        return -1;
    }
    return 0;
}
int SwitchSafeMode(bool mode) { 
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    ::baidu::galaxy::MasterStatus status;
    bool ok = galaxy->SwitchSafeMode(mode);
    if (!ok) {
        fprintf(stderr, "fail to switch safemode\n");
        return -1;
    }
    printf("sucessfully %s safemode\n", mode ? "enter" : "leave");
    return 0;
}

int GetMasterStatus() {
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    ::baidu::galaxy::MasterStatus status;
    bool ok = galaxy->GetStatus(&status);
    if (!ok) {
        fprintf(stderr, "fail to get status\n");
        return -1;
    }
    std::string master_endpoint;
    ok = galaxy->GetMasterAddr(&master_endpoint);
    if (!ok) {
        fprintf(stderr, "fail to get master addr\n");
        return -1;
    }
    baidu::common::TPrinter master(2);
    printf("master infomation\n");
    master.AddRow(2, "addr", "state");
    std::string mode = status.safe_mode ? "safe mode" : "normal mode";
    master.AddRow(2, master_endpoint.c_str(), mode.c_str());
    printf("%s\n", master.ToString().c_str());

    baidu::common::TPrinter agent(3);
    printf("cluster agent infomation\n");
    agent.AddRow(3, "agent total", "live count", "dead count");
    agent.AddRow(3, baidu::common::NumToString(status.agent_total).c_str(), 
                     baidu::common::NumToString(status.agent_live_count).c_str(),
                     baidu::common::NumToString(status.agent_dead_count).c_str());
    printf("%s\n", agent.ToString().c_str());


    baidu::common::TPrinter mem(3);
    printf("cluster memory infomation\n");
    mem.AddRow(3, "mem total", "mem used", "mem assigned");
    mem.AddRow(3, baidu::common::HumanReadableString(status.mem_total).c_str(), 
                     baidu::common::HumanReadableString(status.mem_used).c_str(),
                     baidu::common::HumanReadableString(status.mem_assigned).c_str());
    printf("%s\n", mem.ToString().c_str());

    baidu::common::TPrinter cpu(3);
    printf("cluster cpu infomation\n");
    cpu.AddRow(3, "cpu total", "cpu used", "cpu assigned");
    cpu.AddRow(3, baidu::common::NumToString(status.cpu_total).c_str(), 
                  baidu::common::NumToString(status.cpu_used).c_str(),
                  baidu::common::NumToString(status.cpu_assigned).c_str());
    printf("%s\n", cpu.ToString().c_str());

    baidu::common::TPrinter job(5);
    printf("cluster job infomation\n");
    job.AddRow(5, "job count", "job scale up", "job scale down", "job need update", "pod count");
    job.AddRow(5, baidu::common::NumToString(status.job_count).c_str(), 
                  baidu::common::NumToString(status.scale_up_job_count).c_str(),
                  baidu::common::NumToString(status.scale_down_job_count).c_str(),
                  baidu::common::NumToString(status.need_update_job_count).c_str(),
                  baidu::common::NumToString(status.pod_count).c_str()
                  );
    printf("%s\n", job.ToString().c_str());
    return 0;
}


int ListJob() { 
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    while(true) {
        std::vector<baidu::galaxy::JobInformation> infos;
        baidu::common::TPrinter tp(12);
        tp.AddRow(12, "", "id", "name", "state", "stat(r/p/d/e)", "replica", "batch", "cpu", "memory","disk(r/w)","create", "update");
        if (galaxy->ListJobs(&infos)) {
            for (uint32_t i = 0; i < infos.size(); i++) {
                std::vector<std::string> vs;
                vs.push_back(baidu::common::NumToString(i + 1));
                vs.push_back(infos[i].job_id);
                vs.push_back(infos[i].job_name);
                vs.push_back(infos[i].state);
                vs.push_back(baidu::common::NumToString(infos[i].running_num) + "/" + 
                             baidu::common::NumToString(infos[i].pending_num) + "/" +
                             baidu::common::NumToString(infos[i].deploying_num) + "/"+
                             baidu::common::NumToString(infos[i].death_num));
                vs.push_back(baidu::common::NumToString(infos[i].replica));
                            vs.push_back(infos[i].is_batch ? "batch" : "");
                vs.push_back(baidu::common::NumToString(infos[i].cpu_used));
                vs.push_back(baidu::common::HumanReadableString(infos[i].mem_used));
                vs.push_back(baidu::common::HumanReadableString(infos[i].read_bytes_ps) + "/s / " + 
                             baidu::common::HumanReadableString(infos[i].write_bytes_ps)+"/s");
                vs.push_back(FormatDate(infos[i].create_time));
                vs.push_back(FormatDate(infos[i].update_time));
                tp.AddRow(vs);
            }
            printf("%s\n", tp.ToString().c_str());
        }else {
            fprintf(stderr, "List fail\n");
        }
        if(FLAGS_d <= 0) {
            break;
        }else {
            ::sleep(FLAGS_d);
            ::system("clear");
        }


    }
    return 0;
}

int PreemptPod() { 
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    ::baidu::galaxy::PreemptPropose propose;
    int ret = BuildPreemptFromConfig(FLAGS_f, &propose);
    if (ret != 0) {
        fprintf(stderr, "fail to build preempt propose\n");
        return -1;
    }
    bool ok = galaxy->Preempt(propose);
    if (ok) {
        fprintf(stdout, "submit preempt propose successfully\n");
        return 0;
    }
    fprintf(stderr, "fail to submit preempt propose \n");
    return -1;
}

int KillJob() {
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    if (FLAGS_j.empty()) {
        return 1;
    }
    if (galaxy->TerminateJob(FLAGS_j)) {
        printf("terminate job %s successfully\n", FLAGS_j.c_str());
        return 0;
    }
    return 1;
}

int OnlineAgent() {
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    if (FLAGS_e.empty()) {
        fprintf(stderr, "-e is required when online agent\n");
        return -1;
    }
    bool ok = galaxy->OnlineAgent(FLAGS_e);
    if (ok) {
        fprintf(stdout, "online agent %s successfully \n", FLAGS_e.c_str());
        return 0;
    }
    fprintf(stderr, "fail to online agent %s \n", FLAGS_e.c_str());
    return -1;
}

int OfflineAgent() {
    std::string master_key = FLAGS_nexus_root_path + FLAGS_master_path; 
    baidu::galaxy::Galaxy* galaxy = baidu::galaxy::Galaxy::ConnectGalaxy(FLAGS_nexus_servers, master_key);
    if (FLAGS_e.empty()) {
        fprintf(stderr, "-e is required when offline agent\n");
        return -1;
    }
    bool ok = galaxy->OfflineAgent(FLAGS_e);
    if (ok) {
        fprintf(stdout, "offline agent %s successfully \n", FLAGS_e.c_str());
        return 0;
    }
    fprintf(stderr, "fail to offline agent %s \n", FLAGS_e.c_str());
    return -1;
}

int main(int argc, char* argv[]) {
    FLAGS_flagfile = "./galaxy.flag";
    ::google::SetUsageMessage(kGalaxyUsage);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    if(argc < 2){
        fprintf(stderr,"%s", kGalaxyUsage.c_str());
        return -1;
    }
    if (strcmp(argv[1], "submit") == 0) {
        return AddJob();
    } else if (strcmp(argv[1], "jobs") == 0) {
        return ListJob();
    } else if (strcmp(argv[1], "agents") == 0) {
        return ListAgent();
    } else if (strcmp(argv[1], "label") == 0) {
        return LabelAgent();
    } else if (strcmp(argv[1], "update") == 0) {
        return UpdateJob();
    } else if (strcmp(argv[1], "kill") == 0){
        return KillJob();
    } else if (strcmp(argv[1], "pods") ==0){
        return ShowPod();
    } else if (strcmp(argv[1], "tasks") ==0){
        return ShowTask();
    } else if (strcmp(argv[1], "status") == 0) {
        return GetMasterStatus();
    } else if (argc > 2 && strcmp(argv[2], "safemode") == 0) {
        if (strcmp(argv[1], "enter") == 0) 
            return SwitchSafeMode(true);
        else if (strcmp(argv[1], "leave") == 0) 
            return SwitchSafeMode(false);
    } else if (strcmp(argv[1], "attach") == 0) {
        return AttachPod();
    } else if (strcmp(argv[1], "preempt") == 0) {
        return PreemptPod();
    } else if (strcmp(argv[1], "online") == 0) {
        return OnlineAgent();
    } else if (strcmp(argv[1], "offline") == 0) {
        return OfflineAgent();
    } else {
        fprintf(stderr,"%s", kGalaxyUsage.c_str());
        return -1;
    }
    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */

