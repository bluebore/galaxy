// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#include "downloader_manager.h"
#include <boost/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "common/asm_atomic.h"
#include "curl_downloader.h"
#include "binary_downloader.h"

namespace galaxy {

DownloaderManager::DownloaderManager()
    : downloader_process_pool_(NULL),
      downloader_handler_(),
      handler_lock_(),
      next_id_(0) {
    downloader_process_pool_ = new common::ThreadPool(10);
}

bool DownloaderManager::Stop() {
    common::MutexLock lock(&handler_lock_);    
    std::map<int, Downloader*>::iterator it = 
        downloader_handler_.begin();
    for (; it != downloader_handler_.end(); ++it) {
        it->second->Stop(); 
    }

    return downloader_process_pool_->Stop(false);
}

void DownloaderManager::DownloadThreadWrapper(
        int id, 
        std::string uri,
        std::string path,
        boost::function<void (int)> callback) {
    Downloader* downloader = NULL;
    std::map<int, Downloader*>::iterator it;
    {
        common::MutexLock lock(&handler_lock_);
        it = downloader_handler_.find(id);
        assert(it != downloader_handler_.end());
    }
    downloader = it->second;
    int ret = downloader->Fetch(uri, path);
    callback(ret);

    MutexLock lock(&handler_lock_);
    if (it != downloader_handler_.end()) {
        downloader_handler_.erase(it);
        delete downloader;
    }
    return;
}

int DownloaderManager::DownloadInThread(
        const std::string& uri, 
        const std::string& path, 
        boost::function<void (int)> callback) {
    // TODO change id create func
    int cur_id = common::atomic_inc_ret_old(&next_id_);
    Downloader* downloader = NULL;
    if (boost::starts_with(uri.c_str(), "ftp://")) {
        downloader = new CurlDownloader();
    }
    else {
        downloader = new BinaryDownloader(); 
    }
    
    {
        common::MutexLock lock(&handler_lock_);
        downloader_handler_[cur_id] = downloader;
    }
    downloader_process_pool_->AddTask(
            boost::bind(
                &DownloaderManager::DownloadThreadWrapper, 
                this, 
                cur_id, uri, path, callback));
    return cur_id;
}

void DownloaderManager::KillDownload(int id) {
    common::MutexLock lock(&handler_lock_);
    std::map<int, Downloader*>::iterator it;
    // erase downloader in ThreadWrapper
    it = downloader_handler_.find(id);
    if (it != downloader_handler_.end()) {
        it->second->Stop(); 
    }
    return;
}

pthread_once_t DownloaderManager::once_ = PTHREAD_ONCE_INIT;
DownloaderManager* DownloaderManager::instance_ = NULL;

}   // ending namespace galaxy

/* vim: set ts=4 sw=4 sts=4 tw=100 */
