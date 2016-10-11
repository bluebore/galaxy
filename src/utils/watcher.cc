// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "watcher.h"
#include <glog/logging.h>

namespace baidu {
namespace galaxy {

Watcher::Watcher() : nexus_(NULL) {
}

Watcher::~Watcher()
{
    delete nexus_;
}

bool Watcher::Init(boost::function<void(const std::string& new_endpoint)> handler,
                                        const std::string& nexus_addr,
                                        const std::string& nexus_root,
                                        const std::string& watch_path)
{
    nexus_ =  new InsSDK(nexus_addr);
    nexus_root_ = nexus_root;
    watch_path_ = watch_path;
    handler_ = handler;
    return IssueNewWatch();
}

std::string Watcher::GetEndpoint()
{
    MutexLock lock(&mutex_);
    return endpoint_;
}

void Watcher::EventChange(const ::galaxy::ins::sdk::WatchParam& param,
        ::galaxy::ins::sdk::SDKError /*error*/)
{
    Watcher* watcher = static_cast<Watcher*>(param.context);
    bool ok = watcher->IssueNewWatch();
    if (!ok) {
        LOG(FATAL) << "failed to issue next watch";
        abort();
    }
}

bool Watcher::IssueNewWatch()
{
    std::string watch_path_key = nexus_root_ + watch_path_;
    ::galaxy::ins::sdk::SDKError err;
    bool ok = nexus_->Watch(watch_path_key, &EventChange, this, &err);
    if (!ok) {
        LOG(WARNING) << "fail to watch on nexus, err_code: " << err;
        return false;
    } else {
        LOG(INFO) << "watch success.";
    }
    std::string watch_ep;
    ok = nexus_->Get(watch_path_key, &watch_ep, &err);
    if (!ok) {
        LOG(WARNING) << "fail to get watch endpoint from nexus, err_code: " << err;
        return false;
    } else {
        LOG(INFO) << "watch endpoint is: " << watch_ep;
    }

    std::string old_watch;
    {
        MutexLock lock(&mutex_);
        old_watch = endpoint_;
        endpoint_ = watch_ep;
    }

    if (old_watch != watch_ep) {
        handler_(watch_ep);
    }
    return ok;
}

}
}
