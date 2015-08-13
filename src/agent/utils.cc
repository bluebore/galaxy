// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#include "agent/utils.h"

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include <set>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>

#include "common/logging.h"


namespace galaxy {

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

namespace file {
static bool GetDirFilesByPrefixInternal(
        const std::string& dir,
        const std::string& prefix,
        unsigned deep,
        unsigned max_deep,
        std::vector<std::string>* files);

bool GetDirFilesByPrefix(
        const std::string& dir,
        const std::string& prefix,
        std::vector<std::string>* files,
        unsigned max_deep) {
    unsigned deep = 0;
    return GetDirFilesByPrefixInternal(dir, 
            prefix, deep, max_deep, files);
}

bool IsSpecialDir(const char* path) {
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

bool Chown(const std::string& path, uid_t uid, gid_t gid) 
{
    if (0 == path.length()) {
        return false;
    }
    return OptForEach(path, boost::bind(lchown, _1, uid, gid));
}

bool Remove(const std::string& path)
{
    if (0 == path.length()) {
        return false;
    }
    return OptForEach(path, boost::bind(remove, _1));
}

bool OptForEach(const std::string& path, const OptFunc& opt)
{
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
            DIR* dir_desc = opendir(cur_path.c_str());
            if (dir_desc == NULL) {
                LOG(WARNING, "open dir %s failed err[%d: %s]",
                        cur_path.c_str(),
                        errno,
                        strerror(errno)); 
                return false;
            }
            bool ret = true;
            struct dirent* dir_entity;
            while ((dir_entity = readdir(dir_desc)) != NULL) {
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
            closedir(dir_desc);
            if (!ret) {
                LOG(WARNING, "opt %s failed err", path.c_str());
                return ret;
            } 
        } 
    }

    return true;
}

bool GetDirFilesByPrefixInternal(
        const std::string& dir,
        const std::string& prefix,
        unsigned deep,
        unsigned max_deep,
        std::vector<std::string>* files) {

    if (max_deep > 0 && deep >= max_deep) {
        LOG(WARNING, "search dir too deep MAX %u has %lu", 
                max_deep, files->size());
        return true; 
    }

    DIR* dir_desc = opendir(dir.c_str());    
    if (dir_desc == NULL) {
        LOG(WARNING, "open dir failed err[%d: %s]",
                errno,
                strerror(errno)); 
        return false;
    }

    struct dirent* dir_entity;
    while ((dir_entity = readdir(dir_desc)) != NULL) {
        LOG(DEBUG, "search %s", dir_entity->d_name);
        if (IsSpecialDir(dir_entity->d_name)) {
            continue; 
        }
        std::string log_file_name(dir_entity->d_name);
        std::string abs_path = dir + "/";
        abs_path.append(log_file_name);
        bool is_dir;
        if (!IsDir(abs_path, is_dir)) {
            continue; 
        }
        if (is_dir) {
            if (!GetDirFilesByPrefixInternal(
                        abs_path, prefix, deep + 1, max_deep, files)) {
                LOG(WARNING, "search dir %s failed", abs_path.c_str()); 
            }
        }

        if (!prefix.empty() 
                && !boost::starts_with(log_file_name,
                    prefix)) {
            continue; 
        }
        files->push_back(abs_path);
    }
    closedir(dir_desc);
    return true;
}

bool IsLink(const std::string& path, bool& is_link) {
    struct stat stat_buf;
    int ret = lstat(path.c_str(), &stat_buf);
    if (ret == -1) {
        LOG(WARNING, "stat path %s failed err[%d: %s]",
                path.c_str(),
                errno,
                strerror(errno)); 
        return false;
    }

    if (S_ISLNK(stat_buf.st_mode)) {
        is_link = true; 
    } else {
        is_link = false; 
    }
    return true;
}

bool IsExists(const std::string& path, bool& is_exists) {
    struct stat stat_buf;
    int ret = ::lstat(path.c_str(), &stat_buf);
    if (ret < 0) {
        is_exists = false; 
    } else {
        is_exists = true; 
    }
    return true;
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
    int ret = lstat(path.c_str(), &stat_buf);
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

}   // ending namespace file
}   // ending namespace galaxy
/* vim: set ts=4 sw=4 sts=4 tw=100 */
