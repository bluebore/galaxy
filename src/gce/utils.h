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

namespace file {

typedef boost::function<bool(const char* path)> OptFunc;

bool ListFiles(const std::string& dir,
               std::vector<std::string>* files);

bool Traverse(const std::string& path, const OptFunc& func);

}   // ending namespace file
}   // ending namespace galaxy
}   // ending namespace baidu

#endif

