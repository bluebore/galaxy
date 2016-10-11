// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_GALAXY_APPWORKER_UTILS_H
#define BAIDU_GALAXY_APPWORKER_UTILS_H

#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <vector>

#include <boost/function.hpp>

namespace baidu {
namespace galaxy {

void GetStrFTime(std::string* time);
int RandRange(int min, int max);
int DownloadByDirectWrite(const std::string& binary,
                          const std::string& write_file);
std::string GenerateTaskId(const std::string& pod_id);

namespace process {

void GetProcessOpenFds(const pid_t pid,
                       std::vector<int>* fd_vector);
bool PrepareStdFds(const std::string& pwd,
                   const std::string& process_id,
                   int* stdout_fd,
                   int* stderr_fd);
void ReplaceEmptyChar(std::string& str);
void PrepareChildProcessEnvStep1(pid_t pid, const char* work_dir);
void PrepareChildProcessEnvStep2(const int stdin_fd,
                                 const int stdout_fd,
                                 const int stderr_fd,
                                 const std::vector<int>& fd_vector);
bool GetCwd(std::string* dir);

} // ending namespace process

namespace user {

bool GetUidAndGid(const std::string& user_name, uid_t* uid, gid_t* gid);
bool Su(const std::string& user_name);

} // ending namespace user

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
bool MkdirRecur(const std::string& dir_path);
bool Remove(const std::string& path);
bool Chown(const std::string& path, uid_t uid, gid_t gid);
bool SymbolLink(const std::string& old_path, const std::string& new_path);
bool GetDeviceMajorNumberByPath(const std::string& path, int32_t& major_number);
bool Write(const std::string& path, const std::string& content);

} // ending namespace file

namespace md5 {

std::string Md5(std::string dat);
std::string Md5(const void* dat, size_t len);
std::string Md5File(const char* filename);
std::string Md5File(FILE* file);
std::string Md5Sum6(std::string dat);
std::string Md5Sum6(const void* dat, size_t len);

} // ending namespace md5

namespace net {

bool IsPortOpen(int32_t port);

} // ending namespace net

} // ending namespace galaxy
} // ending namespace baidu

#endif // BAIDU_GALAXY_APPWORKER_UTILS_H
