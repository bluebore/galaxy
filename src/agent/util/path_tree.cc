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
#include "boost/filesystem/path.hpp"


namespace baidu {
    namespace galaxy {
        namespace path {

            const std::string ContainerRootPath(const std::string& contaier_id) {
                return "";
            }

            const std::string CgroupPath(const std::string& subsystem_name,
                    const std::string& container_id,
                    const std::string& group_id) {
                boost::filesystem::path path("galaxy");
                path.append(subsystem_name);
                path.append(group_id);

                return path.string();
            }

            const std::string VolumSourcePath(const std::string& disk, const std::string& container_id) {
                return "";
            }

        }
    }
}
