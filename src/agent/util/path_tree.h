// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * 
 * root/
 * |-- gc_dir
 * `-- work_dir
 *  |-- container1
 *  |   |-- app
 *  |   |-- bin
 *  |   |-- etc
 *  |   `-- home
 *  `-- container2
 */

#pragma once
#include <string>

namespace baidu {
namespace galaxy {
namespace path {
    
    const std::string RootPath();
    const std::string GcDir();
    const std::string WorkDir();
    const std::string AppRootDir();
    
    const std::string ContainerRootPath(const std::string& container_id);
    const std::string CgroupPath(const std::string& subsystem_name,
            const std::string& container_id,
            const std::string& group_id);
    
    const std::string VolumSourcePath(const std::string& disk, const std::string& container_id);
    
    


} //namespace util
} //namespace galaxy
} //namespace path
