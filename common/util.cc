// Copyright (c) 2014, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com


#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace common {
namespace util {

static const uint32_t kMaxHostNameSize = 255;
std::string GetLocalHostName() {
    char str[kMaxHostNameSize + 1];
    if (0 != gethostname(str, kMaxHostNameSize + 1)) {
        return "";
    }
    std::string hostname(str);
    return hostname;
}

void CloseOnExec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    flags |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, flags);
}

void GetEnviron(std::map<std::string, std::string>& env) {
    char** cur_environ = environ;

    env.clear();
    for (int index = 0; cur_environ[index] != NULL; index++) {
        // split =
        std::string env_item(cur_environ[index]);
        size_t pos = env_item.find_first_of('=');
        if (pos == std::string::npos) {
            // invalid format
            continue;
        }
        env[env_item.substr(0, pos)] = env_item.substr(pos + 1);
    }
    return;
}

void GetProcessFdList(int pid, std::vector<int>& fds) {
    std::ostringstream stream;
    stream << pid;
    std::string proc_path = "/proc/";
    proc_path.append(stream.str());
    proc_path.append("/fd/");

    DIR* dir = opendir(proc_path.c_str());
    if (dir == NULL) {
        return;
    }

    fds.push_back(0);

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, ".", 1) == 0) {
            continue;
        }
        int fd = atoi(entry->d_name);
        if (fd == 0) {
            continue;
        }
        fds.push_back(fd);
    }
    closedir(dir);
}

}
}


/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
