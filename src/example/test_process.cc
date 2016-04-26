// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/container/process.h"

#include <boost/bind.hpp>

#include <unistd.h>

#include <iostream>

int routine(void* arg) {
    // mount volum
    // chown
    // change root
    // set user
    // run appworker

    while (true) {
        std::cout << "i am running ..." << std::endl;
        std::cerr << "ii am running ..." << std::endl;
        sleep(1);
    } 
    return 0;
}
int main(int argc, char** argv) {
    baidu::galaxy::container::Process process;
    process.AddEnv("key", "value");
    process.SetRunUser("galaxy");
    process.RedirectStderr("stderr");
    process.RedirectStdout("stdout");
    
    if (0 != process.Clone(boost::bind(&routine, _1), NULL, 0)) {
        std::cerr << "clone failed" << std::endl;
    }
    int status = 0;
    std::cout << "parent pid is: " << (int)baidu::galaxy::container::Process::SelfPid()
        << " child pid is " << (int)process.Pid() << std::endl;
    if(0 != process.Wait(status)) {
        std::cout << "wait failed" << std::endl;
    }
    
    return 0;
    
}

