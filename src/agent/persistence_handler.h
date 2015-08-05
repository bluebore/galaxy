// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#ifndef _SRC_AGENT_PERSISTENCE_HANDLER_H
#define _SRC_AGENT_PERSISTENCE_HANDLER_H

#include <string>
#include <vector>
#include "leveldb/db.h"

namespace galaxy {

struct PersistenceCell {
    std::string key;
    std::string value;
};

class PersistenceHandler {
public:
    explicit PersistenceHandler(const std::string& db_path);
    virtual ~PersistenceHandler();

    virtual int Init();

    virtual int Read(const std::string& key, std::string* value);

    virtual int Write(const std::string& key, const std::string& value);

    virtual int Delete(const std::string& key);

    virtual int Scan(std::vector<PersistenceCell>* cells);
protected:
    leveldb::DB* db_handler_;
    std::string table_name_;
};

}   // ending namespace galaxy

#endif  //_SRC_AGENT_PERSISTENCE_HANDLER_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
