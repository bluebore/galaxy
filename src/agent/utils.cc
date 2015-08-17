// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include <set>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "logging.h"

namespace baidu {
namespace galaxy {

std::string GenerateTaskId(const std::string& podid) {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();    
    std::stringstream sm_uuid;
    sm_uuid << uuid;
    std::string str_uuid = podid;
    str_uuid.append("_");
    str_uuid.append(sm_uuid.str());
    return str_uuid;
}

int DownloadByDirectWrite(const std::string& binary, 
                          const std::string& tar_packet) {
    const int WRITE_FLAG = O_CREAT | O_WRONLY;
    const int WRITE_MODE = S_IRUSR | S_IWUSR 
        | S_IRGRP | S_IRWXO;
    int fd = ::open(tar_packet.c_str(), 
            WRITE_FLAG, WRITE_MODE);
    if (fd == -1) {
        LOG(WARNING, "open download "
                "%s file failed err[%d: %s]",
                tar_packet.c_str(),
                errno,
                strerror(errno));     
        return -1;
    }
    int write_len = ::write(fd, 
            binary.c_str(), binary.size());
    if (write_len == -1) {
        LOG(WARNING, "write download "
                "%s file failed err[%d: %s]",
                tar_packet.c_str(),
                errno,
                strerror(errno)); 
        ::close(fd);
        return -1;
    }
    ::close(fd);
    return 0;
}

int RandRange(int min, int max) {
    srand(time(NULL));
    return min + rand() % (max - min + 1);
}

void GetStrFTime(std::string* time_str) {
    const int TIME_BUFFER_LEN = 100;        
    char time_buffer[TIME_BUFFER_LEN];
    time_t seconds = time(NULL);
    struct tm t;
    localtime_r(&seconds, &t);
    size_t len = strftime(time_buffer,
            TIME_BUFFER_LEN - 1,
            "%Y%m%d%H%M%S",
            &t);
    time_buffer[len] = '\0';
    time_str->clear();
    time_str->append(time_buffer, len);
    return ;
}

void ReplaceEmptyChar(std::string& str) {
    size_t index = str.find_first_of(" ");
    while (index != std::string::npos) {
        str.replace(index, 1, "_");
        index = str.find_first_of(" ");
    }
}

namespace process { 

bool PrepareStdFds(const std::string& pwd, 
                   int* stdout_fd, 
                   int* stderr_fd) {
    if (stdout_fd == NULL || stderr_fd == NULL) {
        LOG(WARNING, "prepare stdout_fd or stderr_fd NULL"); 
        return false;
    }
    std::string now_str_time;
    GetStrFTime(&now_str_time);
    pid_t pid = ::getpid();
    std::string str_pid = boost::lexical_cast<std::string>(pid);
    std::string stdout_file = pwd + "/stdout_" + str_pid + "_" + now_str_time;
    std::string stderr_file = pwd + "/stderr_" + str_pid + "_" + now_str_time;

    const int STD_FILE_OPEN_FLAG = O_CREAT | O_APPEND | O_WRONLY;
    const int STD_FILE_OPEN_MODE = S_IRWXU | S_IRWXG | S_IROTH;
    *stdout_fd = ::open(stdout_file.c_str(), 
            STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    *stderr_fd = ::open(stderr_file.c_str(),
            STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    if (*stdout_fd == -1 || *stderr_fd == -1) {
        LOG(WARNING, "open file %s failed err[%d: %s]",
            stdout_file.c_str(), errno, strerror(errno));
        return false; 
    }
    return true;
}

void GetProcessOpenFds(const pid_t pid, 
                       std::vector<int>* fd_vector) {
    if (fd_vector == NULL) {
        return; 
    }

    // pid_t current_pid = ::getpid();
    std::string pid_path = "/proc/";
    pid_path.append(boost::lexical_cast<std::string>(pid));
    pid_path.append("/fd/");

    std::vector<std::string> fd_files;
    if (!file::ListFiles(pid_path, &fd_files)) {
        LOG(WARNING, "list %s failed", pid_path.c_str());    
        return;
    }

    for (size_t i = 0; i < fd_files.size(); i++) {
        if (fd_files[i] == "." || fd_files[i] == "..") {
            continue; 
        }
        fd_vector->push_back(::atoi(fd_files[i].c_str()));    
    }
    return;
}

void PrepareChildProcessEnvStep1(pid_t pid, const char* work_dir) {
    // pid_t my_pid = ::getpid();
    int ret = ::setpgid(pid, pid);
    if (ret != 0) {
        assert(0); 
    }

    ret = ::chdir(work_dir);
    if (ret != 0) {
        assert(0);
    }
}

void PrepareChildProcessEnvStep2(const int stdout_fd, 
                                 const int stderr_fd, 
                                 const std::vector<int>& fd_vector) {
    while (::dup2(stdout_fd, STDOUT_FILENO) == -1 
            && errno == EINTR) {}
    while (::dup2(stderr_fd, STDERR_FILENO) == -1
            && errno == EINTR) {}
    for (size_t i = 0; i < fd_vector.size(); i++) {
        if (fd_vector[i] == STDOUT_FILENO
            || fd_vector[i] == STDERR_FILENO
            || fd_vector[i] == STDIN_FILENO) {
            // not close std fds
            continue; 
        } 
        ::close(fd_vector[i]);
    }
}

} // ending namespace process

namespace file {

bool Traverse(const std::string& path, const OptFunc& opt) {
    bool rs = false;
    if (!IsDir(path, rs)) {
        return false; 
    }
    if (!rs) {
        if (0 != opt(path.c_str())) {
            LOG(WARNING, "opt %s failed err[%d: %s]",
                    path.c_str(),
                    errno,
                    strerror(errno));
            return false;
        }
        return true;
    }

    std::vector<std::string> stack;
    stack.push_back(path);
    std::set<std::string> visited;
    std::string cur_path;
    while (stack.size() != 0) {
        cur_path = stack.back();
        
        bool is_dir;
        if (!IsDir(cur_path, is_dir)) {
            LOG(WARNING, "IsDir %s failed err", path.c_str());
            return false; 
        }

        if (is_dir) {
            if (visited.find(cur_path) != visited.end()) {
                stack.pop_back();
                if (0 != opt(cur_path.c_str())) {
                    LOG(WARNING, "opt %s failed err[%d: %s]",
                            cur_path.c_str(),
                            errno,
                            strerror(errno)); 
                    return false;
                } 
                continue;
            }
            visited.insert(cur_path);
            DIR* dir_desc = ::opendir(cur_path.c_str());
            if (dir_desc == NULL) {
                LOG(WARNING, "open dir %s failed err[%d: %s]",
                        cur_path.c_str(),
                        errno,
                        strerror(errno)); 
                return false;
            }
            bool ret = true;
            struct dirent* dir_entity;
            while ((dir_entity = ::readdir(dir_desc)) != NULL) {
                if (IsSpecialDir(dir_entity->d_name)) {
                    continue; 
                } 
                std::string tmp_path = cur_path + "/";
                tmp_path.append(dir_entity->d_name);
                is_dir = false;
                if (!IsDir(tmp_path, is_dir)) {
                    ret = false;
                    break;
                }
                if (is_dir) {
                    stack.push_back(tmp_path);
                } else {
                    if (opt(tmp_path.c_str()) != 0) { 
                        LOG(WARNING, "opt %s failed err[%d: %s]",
                                tmp_path.c_str(),
                                errno,
                                strerror(errno));
                        ret = false; 
                        break;
                    }
                }
            }
            ::closedir(dir_desc);
            if (!ret) {
                LOG(WARNING, "opt %s failed err", path.c_str());
                return ret;
            } 
        } 
    }

    return true;

}

bool ListFiles(const std::string& dir_path,
        std::vector<std::string>* files) {
    if (files == NULL) {
        return false;
    }

    DIR* dir = ::opendir(dir_path.c_str());

    if (dir == NULL) {
        LOG(WARNING, "opendir %s failed err[%d: %s]",
                dir_path.c_str(),
                errno,
                strerror(errno));
        return false;
    }

    struct dirent* entry;

    while ((entry = ::readdir(dir)) != NULL) {
        files->push_back(entry->d_name);
    }

    closedir(dir);
    return true;
}

bool IsExists(const std::string& path) {
    struct stat stat_buf;
    int ret = ::lstat(path.c_str(), &stat_buf);
    if (ret < 0) {
        return false; 
    }
    return true;
}

bool Mkdir(const std::string& dir_path) {
    const int dir_mode = 0777;
    int ret = ::mkdir(dir_path.c_str(), dir_mode); 
    if (ret == 0 || errno == EEXIST) {
        return true; 
    }
    LOG(WARNING, "mkdir %s failed err[%d: %s]", 
            dir_path.c_str(), errno, strerror(errno));
    return false;
}

bool IsFile(const std::string& path, bool& is_file) {
    struct stat stat_buf;
    int ret = lstat(path.c_str(), &stat_buf);
    if (ret == -1) {
        LOG(WARNING, "stat path %s failed err[%d: %s]",
                path.c_str(),
                errno,
                strerror(errno)); 
        return false;
    }

    if (S_ISREG(stat_buf.st_mode)) {
        is_file = true; 
    } else {
        is_file = false; 
    }
    return true;
}

bool IsDir(const std::string& path, bool& is_dir) {
    struct stat stat_buf;        
    int ret = ::lstat(path.c_str(), &stat_buf);
    if (ret == -1) {
        LOG(WARNING, "stat path %s failed err[%d: %s]",
                path.c_str(),
                errno,
                strerror(errno));    
        return false;
    }

    if (S_ISDIR(stat_buf.st_mode)) {
        is_dir = true; 
    } else {
        is_dir = false; 
    }
    return true;
}

bool IsSpecialDir(const char* path) {
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

bool Remove(const std::string& path) {
    if (path.empty()) {
        return false; 
    }
    return Traverse(path, boost::bind(remove, _1));
}

}   // ending namespace file
}   // ending namespace galaxy
}   // ending namespace baidu
