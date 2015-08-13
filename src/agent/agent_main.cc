// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "agent_impl.h"

#include <sofa/pbrpc/pbrpc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <signal.h>
#include <gflags/gflags.h>

#include <string>

#include "proto/agent.pb.h"
#include "agent/resource_collector_engine.h"
#include "agent/dynamic_resource_scheduler.h"
#include "agent/utils.h"
#include "common/util.h"

DECLARE_string(agent_port);
DECLARE_int32(agent_http_port);
DECLARE_string(master_addr);
DECLARE_string(agent_work_dir);
DECLARE_string(container);
DECLARE_string(cgroup_root);
DECLARE_string(task_acct);
DECLARE_double(cpu_num);
DECLARE_int64(mem_gbytes);
DECLARE_int64(mem_bytes);
DECLARE_string(agent_restart_persisten_data);
DECLARE_bool(agent_dynamic_scheduler_switch);

static volatile bool s_quit = false;
static volatile bool s_restart = false;
static void SignalIntHandler(int /*sig*/) {
    s_quit = true;
}

static void RestartSignalIntHandler(int /*sig*/) {
    s_quit = true;
    s_restart = true;
}

static bool LoadPersistenceInfo(galaxy::AgentImpl* agent_service) {
    if (agent_service == NULL) {
        return false; 
    }
    fprintf(stdout, "begin to restart load\n");
    bool persistence_data_exists = false;
    galaxy::file::IsExists(
            FLAGS_agent_restart_persisten_data, 
            persistence_data_exists);
    if (!persistence_data_exists) {
        return false;
    }
    int fd = open(
            FLAGS_agent_restart_persisten_data.c_str(), 
            O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open agent persisten "
                "data failed err[%d: %s] recover normally\n",
                errno, strerror(errno));
        return false;
    }
    long buffer_len = 0;
    int read_len = read(fd, 
            reinterpret_cast<void*>(&buffer_len), 
            sizeof(buffer_len));
    if (read_len == -1 || read_len != sizeof(buffer_len)) {
        fprintf(stderr, "read persistence data "
                "failed err[%d: %s] recover normally\n",
                errno, strerror(errno)); 
        close(fd);
        return false;
    }
    std::string persistence_buffer;
    char* tmp_buffer = new char[buffer_len];
    read_len = read(fd, 
            reinterpret_cast<void*>(tmp_buffer),
            buffer_len);
    if (read_len == -1 ||
            read_len != buffer_len) {
        fprintf(stderr, "read persistence data "
                "failed err[%d: %s] recover normally\n",
                errno, strerror(errno)); 
        close(fd);
        delete[] tmp_buffer;
        return false;
    }
    persistence_buffer.append(tmp_buffer, buffer_len);
    delete[] tmp_buffer;
    close(fd);
    galaxy::AgentServicePersistence service_info;   
    if (!service_info.ParseFromString(persistence_buffer)) {
        fprintf(stderr, "parse agent service "
                "persisten failed\n"); 
        return false;
    }
    
    if (!agent_service->LoadPersistenceInfo(service_info)) {
        fprintf(stderr, "load persisntence failed\n");
        return false;
    }
    fprintf(stdout, "load persistence success\n");
    if (!galaxy::file::Remove(
                FLAGS_agent_restart_persisten_data)) {
        fprintf(stderr, "remove persistence failed\n"); 
        return false;
    }
    return true;
}

static bool DumpPersistenceInfo(galaxy::AgentImpl* agent_service) {
    if (agent_service == NULL) {
        return false; 
    }
    // stop first 
    fprintf(stdout, "agent begin to restart\n");
    if (!agent_service->Stop()) {
        fprintf(stderr, "agent impl stop failed\n");
        return false;
    }
    
    galaxy::AgentServicePersistence service_info;
    if (!agent_service->DumpPersistenceInfo(&service_info)) {
        fprintf(stderr, "agent impl dump failed\n"); 
        return false;
    }

    std::string persistence_buffer;
    if (!service_info.SerializeToString(&persistence_buffer)) {
        fprintf(stderr, "persistence buffer serilize failed\n"); 
        return false;
    }

    int64_t buffer_size = persistence_buffer.size();
    const int OPEN_FLAG = O_WRONLY | O_CREAT;
    const int OPEN_MODE = S_IRWXU;
    int fd = ::open(
            FLAGS_agent_restart_persisten_data.c_str(), 
            OPEN_FLAG, OPEN_MODE);
    if (fd == -1) {
        fprintf(stderr, "open restart persistence "
                "data failed err[%d: %s]",
                errno, strerror(errno)); 
        return false;
    }

    int write_len = write(fd, &buffer_size, sizeof(buffer_size));
    if (write_len == -1) {
        fprintf(stderr, "write buffer len failed err[%d: %s]",
                errno, strerror(errno)); 
        return false;
    }

    write_len = write(fd, 
            persistence_buffer.data(), buffer_size);
    if (write_len == -1) {
        fprintf(stderr, "write buffer data failed err[%d: %s]",
                errno, strerror(errno));    
        return false;
    }
    close(fd);
    fprintf(stdout, "write persisten data success\n");
    return true;
}

const char* GetBuildTime() {
#if defined(__DATE__) && defined(__TIME__)
    const char* build_time = __DATE__" "__TIME__;
#else
    const char* build_time = "unkown build time";
#endif
    return build_time;
}

void Version() {
    fprintf(stdout, "====================\n");    
    fprintf(stdout, "BuildTime: %s\n", GetBuildTime());
    fprintf(stdout, "====================\n");    
}

int main(int argc, char* argv[]) {
    // NOTE gflags parser will change argc, argv
    char* restart_argv[argc + 1];
    int restart_argc = argc;
    for (int i = 0; i < argc; i++) {
        restart_argv[i] = new char[strlen(argv[i]) + 1]; 
        strncpy(restart_argv[i], argv[i], strlen(argv[i]));
        restart_argv[i][strlen(argv[i])] = '\0';
    }
    int ret = ::google::ParseCommandLineFlags(&argc, &argv, true);
    fprintf(stderr, "parse command line flag ret = %d\n", ret);
    Version();
    FLAGS_mem_bytes = FLAGS_mem_gbytes*(1024*1024*1024);
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);

    galaxy::AgentImpl* agent_service = new galaxy::AgentImpl();

    // use for atexit regist
    galaxy::ResourceCollectorEngine* engine = 
        galaxy::GetResourceCollectorEngine();
    engine = engine;
    galaxy::DynamicResourceScheduler* dy_scheduler = 
        galaxy::GetDynamicResourceScheduler();
    dy_scheduler = dy_scheduler;

    // TODO 部分情况可重新走正常启动流程 
    bool restart_pb_exists = false;
    galaxy::file::IsExists(FLAGS_agent_restart_persisten_data, restart_pb_exists);

    if (!restart_pb_exists && !agent_service->InitData()) {
        fprintf(stderr, "agent service init failed\n");
        return EXIT_FAILURE;
    } else if (restart_pb_exists && 
            !LoadPersistenceInfo(agent_service)) {
        return EXIT_FAILURE; 
    } 

    //if (!restart
    //        && !agent_service->InitData()) {
    //    fprintf(stderr, "agent impl init data failed\n");
    //    return EXIT_FAILURE;
    //}
    
    if (!rpc_server.RegisterService(agent_service)) {
        return EXIT_FAILURE;
    }

    std::string server_host = std::string("0.0.0.0:") 
        + FLAGS_agent_port;
    if (!rpc_server.Start(server_host)) {
        return EXIT_FAILURE;
    }

    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    signal(SIGUSR1, RestartSignalIntHandler);
    while (!s_quit) {
        sleep(1);
    }

    if (s_restart) {
        rpc_server.Stop(); 
        if (!DumpPersistenceInfo(agent_service)) {
            return EXIT_FAILURE;
        }
                
        // 防止以后升级句柄问题，重置所有句柄o_cloexec
        //std::vector<int> fds;
        //pid_t current_pid = getpid();
        //common::util::GetProcessFdList(current_pid, fds);
        //for (size_t i = 0; i < fds.size(); i++) {
        //    if (fds[i] == STDOUT_FILENO
        //            || fds[i] == STDERR_FILENO
        //            || fds[i] == STDIN_FILENO) {
        //        // 不处理标准输入输出
        //        continue; 
        //    } 
        //    int flags = 0;
        //    if ((flags = ::fcntl(fds[i], F_GETFD)) == -1) {
        //        fprintf(stderr, "fcntl %d F_GETFD failed err[%d: %s]",
        //                fds[i], errno, strerror(errno));
        //        return EXIT_FAILURE;
        //    }
        //    if (::fcntl(fds[i], F_SETFD, FD_CLOEXEC) == -1) {
        //        fprintf(stderr, "fcntl %d FD_CLOEXEC failed err[%d: %s]",
        //                fds[i], errno, strerror(errno)); 
        //        return EXIT_FAILURE;
        //    }
        //}   
        restart_argv[restart_argc] = NULL;
        execvp(restart_argv[0], restart_argv); 
        fprintf(stderr, "execvp failed err[%d: %s]",
                errno, strerror(errno));
        assert(0);
    }

    return EXIT_SUCCESS;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
