// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _SRC_AGENT_PERSISTENCE_HANDLER_H
#define _SRC_AGENT_PERSISTENCE_HANDLER_H

#include <vector>
#include <string>

#include "leveldb/db.h"

#include "agent/agent_internal_infos.h"

namespace baidu {
namespace galaxy {

class PersistenceHandler {
public:
    PersistenceHandler() 
        : persistence_path_(), 
          persistence_handler_(NULL) {
    }
    virtual ~PersistenceHandler();
    bool Init(const std::string& persistence_path);
    bool SavePodInfo(const PodInfo& pod_info);
    bool ScanPodInfo(std::vector<PodInfo>* pods);
    bool DeletePodInfo(const std::string& pod_id);
protected:
    std::string persistence_path_;
    leveldb::DB* persistence_handler_;
};

}   // ending namespace galaxy
}   // ending namespace baidu

#endif  //_SRC_AGENT_PERSISTENCE_HANDLER_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
