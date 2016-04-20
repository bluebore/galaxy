/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file src/agent/util/path_tree.cc
 * @author haolifei(com@baidu.com)
 * @date 2016/04/20 19:17:53
 * @brief 
 *  
 **/

#include "path_tree.h"


namespace baidu {
    namespace galaxy {
        namespace path {
            
    const std::string ContainerDir(const std::string& container) {
        return "";
    }

    const std::string CgroupPath(const std::string& subsystem_name,
            const std::string& container_id,
            const std::string& group_id) {
        return "";
    }
    
    const std::string VolumSourcePath(const std::string& disk, const std::string& container_id) {
        return "";
    }

        }
    }
}





















/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
