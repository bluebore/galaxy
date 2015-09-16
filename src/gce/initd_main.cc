// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <boost/algorithm/string.hpp>
#include "gce/initd_impl.h"
#include "gflags/gflags.h"
#include "sofa/pbrpc/pbrpc.h"
#include "logging.h"
#include "agent/utils.h"

using baidu::common::Log;
using baidu::common::FATAL;
using baidu::common::INFO;
using baidu::common::WARNING;
const int RPC_START_FAIL = -3;
const int MAX_START_TIMES = 15;

DECLARE_string(gce_initd_dump_file);
DECLARE_string(gce_initd_port);
DECLARE_string(gce_bind_config);

volatile static bool s_is_stop = false;
volatile static bool s_is_restart = false;

void StopSigHandler(int /*sig*/) {
    s_is_stop = true;        
}

void RestartSigHandler(int /*sig*/) {
    s_is_restart = true;
}

bool LoadInitdCheckpoint(baidu::galaxy::InitdImpl* service) {
    if (service == NULL) {
        return false; 
    }

    std::ifstream fin(FLAGS_gce_initd_dump_file.c_str(), std::ios::binary);
    if (!fin.is_open()) {
        LOG(WARNING, "open %s failed",
                FLAGS_gce_initd_dump_file.c_str());
        return false;
    }

    fin.seekg(0, fin.end);
    int len = fin.tellg();
    fin.seekg(0, fin.beg);
    char temp_read_buffer[len];
    fin.read(temp_read_buffer, len);
    fin.close();
    
    std::string pb_buffer(temp_read_buffer, len);
    LOG(INFO, "load initd checkpoint size %lu", pb_buffer.size());

    // TODO delete dump file

    baidu::galaxy::ProcessInfoCheckpoint checkpoint;
    if (!checkpoint.ParseFromString(pb_buffer)) {
        LOG(WARNING, "parse checkpoint failed");     
        return false;
    }
    return service->LoadProcessInfoCheckPoint(checkpoint);
}

bool DumpInitdCheckpoint(baidu::galaxy::InitdImpl* service) {

    if (service == NULL) {
        return false;
    }

    baidu::galaxy::ProcessInfoCheckpoint checkpoint;

    if (!service->DumpProcessInfoCheckPoint(&checkpoint)) {
        LOG(WARNING, "dump checkpoint failed");     
        return false;
    }

    std::string checkpoint_buffer;
    if (!checkpoint.SerializeToString(&checkpoint_buffer)) {
        LOG(WARNING, "serialize to string error");     
        return false;
    }

    std::ofstream ofs(FLAGS_gce_initd_dump_file.c_str(), std::ios::binary);

    if (!ofs.is_open()) {
        LOG(WARNING, "serialize to string error");     
        return false;
    }

    ofs << checkpoint_buffer;
    ofs.close();
    LOG(INFO, "dump initd size %lu", checkpoint_buffer.size());
    return true;
}

bool ReadMountBindConfig(std::vector<std::string>* files) {
    if (files == NULL) {
        return false; 
    }
    std::ifstream ifs(FLAGS_gce_bind_config.c_str(), std::ifstream::in);
    if (ifs.fail()) {
        LOG(WARNING, "open %s failed", FLAGS_gce_bind_config.c_str()); 
        return false;
    }

    while (ifs.good()) {
        std::string line;
        getline(ifs, line);
        boost::algorithm::trim(line);
        if (line.empty()) {
            continue; 
        }
        files->push_back(line);
    }
    if (!ifs.good() && !ifs.eof()) {
        ifs.close(); 
        return false;
    }
    ifs.close();
    return true;
}

bool MountRootfs() {
    if (::getpid() != 1) {
        // TODO
        LOG(WARNING, "no pid_namespace no need bind");
        return true; 
    }
    std::vector<std::string> files;
    if (!ReadMountBindConfig(&files)) {
        LOG(WARNING, "read mount bind config failed");
        return false; 
    }
    std::string cwd;
    ::baidu::galaxy::process::GetCwd(&cwd);

    for (size_t i = 0; i < files.size(); ++i) {
        if (files[i] == "proc") {
            LOG(INFO, "not bind proc");
            continue; 
        }    
        if (files[i] == "." || files[i] == "..") {
            continue; 
        }
        std::string old_mount_path(files[i]);
        bool is_dir = false;
        if (!baidu::galaxy::file::IsDir(old_mount_path, is_dir) 
                || !is_dir) {
            LOG(WARNING, "%s not dir no need bind", 
                    old_mount_path.c_str());
            continue; 
        }
        std::string new_mount_path(cwd);
        new_mount_path.append("/");
        new_mount_path.append(files[i]);
        if (!baidu::galaxy::file::MkdirRecur(new_mount_path)) {
            LOG(WARNING, "mdkir new mount %s failed", 
                    new_mount_path.c_str());
            return false;
        }
    
        if (0 != ::mount(old_mount_path.c_str(), 
                         new_mount_path.c_str(), 
                         "", MS_BIND, "")
                && errno != EBUSY) {
            LOG(WARNING, "mount %s at %s failed", 
                    old_mount_path.c_str(), new_mount_path.c_str());
            return false;
        }
        LOG(INFO, "mount %s at %s success", 
                old_mount_path.c_str(), new_mount_path.c_str());
    }
    
    return true;
}

bool MountProc() {
    pid_t cur_pid = ::getpid();
    if (cur_pid != 1) {
        // NOTE only new PID namespace need mount proc 
        LOG(WARNING, "current pid not init pid, no need mount proc");
        return true;
    }
    std::string proc_path;
    if (!baidu::galaxy::process::GetCwd(&proc_path)) {
        LOG(WARNING, "get cwd failed"); 
        return false;
    }

    proc_path.append("/proc/"); 
    if (!baidu::galaxy::file::Mkdir(proc_path)) {
        LOG(WARNING, "mkdir proc path %s failed", proc_path.c_str()); 
        return false;
    }
    
    if (0 != ::mount("proc", proc_path.c_str(), "proc", 0, "")
            && errno != EBUSY) {
        LOG(WARNING, "mount proc at %s failed", proc_path.c_str()); 
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    // keep argv for restart
    char* restart_argv[argc + 1];
    int restart_argc = argc;
    for (int i = 0; i < restart_argc; i++) {
        restart_argv[i] =  new char[strlen(argv[i]) + 1]; 
        ::strncpy(restart_argv[i], argv[i], strlen(argv[i]));
        restart_argv[i][strlen(argv[i])] = '\0';
    }
    ::google::ParseCommandLineFlags(&argc, &argv, true);

    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);

    if (!MountProc()) {
        return EXIT_FAILURE; 
    }
    if (!MountRootfs()) {
        return EXIT_FAILURE; 
    }

    baidu::galaxy::InitdImpl* initd_service =
                                new baidu::galaxy::InitdImpl();
    if (!initd_service->Init()) {
        LOG(WARNING, "Initd Service Init failed"); 
        return EXIT_FAILURE;
    }

    if (baidu::galaxy::file::IsExists(FLAGS_gce_initd_dump_file)
            && !LoadInitdCheckpoint(initd_service)) {
        LOG(WARNING, "Load Initd Checkpoint failed");     
        return EXIT_FAILURE;
    }

    baidu::galaxy::file::Remove(FLAGS_gce_initd_dump_file);

    if (!rpc_server.RegisterService(initd_service)) {
        LOG(WARNING, "Rpc Server Regist Service failed");
        return EXIT_FAILURE;
    }

    std::string server_host = std::string("0.0.0.0:") 
        + FLAGS_gce_initd_port;
    int start_retry_times = MAX_START_TIMES;    
    bool ret = false;
    // retry when restart
    while (start_retry_times-- > 0) {
        if (!rpc_server.Start(server_host)) {
            LOG(WARNING, "Rpc Server Start failed");
            sleep(1000);
            continue;
        } 
        ret = true;
        break;
    }
    
    if (!ret) {
        return RPC_START_FAIL; 
    }

    signal(SIGTERM, StopSigHandler);
    signal(SIGINT, StopSigHandler);
    signal(SIGUSR1, RestartSigHandler);

    while (!s_is_stop && !s_is_restart) {
        sleep(5); 
    }

    if (s_is_restart) {
        rpc_server.Stop(); 
        if (!DumpInitdCheckpoint(initd_service)) {
            LOG(WARNING, "Dump Initd Checkpoint failed"); 
            return EXIT_FAILURE;
        }
        restart_argv[restart_argc] = NULL;
        ::execvp(restart_argv[0], restart_argv);
        LOG(WARNING, "execvp failed err[%d: %s]",
                errno, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
