// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef _UTILS_H_
#define _UTILS_H_

#include <string>
#include <vector>
#include <boost/function.hpp>

namespace baidu {
namespace galaxy {

void GetStrFTime(std::string* time);

namespace process {

void GetProcessOpenFds(const pid_t pid, 
                       std::vector<int>* fd_vector);

bool PrepareStdFds(const std::string& pwd, 
                   int* stdout_fd, 
                   int* stderr_fd);

void ReplaceEmptyChar(std::string& str);

void PrepareChildProcessEnvStep1(pid_t pid, const char* work_dir);

void PrepareChildProcessEnvStep2(const int stdout_fd, 
                                 const int stderr_fd, 
                                 const std::vector<int>& fd_vector);

} // ending namespace process

namespace file {

typedef boost::function<bool(const char* path)> OptFunc;

bool ListFiles(const std::string& dir,
               std::vector<std::string>* files);

bool Traverse(const std::string& path, const OptFunc& func);

bool IsExists(const std::string& file);

bool IsDir(const std::string& path, bool& is_dir);

bool IsFile(const std::string& path, bool& is_file);

bool IsSpecialDir(const char* path);

bool Mkdir(const std::string& dir_path);

bool Remove(const std::string& path);

}   // ending namespace file
}   // ending namespace galaxy
}   // ending namespace baidu

#endif

