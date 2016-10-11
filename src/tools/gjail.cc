/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file prison_breaker.cc
 * @author haolifei(com@baidu.com)
 * @date 2016/05/31 13:41:12
 * @brief 
 *  
 **/

#include "util/input_stream_file.h"
#include "gflags/gflags.h"
#include <boost/filesystem/path.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include <sys/mount.h>

#include <iostream>
#include <vector>
#include <string>


#ifndef setns
#define setns(fd) syscall(308, 0, fd)
#endif

DEFINE_string(w, "/home/galaxy3/agent/work_dir", "");
DEFINE_string(i, "", "");


baidu::galaxy::util::ErrorCode GetProcessInfo(const std::string& root_path, int& pid);
baidu::galaxy::util::ErrorCode ListContainer();
baidu::galaxy::util::ErrorCode GotoJail();
void PrintHelp(const std::string& arg0);

struct ContainerInfo {
    std::string id;
    std::string user;
    std::string create_time;
};

baidu::galaxy::util::ErrorCode GetContainerInfo(std::vector<ContainerInfo>& info);

int main(int argc, char *argv[])
{

    if (argc < 2) {
        PrintHelp(argv[0]);
        return (-1);
    }

    google::ParseCommandLineFlags(&argc, &argv, true);

    std::string action = argv[1];
    baidu::galaxy::util::ErrorCode ec = ERRORCODE_OK;
    if ("ls" == action) {
        ec = ListContainer();
    } else if ("jail" == action) {
        ec = GotoJail();
    } else if ("-h" == action) {
        PrintHelp(argv[0]);
    } else {
        ec = ERRORCODE(-1, "bad parameter: %s", action.c_str());
    }

    if (ec.Code() != 0) {
        std::cerr << ec.Message() << std::endl; 
        return (-1);
    }
    return (0);
}


baidu::galaxy::util::ErrorCode GetProcessInfo(const std::string& path, int& pid) {
    const std::string property = path + "/container.property";
    boost::system::error_code ec;
    if (!boost::filesystem::exists(property, ec)) {
        return ERRORCODE(-1, "%s donot exist", property.c_str());
    }

    baidu::galaxy::file::InputStreamFile file(property);
    if (!file.IsOpen()) {
        return ERRORCODE(-1, "open %s failed", property.c_str());
    }

    std::string line;
    while (!file.Eof()) {
        file.ReadLine(line);
        if (line.size() < 5) {
            continue;
        }
        if (boost::starts_with(line, "appworker :")) {
            std::vector<std::string> v;
            boost::split(v, line, boost::is_any_of(":"));
            if (v.size() != 2) {
                return ERRORCODE(-1, "bad format: %s", property.c_str());
            }
            pid = atoi(v[1].c_str());
            return ERRORCODE_OK;
        }
    }
    return ERRORCODE(-1, "donot fine appworker");
}

baidu::galaxy::util::ErrorCode ListContainer() {
    if (FLAGS_w.empty()) {
        return ERRORCODE(-1, "'-w workspace' is needed");
    }

    std::vector<ContainerInfo> cis;
    baidu::galaxy::util::ErrorCode ec = GetContainerInfo(cis);
    if (ec.Code() != 0) {
        return ERRORCODE(-1, "get container info failed: %s", ec.Message().c_str());
    }

    std::cout << "total: " << cis.size() << std::endl;
    for (size_t i = 0; i < cis.size(); i++) {
        std::cout << cis[i].id << std::endl;
    }
    return ERRORCODE_OK;
}

baidu::galaxy::util::ErrorCode GotoJail() {
    if (FLAGS_i.empty()) {
        return ERRORCODE(-1, "'-i container_id' is needed");
    }

    if (FLAGS_i.empty()) {
        return ERRORCODE(-1, "'-w workspace' is needed");
    }

    std::string container_id = FLAGS_i;
    std::string work_dir = FLAGS_w;
    const std::string root_path = work_dir + "/" + container_id;
    int appworker_pid = -1;
    baidu::galaxy::util::ErrorCode ec = GetProcessInfo(root_path, appworker_pid);
    if (0 != ec.Code()) {
        return ERRORCODE(-1, "get container info failed: %s", ec.Message().c_str());
    }

    int fd;
    char buf[1024] = {0};
    snprintf(buf, sizeof buf, "/proc/%d/ns/pid", appworker_pid);
    fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return ERRORCODE(-1, "open %s failed", buf);
    }

    if (setns(fd) == -1) {
        return ERRORCODE(-1, "setns(%s) failed", buf);
    }

    snprintf(buf, sizeof buf, "/proc/%d/ns/mnt", appworker_pid);
    fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return ERRORCODE(-1, "open %s failed", buf);
    }

    if (setns(fd) == -1) {
        return ERRORCODE(-1, "setns(%s) failed", buf);
    }


    const char* mounts[] = {"/proc", NULL};
    int i = 0;
    const char* x = mounts[i];
    while (x != NULL) {
        if (strcmp(mounts[i], "/proc") != 0) {
            std::string target = root_path + mounts[i];
            mount(mounts[i], target.c_str(), "", MS_BIND, NULL);
        } else {
            std::string target = root_path + mounts[i];
            mount("proc", target.c_str(), "proc", 0, NULL);
        }
        i++;
        x = mounts[i];
    }

    if (0 != chroot(root_path.c_str())) {
        return ERRORCODE(-1, "chroot %s failed", root_path.c_str());
    }


    if (0 != chdir("/")) {
        return ERRORCODE(-1, "chdir failed");
    }

    pid_t pid = fork();

    if (pid == 0) {
        const char* cmd[] = {"sh", "-c", "/bin/sh -l", NULL};
        execv("/bin/sh", (char**)cmd);
        perror("sh -c /bin/sh -l failed");
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("mb");
    }
    return ERRORCODE_OK;
}

void PrintHelp(const std::string& arg0) {
    std::cerr << "usage: \n"
        << arg0 << " ls [-w workspace_path]\n"
        << arg0 << " jail -i container_id [-w workspace_path]\n";
}


baidu::galaxy::util::ErrorCode GetContainerInfo(std::vector<ContainerInfo>& info) {
    boost::filesystem::path path(FLAGS_w);
    boost::system::error_code ec;

    if (!boost::filesystem::exists(path, ec)) {
        return ERRORCODE(-1, "%s donot exist", FLAGS_i.c_str());
    }

    boost::filesystem::directory_iterator begin(path);
    boost::filesystem::directory_iterator end;

    for (boost::filesystem::directory_iterator iter = begin; iter != end; iter++) {
        std::string file_name = iter->path().filename().string();
        boost::filesystem::path property(path/file_name/"container.property");

        if (!boost::filesystem::exists(property, ec)) {
            continue;
        }
        
        ContainerInfo ci;
        ci.id = file_name;
        info.push_back(ci);
    }
    return ERRORCODE_OK;
}
