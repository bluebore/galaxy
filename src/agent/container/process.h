// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdint.h>

#include <boost/function.hpp>

#include <string>
#include <map>
#include <vector>

#ifndef CLONE_NEWPID        
#define CLONE_NEWPID 0x20000000
#endif

#ifndef CLONE_NEWUTS
#define CLONE_NEWUTS 0x04000000
#endif 


namespace baidu{
namespace galaxy {
namespace container {

class Process {
public:
    Process();
    ~Process();
    static pid_t SelfPid();
    void AddEnv(const std::string& key, const std::string& value);
    int  SetRunUser(const std::string& user);
    
    int RedirectStderr(const std::string& path);
    int RedirectStdout(const std::string& path);
    
    int Clone(boost::function<int (void*)>* routine, int32_t flag);
    int Fork(boost::function<int (void*)>* _routine);
    pid_t Pid();
    
    
private:
    class Context {
    public:
        Context() : self(NULL),
                stdout_fd(-1),
                stderr_fd(-1),
                routine(NULL) {
            
        }
    public:
        Process* self;
        int stdout_fd;
        int stderr_fd;
        boost::function<int (void*)> routine;
        std::vector<int> fds;
    };
    
    static int CloneRoutine(void* self);
    
    int ListFds(pid_t pid, std::vector<int>& fd);
 
    pid_t _pid;
    
    std::string _user;
    std::string _user_group;
    std::string _stderr_path;
    std::string _stdout_path;
    std::map<std::string, std::string> _m_env;
    
};

} //namespace container
} //namespace galaxy
} //namespace baidu
