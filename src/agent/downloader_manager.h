// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#ifndef _DOWNLOADER_MANAGER_H
#define _DOWNLOADER_MANAGER_H

#include <boost/function.hpp>
#include <map>
#include <pthread.h>
#include "agent/downloader.h"
#include "common/thread_pool.h"

namespace galaxy {

class DownloaderManager {
public:
    ~DownloaderManager() {}
    int DownloadInThread(
            const std::string& uri, 
            const std::string& path, 
            boost::function<void (int)> callback);

    void KillDownload(int id);
    bool Stop();
private:
    static DownloaderManager* instance_;
    static pthread_once_t once_;
public:
    static DownloaderManager* GetInstance() {
        pthread_once(&once_, Init);     
        return instance_;
    }

private:
    void DownloadThreadWrapper(
            int id,
            std::string uri,
            std::string path,
            boost::function<void (int)> callback);

    common::ThreadPool* downloader_process_pool_;
    std::map<int, Downloader*> downloader_handler_; 
    common::Mutex handler_lock_;
    volatile int next_id_;

    DownloaderManager();
    static void Init() {
        instance_ = new DownloaderManager(); 
        ::atexit(Destory);
    }
    static void Destory() {
        delete instance_;
    }
};

}   // ending namespace galaxy

#endif  //_DOWNLOADER_MANAGER_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
