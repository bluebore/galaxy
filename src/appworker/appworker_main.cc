// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "appworker_impl.h"
#include "src/utils/setting_utils.h"
#include "utils.h"

static volatile bool s_quit = false;
static volatile bool s_upgrade = false;

static void SignalTermHandler(int /*sig*/) {
    s_quit = true;
}

static void SignalHupHandler(int /*sig*/) {
    s_upgrade = true;
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, false);
    google::InitGoogleLogging(argv[0]);
    std::string log_file = "appworker";
    if (baidu::galaxy::file::Mkdir(".appworker")) {
        log_file = ".appworker/appworker";
    }
    baidu::galaxy::SetupLog(log_file);
    google::SetStderrLogging(google::GLOG_INFO);

    signal(SIGTERM, SignalTermHandler);
    signal(SIGHUP, SignalHupHandler);

    baidu::galaxy::AppWorkerImpl* appworker_impl = new baidu::galaxy::AppWorkerImpl();

    // new start or upgrade
    bool is_upgrade = false;
    char* c_upgrade = getenv("UPGRADE");
    if (c_upgrade != NULL) {
        std::string s_upgrade = std::string(c_upgrade);
        if (s_upgrade == "1") {
            is_upgrade = true;
            unsetenv("UPGRADE");
        }
    }
    appworker_impl->Init(is_upgrade);
    appworker_impl->StartLoops();

    while (true) {
        if (s_quit) {
            appworker_impl->Quit();
        }

        if (s_upgrade && 0 == setenv("UPGRADE", "1", 1)) {
            LOG(WARNING) << "appworker catch sig HUP, begin to upgrade";

            if (!baidu::galaxy::file::IsExists(argv[0])) {
                LOG(WARNING) << "appworker binary not exist";
                s_upgrade = false;
            } else {
                appworker_impl->PauseLoops();
                if (appworker_impl->Dump()) {
                    ::execve(argv[0], argv, environ);
                    // if execve fail, give up upgrade
                    s_upgrade = false;
                    appworker_impl->StartLoops();
                }
            }
        }

        sleep(1);
    }

    return 0;
}

