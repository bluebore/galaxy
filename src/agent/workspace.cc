/*
 *
 * workspace.cc
 * Copyright (C) 2015 wangtaize <wangtaize@baidu.com>
 *
 * Distributed under terms of the MIT license.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>
#include <unistd.h>
#include "agent/workspace.h"
#include "common/logging.h"

namespace galaxy{

int DefaultWorkspace::Create(){
    LOG(INFO,"create workspace for task %d",m_task_info.task_id());
    if(m_has_created){
        return 0;
    }
    m_mutex->Lock();
    if(m_has_created){
        m_mutex->Unlock();
        return 0;
    }
    //TODO safe path join
    std::stringstream ss;
    ss << m_root_path << "/" << m_task_info.task_id();
    m_task_root_path = ss.str();
    int status ;
    status = mkdir(m_task_root_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(status == 0 ){
        //TODO move to fetcher,safe path join
        std::string task_path = m_task_root_path +"/"+ m_task_info.task_name();
        int fd = open(task_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);
        if (fd < 0) {
            LOG(WARNING, "Open %s sor write fail", task_path.c_str());
            return -1;
        }
        int len = write(fd, m_task_info.task_raw().data(), m_task_info.task_raw().size());
        if (len < 0) {
            LOG(WARNING, "Write fail : %s", strerror(errno));
        }
        LOG(INFO, "Write %d bytes to %s", len, task_path.c_str());
        close(fd);

        m_has_created = true;
    }
    m_mutex->Unlock();
    return status;
}

std::string DefaultWorkspace::GetPath(){
    return m_task_root_path;
}

int DefaultWorkspace::Clean(){
    if(!m_has_created){
        return 0;
    }
    m_mutex->Lock();
    if(!m_has_created){
        m_mutex->Unlock();
        return 0;
    }
    struct stat st;
    int ret = 0;
    if (stat(m_task_root_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)){
        std::string rm_cmd ;
        rm_cmd = "rm -rf " + m_task_root_path;
        ret = system(rm_cmd.c_str());
        if (ret == 0 ){
            m_has_created = false;
        }
    }
    m_mutex->Unlock();
    return ret;
}
bool DefaultWorkspace::IsExpire(){
    return false;
}
}



