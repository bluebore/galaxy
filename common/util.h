// Copyright (c) 2014, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#ifndef  COMMON_UTIL_H_
#define  COMMON_UTIL_H_

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include <map>
#include <sstream>
#include <string>

// use only in linux
extern char** environ;

namespace common {
namespace util {

static const uint32_t kMaxHostNameSize = 255;
std::string GetLocalHostName();
void CloseOnExec(int fd);
void GetEnviron(std::map<std::string, std::string>& env);
void GetProcessFdList(int pid, std::vector<int>& fds);
}
}

#endif  //COMMON_UTIL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
