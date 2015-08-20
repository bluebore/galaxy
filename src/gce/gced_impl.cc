// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gced_impl.h"
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include "gflags/gflags.h"
#include "rpc/rpc_client.h"
#include "logging.h"
#include "proto/initd.pb.h"
#include "utils.h"
#include "thread_pool.h"

DECLARE_string(gce_initd_bin);
DECLARE_string(gce_work_dir);
namespace baidu {
namespace galaxy {

GcedImpl::GcedImpl() {
    ::srand(::time(NULL));
    rpc_client_ = new RpcClient();
    // thread_pool_ = new ThreadPool(1);
    // thread_pool_.DelayTask(
    //     10000,
    //     boost::bind(&GcedImpl::TaskStatusCheck, this));
    if (!file::IsExists(FLAGS_gce_work_dir)) {
        bool ret = file::Mkdir(FLAGS_gce_work_dir);
        if (!ret) {
            LOG(WARNING, "fail to create gce work dir %s", FLAGS_gce_work_dir.c_str());
            assert(0);
        }
    }
}

GcedImpl::~GcedImpl() {
    delete rpc_client_;
    // delete thread_pool_;
}

void GcedImpl::CreateCgroup() {
    return;
}

void GcedImpl::LaunchInitd() {
      
}

void GcedImpl::GetProcessStatus() {
    {
    MutexLock lock(&mutex_);
    std::map<std::string, Pod>::iterator it = pods_.begin();
    for (; it != pods_.end(); ++it) {
        if (it->second.state == kPodDeploying) {
            std::stringstream ss("localhost:");
            ss << it->second.port;
            baidu::galaxy::Initd_Stub* initd;
            rpc_client_->GetStub(ss.str(), &initd);
            if (initd == NULL) {
                LOG(WARNING, "get initd service error");
                continue;
            }

            const std::vector<Task>& task_group = 
                it->second.task_group;
            for (size_t i = 0; i < task_group.size(); ++i) {
                std::string key(it->first + task_group[i].key);
                GetProcessStatusRequest get_status_req;
                GetProcessStatusResponse get_status_resp;
                get_status_req.set_key(key);
                rpc_client_->SendRequest(initd, 
                                         &baidu::galaxy::Initd_Stub::GetProcessStatus, 
                                         &get_status_req, 
                                         &get_status_resp, 5, 1);
                if (get_status_resp.status() != kOk) {
                }
            }
        }
    }
    }
}

void GcedImpl::TaskStatusCheck() {
    {
        MutexLock scope_lock(&mutex_);
        std::map<std::string, Pod>::iterator it = pods_.begin();
        for (; it != pods_.end(); ++it) {
        }
    }
}

int GcedImpl::RandRange(int min, int max) {
    return min + rand() % (max - min + 1);
}

void GcedImpl::ConvertToInternalTask(const TaskDescriptor& task_desc, 
                                    Task* task) {
    if (NULL == task) {
        return;
    }

    const Resource& resource = task_desc.requirement();
    // Task task; 
    task->millicores = resource.millicores();
    task->memory = resource.memory();
    task->start_command = task_desc.start_command();
    task->stop_command = task_desc.stop_command();

    for (int i = 0; i < resource.ports_size(); ++i) {
        task->ports.push_back(resource.ports(i));
    }

    for (int i = 0; i < resource.disks_size(); ++i) {
        task->disks.push_back(resource.disks(i));
    }

    for (int i = 0; i < resource.ssds_size(); ++i) {
        task->ssds.push_back(resource.ssds(i));
    }
    // pod->task_group.push_back(task);
    return;
}

void GcedImpl::LaunchPod(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::LaunchPodRequest* request,
                         ::baidu::galaxy::LaunchPodResponse* response,
                         ::google::protobuf::Closure* done) {
    if (!request->has_podid() 
        || !request->has_pod()) {
        LOG(WARNING, "not enough input params"); 
        response->set_status(kInputError);
        done->Run();
        return;
    }
    
    // 1. TODO create cgroup
    // CreateCgroup();

    // 2. TODO clone namespace
     
    // 3. TODO launch
     
    std::string work_dir(FLAGS_gce_work_dir);
    std::string pod_work_dir = work_dir +  "/" + request->podid();
    file::Mkdir(pod_work_dir.c_str());

    LOG(INFO, "initd pod work mkdir %s", pod_work_dir.c_str());


    // TODO
    int port = RandRange(9000, 9999);
    std::stringstream ss;
    ss << FLAGS_gce_initd_bin;
    ss << " --gce_initd_port=";
    ss << port; 
      
    // 1. collect initd fds
    std::vector<int> fd_vector;
    process::GetProcessOpenFds(::getpid(), &fd_vector);

    // 2. prepare std fds for child 
    int stdout_fd = 0;
    int stderr_fd = 0;

    // TODO
    if (!process::PrepareStdFds(pod_work_dir, 
                                &stdout_fd, &stderr_fd)) {
        if (stdout_fd != -1) {
            ::close(stdout_fd); 
        }

        if (stderr_fd != -1) {
            ::close(stderr_fd); 
        }
        LOG(WARNING, "prepare for std file failed"); 
        response->set_status(kUnknown);
        done->Run();
        return;
    }

    pid_t child_pid = ::fork();
    if (child_pid == -1) {
        LOG(WARNING, "fork %s failed err[%d: %s]",
                request->podid().c_str(), errno, strerror(errno)); 
        response->set_status(kUnknown);
        done->Run();
        return;
    } else if (child_pid == 0) {
        // setpgid  & chdir
        pid_t my_pid = ::getpid();
        process::PrepareChildProcessEnvStep1(my_pid, 
                                             pod_work_dir.c_str());

        process::PrepareChildProcessEnvStep2(stdout_fd, 
                                             stderr_fd, 
                                             fd_vector);

        // prepare argv
        char* argv[] = {
            const_cast<char*>("sh"),
            const_cast<char*>("-c"),
            const_cast<char*>(ss.str().c_str()),
            NULL};
        // prepare envs
        
        // TODO
        // char* env[1];
        // env[1] = NULL;
        // for (int i = 0; i < request->envs_size(); i++) {
        //     env[i] = const_cast<char*>(request->envs(i).c_str());     
        // }
        // exec
        ::execve("/bin/sh", argv, NULL);
        assert(0);
    }

    // close child's std fds
    ::close(stdout_fd); 
    ::close(stderr_fd);

    sleep(5);
    std::string addr("127.0.0.1:");
    addr += boost::lexical_cast<std::string>(port);
    baidu::galaxy::Initd_Stub* initd;

    LOG(WARNING, "init address %s", addr.c_str());

    rpc_client_->GetStub(addr, &initd);
    Pod pod;
    pod.id = request->podid();
    LOG(WARNING, "run pod with %d tasks", request->pod().tasks_size());
    for (int i = 0; i < request->pod().tasks_size(); ++i) {
        const TaskDescriptor& task_desc = request->pod().tasks(i);
        Task user_task;
        ConvertToInternalTask(task_desc, &user_task);
        user_task.key = request->podid();
        user_task.key += task_desc.start_command();
        user_task.binary = task_desc.binary();

        // std::string filename(work_dir);
        LOG(INFO, "prepare task workdir %s", pod_work_dir.c_str());
        std::stringstream ss;
        ss << pod_work_dir;
        ss << "/test_";
        ss << i;
        ss << ".tar.gz";

        std::stringstream st;
        st << "test_";
        st << i;
        st << ".tar.gz";
        LOG(INFO, "download binary to file %s binary size %u", ss.str().c_str(), task_desc.binary().size());
        std::ofstream ofs(ss.str().c_str(), std::ios::binary);
        ofs << task_desc.binary();
        ofs.close();

        pod.task_group.push_back(user_task);

        //std::string path(task_desc.binary());
        baidu::galaxy::ExecuteRequest exec_request;
        exec_request.set_key(request->podid() + 
                             user_task.key +  "_getpackage");

        baidu::galaxy::ExecuteResponse exec_response;
        exec_request.set_path(work_dir);
        exec_request.set_commands("tar zxvf " + st.str());
        LOG(INFO, "execute command %s", exec_request.commands().c_str());
        rpc_client_->SendRequest(initd, 
                                 &baidu::galaxy::Initd_Stub::Execute, 
                                 &exec_request, 
                                 &exec_response, 5, 1);
        sleep(5);

        // TODO
        // std::string deploying_command;
        // BuildDeployingCommand(user_task, &deploying_command);
        // exec_request.set_commands(deploying_command);
        exec_request.set_commands(task_desc.start_command());

        LOG(INFO, "start command %s", task_desc.start_command().c_str());
        // exec_request.set_path(".");
        rpc_client_->SendRequest(initd, 
                                 &baidu::galaxy::Initd_Stub::Execute, 
                                 &exec_request, 
                                 &exec_response, 5, 1);
        if (exec_response.status() != kOk) {
            LOG(WARNING, "get package error %d", 
                exec_response.status());
            response->set_status(exec_response.status());
            done->Run();
            return;
        }
        LOG(WARNING, "execute task success");
        pod.state = kPodDeploying;  

        sleep(5);

        exec_request.set_commands(task_desc.start_command());
        rpc_client_->SendRequest(initd, 
                                 &baidu::galaxy::Initd_Stub::Execute, 
                                 &exec_request, 
                                 &exec_response, 5, 1);
        
        // TODO
        // Task get_package_task; 
        // get_package_task.start_command = path;
        // pod.task_group.push_back(get_package_task);
        
        // exec_request.set_key(request->podid() + task_desc.start_command());
        // exec_request.set_commands(task_desc.start_command());
        // exec_request.set_path(work_dir);
        // rpc_client_->SendRequest(initd, 
        //                          &baidu::galaxy::Initd_Stub::Execute, 
        //                          &exec_request, 
        //                          &exec_response, 5, 1);
        // if (exec_response.status() != kOk) {
        //     response->set_status(exec_response.status());
        //     done->Run();
        //     return;
        // }
    }

    pod.port = port;
    const std::string& podid = request->podid();
    LOG(DEBUG, "launch pod id:%s", podid.c_str());
    {
        MutexLock scope_lock(&mutex_);
        pods_[podid] = pod;
    }

    // std::string path(request->pod().tasks().binary());
    done->Run();
    return;

    // Initd_Stub* initd;
    // rpc_client_->GetS
}

void GcedImpl::TerminatePod(::google::protobuf::RpcController* controller,
                            const ::baidu::galaxy::TerminatePodRequest* request, 
                            ::baidu::galaxy::TerminatePodResponse* response,
                            ::google::protobuf::Closure* done) {
    if (!request->has_podid()) {
        LOG(WARNING, "not enough input params"); 
        done->Run(); 
        return;
    }

    done->Run();
    return;
}

void GcedImpl::QueryPods(::google::protobuf::RpcController* controller,
                         const ::baidu::galaxy::QueryPodsRequest* request,
                         ::baidu::galaxy::QueryPodsResponse* response, 
                         ::google::protobuf::Closure* done) {
    
    done->Run(); 
    return;
}

void GcedImpl::BuildDeployingCommand(const Task& task, std::string* deploying_command) {
    if (deploying_command == NULL) {
        return; 
    }

    if (task.binary.empty()) {
        return; 
    }

    deploying_command->clear();
    deploying_command->append("echo \"");
    deploying_command->append(task.binary);
    deploying_command->append("\" > test && chmod u+x test");

    LOG(WARNING, "build deploying %s", deploying_command->c_str());
    return;
}

} // ending namespace galaxy
} // ending namespace baidu
