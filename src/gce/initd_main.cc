// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

//#include "mutex.h"
#include "thread_pool.h"

volatile bool is_stop = false;
volatile bool is_restart = false;

void StopSigHandler(int /*sig*/) {
    is_stop = true;        
}

void RestartSigHandler(int /*sig*/) {
    is_restart = true;
}

int main (int argc, char* argv[]) {

    signal(SIGTERM, StopSigHandler);
    signal(SIGINT, StopSigHandler);
    signal(SIGUSR1, RestartSigHandler);

    while (!is_stop && !is_restart) {
        sleep(5); 
    }

    return EXIT_SUCCESS;
}
