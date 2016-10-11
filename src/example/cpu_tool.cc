// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "thread.h"

#include <iostream>
#include <vector>


void* Consum(void*) {
    double x = 3.1415926;

    while (true) {
        x *= x;
    }

    return NULL;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " core_num" << std::endl;
        return -1;
    }

    int cnt = atoi(argv[1]);
    std::vector<baidu::common::Thread*> vthread;

    for (int i = 0; i < cnt; i++) {
        baidu::common::Thread* th(new baidu::common::Thread);
        th->Start(Consum, NULL);
        vthread.push_back(th);
    }

    for (size_t i = 0; i < vthread.size(); i++) {
        vthread[i]->Join();
    }

    return 0;
}

