// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#include "guarder/guarder_impl.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "gflags/gflags.h"
#include "sofa/pbrpc/pbrpc.h"
#include "proto/guarder.pb.h"
#include "agent/utils.h"

DECLARE_string(guarder_port);
DECLARE_string(guarder_persistence_file);

static volatile bool s_quit = false;
static volatile bool s_restart = false;
static void SignalIntHandler(int /*sig*/) {
    s_quit = true;
}

static void SignalIntRestartHandler(int /*sig*/) {
    s_restart = true;
    s_quit = true;
}

static bool LoadPersistenceInfo(galaxy::GuarderImpl* impl) {
    if (impl == NULL) {
        return false; 
    }

    int64_t buffer_size;
    fprintf(stdout, "start to load persistence info\n");
    int fin = ::open(FLAGS_guarder_persistence_file.c_str(),
            O_RDONLY);
    if (fin == -1) {
        fprintf(stderr, "open persistence file "
                "%s failed err[%d: %s]\n",
                FLAGS_guarder_persistence_file.c_str(),
                errno, strerror(errno)); 
        return false;
    }

    int read_len = ::read(fin, 
            &buffer_size, sizeof(buffer_size)); 
    if (read_len == -1) {
        fprintf(stderr, "read persistence buffer "
                "len failed err[%d: %s]\n",
                errno, strerror(errno)); 
        ::close(fin);
        return false;
    }
    if (read_len != sizeof(buffer_size)) {
        fprintf(stderr, "read persistence buffer "
                "len faild need[%lu] read[%d]\n",
                sizeof(buffer_size), read_len);
        ::close(fin);
        return false;
    }

    read_len = 0;
    char* buffer = new char[buffer_size];
    read_len = ::read(fin, buffer, buffer_size);
    if (read_len == -1) {
        fprintf(stderr, "read persistence buffer "
                "failed err[%d: %s]\n",
                errno, strerror(errno));
        delete[] buffer;
        ::close(fin);
        return false;
    }

    if (read_len != buffer_size) {
        fprintf(stderr, "read persistence buffer "
                "failed need[%ld] read[%d]\n", 
                buffer_size, read_len); 
        delete[] buffer;
        ::close(fin);
        return false;
    }

    std::string persistence_buffer(buffer, buffer_size);
    
    ::close(fin);
    delete[] buffer;

    galaxy::ProcessStatusPersistence info; 
    if (!info.ParseFromString(persistence_buffer)) {
        fprintf(stderr, "parse persistence buffer failed\n"); 
        return false;
    }

    if (!impl->LoadPersistenceInfo(&info)) {
        fprintf(stderr, "load persistence info failed\n"); 
        return false;
    }

    if (!galaxy::file::Remove(
                FLAGS_guarder_persistence_file)) {
        fprintf(stderr, "remove persistence info failed\n"); 
        return false;
    }

    fprintf(stdout, "load persistence info success\n");
    return true;
}

static bool DumpPersistenceInfo(galaxy::GuarderImpl* impl) {
    if (impl == NULL) {
        return false; 
    }
    fprintf(stdout, "start to dump persistence info\n");

    galaxy::ProcessStatusPersistence persistence_info;
    if (!impl->DumpPersistenceInfo(&persistence_info)) {
        fprintf(stderr, 
                "guarder impl persistence info failed\n"); 
        return false;
    }

    std::string persistence_buffer;
    int64_t buffer_size = 0;
    if (!persistence_info.SerializeToString(
                &persistence_buffer)) {
        fprintf(stderr, 
                "guarder impl persistence serialize failed\n"); 
        return false;
    }
    buffer_size = persistence_buffer.size();

    int fout = ::open(FLAGS_guarder_persistence_file.c_str(),
            O_CREAT | O_TRUNC | O_WRONLY,
            S_IRWXU | S_IRWXG | S_IROTH); 
    if (fout == -1) {
        fprintf(stderr, "open persistence file %s failed "
                "err[%d: %s]\n",
                FLAGS_guarder_persistence_file.c_str(),
                errno, strerror(errno));
        return false;
    }

    int write_len = ::write(fout, 
            &buffer_size, sizeof(buffer_size));
    if (write_len == -1) {
        fprintf(stderr, "write persistence %s"
                "len failed err[%d: %s]\n",
                FLAGS_guarder_persistence_file.c_str(),
                errno, strerror(errno));
        ::close(fout);
        return false;
    }

    write_len = ::write(fout, persistence_buffer.c_str(),
            buffer_size);
    if (write_len == -1) {
        fprintf(stderr, "write persistence info "
                "failed err[%d: %s]\n",
                errno, strerror(errno)); 
        ::close(fout);
        return false;
    }

    ::fsync(fout);
    ::close(fout);
    fprintf(stdout, "write persistence info success\n");
    return true;
}

int main(int argc, char* argv[]) {
    char* restart_argv[argc + 1];
    int restart_argc = argc;
    for (int i = 0; i < restart_argc; i++) {
        restart_argv[i] = new char[strlen(argv[i]) + 1];
        strncpy(restart_argv[i], argv[i], strlen(argv[i]));
        restart_argv[i][strlen(argv[i])] = '\0';
    }
    ::google::ParseCommandLineFlags(&argc, &argv, true);

    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);

    galaxy::GuarderImpl* guarder_service = 
        new galaxy::GuarderImpl();

    bool persistence_file_exists;    
    galaxy::file::IsExists(
            FLAGS_guarder_persistence_file.c_str(),
            persistence_file_exists);
    if (persistence_file_exists 
            && !LoadPersistenceInfo(guarder_service)) {
        return EXIT_FAILURE;
    }

    if (!rpc_server.RegisterService(guarder_service)) {
        return EXIT_FAILURE; 
    }

    std::string server_host = std::string("0.0.0.0:") + FLAGS_guarder_port;

    if (!rpc_server.Start(server_host)) {
        return EXIT_FAILURE; 
    }

    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    signal(SIGUSR1, SignalIntRestartHandler);
    while (!s_quit && !s_restart) {
        sleep(1); 
    }

    if (s_restart) {
        rpc_server.Stop(); 
        if (!DumpPersistenceInfo(guarder_service)) {
            return EXIT_FAILURE;
        }
        restart_argv[restart_argc] = NULL;
        execvp(restart_argv[0], restart_argv);
        fprintf(stderr, "execvp failed err[%d: %s]\n",
                errno, strerror(errno));
        assert(0);
    }

    return EXIT_SUCCESS;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
