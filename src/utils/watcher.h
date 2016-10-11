// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "ins_sdk.h"

#include <string>
#include <boost/function.hpp>
#include <mutex.h>

namespace baidu {
namespace galaxy {

using ::galaxy::ins::sdk::InsSDK;

class Watcher {
public:
    Watcher();
    ~Watcher();
    bool Init(boost::function<void(const std::string& new_endpoint)> handler,
            const std::string& nexus_addr,
            const std::string& nexus_root,
            const std::string& watch_path);
    std::string GetEndpoint();

private:
    bool IssueNewWatch();
    static void EventChange(const ::galaxy::ins::sdk::WatchParam& param,
            ::galaxy::ins::sdk::SDKError /*error*/);


    InsSDK* nexus_;
    std::string endpoint_;
    Mutex mutex_;
    boost::function<void(const std::string& new_endpoint)> handler_;
    std::string nexus_root_;
    std::string watch_path_;
};

}
}
