// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H

#include <string>

namespace galaxy {

class Downloader {
public:
    virtual ~Downloader() {}
    virtual int Fetch(const std::string& uri, const std::string& dir) = 0;
    virtual void Stop() = 0;
};

}   // ending namespace galaxy

#endif  //_DOWNLOAD_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
