// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#ifndef _AGENT_UTILS_H
#define _AGENT_UTILS_H

#include <string>
#include <vector>
#include <sys/types.h>
#include <boost/function.hpp>

#include "google/protobuf/message.h"

namespace galaxy {

// %Y%m%d%H%M%S
void GetStrFTime(std::string* time_str);

namespace file {

typedef boost::function<bool(const char* path)> OptFunc; 

bool GetDirFilesByPrefix(
        const std::string& dir, 
        const std::string& prefix, 
        std::vector<std::string>* files,
        unsigned max_deep = 1);

bool Remove(const std::string& path); 

bool Chown(const std::string& path, uid_t uid, gid_t gid);

bool OptForEach(const std::string& path, const OptFunc& func);

bool IsDir(const std::string& path, bool& is_dir);

bool IsFile(const std::string& path, bool& is_file);

bool IsLink(const std::string& path, bool& is_link);

void IsExists(const std::string& path, bool& is_exists);

}   // ending namespace galaxy
}   // ending namespace galaxy

#endif  //_AGENT_UTILS_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
