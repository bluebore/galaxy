// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gce/utils.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include "logging.h"

namespace baidu {
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

}   // ending namespace file
}   // ending namespace galaxy
}   // ending namespace baidu
