// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/persistence_handler.h"
#include "logging.h"
#include "proto/agent.pb.h"

namespace baidu {
namespace galaxy {

PersistenceHandler::~PersistenceHandler() {
    if (persistence_handler_ != NULL) {
        delete persistence_handler_; 
        persistence_handler_ = NULL;
    }
}

bool PersistenceHandler::Init(const std::string& persistence_path) {
    persistence_path_ = persistence_path;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(
            options, persistence_path_, &persistence_handler_);
    if (!status.ok()) {
        LOG(FATAL, "open leveldb at %s failed for %s",
                persistence_path_.c_str(), 
                status.ToString().c_str()); 
        return false;
    }
    return true;
}

bool PersistenceHandler::SavePodInfo(const PodInfo& pod_info) {
    if (persistence_handler_ == NULL) {
        LOG(WARNING, "persistence handler not inited yet"); 
        return false;
    }
    PodInfoPersistence persistence;
    persistence.set_pod_id(pod_info.pod_id);
    persistence.set_job_id(pod_info.job_id);
    persistence.mutable_pod_desc()->CopyFrom(pod_info.pod_desc);
    persistence.set_initd_port(pod_info.initd_port);
    persistence.set_initd_pid(pod_info.initd_pid);
    std::map<std::string, TaskInfo>::const_iterator it = pod_info.tasks.begin();
    for (; it != pod_info.tasks.end(); ++it) {
        const TaskInfo& task = it->second;
        TaskInfoPersistence* persistence_data 
                        = persistence.add_tasks(); 
        persistence_data->set_task_id(task.task_id);
        persistence_data->mutable_task_desc()->CopyFrom(task.desc);
    }
    persistence.set_job_name(pod_info.job_name);
    
    std::string key = pod_info.pod_id;
    std::string value;
    if (!persistence.SerializeToString(&value)) {
        LOG(WARNING, "persistence for pod %s serialize failed",
                key.c_str()); 
        return false;
    }


    leveldb::WriteOptions ops;
    ops.sync = true;
    leveldb::Status write_status = 
        persistence_handler_->Put(ops, key, value);
    if (!write_status.ok()) {
        LOG(WARNING, "persistence for pod %s write failed %s",
                     key.c_str(),
                     write_status.ToString().c_str());
        return false;
    }
    return true;
}

bool PersistenceHandler::ScanPodInfo(std::vector<PodInfo>* pods) {
    if (persistence_handler_ == NULL) {
        LOG(WARNING, "persistence handler not inited yet"); 
        return false;
    }
    if (pods == NULL) {
        LOG(WARNING, "invalid input error");
        return false;
    }

    leveldb::Iterator* it = 
        persistence_handler_->NewIterator(leveldb::ReadOptions());
    it->SeekToFirst();
    bool ret = true;
    while (it->Valid()) {
        std::string key = it->key().ToString(); 
        std::string value = it->value().ToString();
        PodInfoPersistence persistence;   
        if (!persistence.ParseFromString(value)) {
            LOG(WARNING, "scan %s parse from string failed",
                         key.c_str()); 
            ret = false;
            break;
        }
        PodInfo pod;  
        pod.pod_id = persistence.pod_id();
        pod.job_id = persistence.job_id();
        pod.pod_desc.CopyFrom(persistence.pod_desc());
        pod.initd_port = persistence.initd_port();
        pod.initd_pid = persistence.initd_pid();
        for (int i = 0; i < persistence.tasks_size(); ++i) {
            TaskInfo task_info;     
            task_info.task_id = persistence.tasks(i).task_id();
            task_info.desc.CopyFrom(persistence.tasks(i).task_desc());
            pod.tasks[task_info.task_id] = task_info;
        }
        pod.job_name = persistence.job_name();
        pods->push_back(pod);
        it->Next();
    }

    delete it;
    return ret;
}

bool PersistenceHandler::DeletePodInfo(const std::string& pod_id) {
    if (persistence_handler_ == NULL) {
        LOG(WARNING, "persistence handler not inited yet"); 
        return false;
    }
    std::string key = pod_id; 
    leveldb::WriteOptions ops;
    ops.sync = true;
    leveldb::Status status = 
        persistence_handler_->Delete(ops, key);
    if (!status.ok()) {
        LOG(WARNING, "delete pod %s failed %s", 
                     key.c_str(), 
                     status.ToString().c_str()); 
        return false;
    }
    return true;
}


}   // ending namespace galaxy
}   // ending namespace baidu

/* vim: set ts=4 sw=4 sts=4 tw=100 */
