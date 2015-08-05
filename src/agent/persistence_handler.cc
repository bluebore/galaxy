// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#include "agent/persistence_handler.h"
#include "common/logging.h"

namespace galaxy {

PersistenceHandler::PersistenceHandler(
        const std::string& db_path) 
    : db_handler_(NULL), table_name_(db_path) {
}

PersistenceHandler::~PersistenceHandler() {
    if (db_handler_ != NULL) {
        delete db_handler_; 
        db_handler_ = NULL;
    }    
}

int PersistenceHandler::Init() {
    if (db_handler_ != NULL) {
        return 0;
    }

    leveldb::Status status;
    leveldb::Options options;
    options.create_if_missing = true;
    status = leveldb::DB::Open(options, table_name_, &db_handler_);
    if (!status.ok()) {
        LOG(WARNING, "open leveldb %s failed %s", 
                table_name_.c_str(), status.ToString().c_str());     
        return -1;
    }
    return 0;
}

int PersistenceHandler::Read(
        const std::string& key, std::string* value) {
    if (value == NULL 
            || db_handler_ == NULL) {
        return -1; 
    }

    leveldb::Status status;
    status = db_handler_->Get(
            leveldb::ReadOptions(), key, value);
    if (!status.ok()) {
        LOG(WARNING, "read leveldb err, key %s",
                key.c_str()); 
        return -1;
    }
    return 0;
}

int PersistenceHandler::Write(
        const std::string& key, const std::string& value) {
    if (db_handler_ == NULL) {
        return -1; 
    }

    leveldb::Status status;
    status = db_handler_->Put(
            leveldb::WriteOptions(), key, value);
    if (!status.ok()) {
        LOG(WARNING, "write leveldb err, "
                "key %s value %s err %s",
                key.c_str(), 
                value.c_str(), 
                status.ToString().c_str());
        return -1;
    }
    LOG(DEBUG, "write leveldb success %s", key.c_str());
    return 0;
}

int PersistenceHandler::Delete(const std::string& key) {
    if (db_handler_ == NULL) {
        return -1; 
    }    

    leveldb::Status status;
    status = db_handler_->Delete(
            leveldb::WriteOptions(), key);
    if (!status.ok()) {
        LOG(WARNING, "delete leveldb err, "
                "key %s err %s",
                key.c_str(), status.ToString().c_str());
        return -1;
    }
    LOG(DEBUG, "delete leveldb success %s", key.c_str());
    return 0;
}

int PersistenceHandler::Scan(std::vector<PersistenceCell>* cells) {
    if (cells == NULL
            || db_handler_ == NULL) {
        return -1; 
    }

    cells->clear();
    leveldb::Iterator* it = 
        db_handler_->NewIterator(leveldb::ReadOptions());
    it->SeekToFirst();
    while (it->Valid()) {
        PersistenceCell cell;
        cell.key = it->key().ToString();
        cell.value = it->value().ToString();
        cells->push_back(cell);
        it->Next();  
    }

    delete it;
    LOG(DEBUG, "scan leveldb size %u", cells->size());
    return 0;
}


}   // ending namespace galaxy

/* vim: set ts=4 sw=4 sts=4 tw=100 */
