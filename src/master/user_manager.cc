// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "master/user_manager.h"
#include <gflags/gflags.h>
#include <boost/bind.hpp>
#include "logging.h"
#include "timer.h"

DECLARE_string(nexus_servers);
DECLARE_string(nexus_root_path);
DECLARE_string(users_store_path);
DECLARE_int32(master_session_timeout);
DECLARE_int32(master_session_gc_period);

namespace baidu {
namespace galaxy {

UserManager::UserManager(){
    user_set_ = new UserSet();
    nexus_ = new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_servers);
    sessions_ = new Sessions();
    worker_ = new ::baidu::common::ThreadPool(4);
    worker_->DelayTask(FLAGS_master_session_gc_period, 
                       boost::bind(&UserManager::CleanSession, this));
}

UserManager::~UserManager(){}

bool UserManager::AddUser(const User& user) {
    UserIndex user_index;
    user_index.uid = GenUuid(); 
    user_index.name = user.name();
    //TODO md5sum password
    user_index.user.CopyFrom(user); 
    user_index.user.set_create_time(::baidu::common::timer::get_micros());
    user_index.user.set_last_login_time(::baidu::common::timer::get_micros());
    user_index.user.set_uid(user_index.uid);
    {
        MutexLock lock(&mutex_);
        LOG(INFO, "add user %s", user.name().c_str());
        const UserSetNameIndex& name_index = user_set_->get<name_tag>();
        UserSetNameIndex::const_iterator it = name_index.find(user.name());
        if (it != name_index.end()) {
            LOG(WARNING, "user %s does exist", user.name().c_str());
            return false; 
        }
        bool ok =  SaveUser(user_index.user);
        if (!ok) {
            LOG(WARNING, "fail to save user %s", user.name().c_str());
            return false;
        }
    }
    {
        MutexLock lock(&mutex_);
        const UserSetNameIndex& name_index = user_set_->get<name_tag>();
        UserSetNameIndex::const_iterator it = name_index.find(user.name());
        if (it != name_index.end()) {
            LOG(WARNING, "user %s does exist", user.name().c_str());
            return false; 
        } 
        user_set_->insert(user_index);
    }
    return true;
}

void UserManager::CleanSession() {
    MutexLock lock(&mutex_);
    std::set<std::string> timeout_sessions;
    Sessions::iterator it = sessions_->begin();
    for (; it != sessions_->end(); ++it) {
        int64_t now = ::baidu::common::timer::get_micros() / 1000;
        if ((now - it->second.last_update_time/1000) > FLAGS_master_session_timeout) {
            LOG(WARNING, "session %s is timeout", it->first.c_str());
            timeout_sessions.insert(it->first);
        }
    }
    std::set<std::string>::iterator timeout_it = timeout_sessions.begin();
    for (; timeout_it != timeout_sessions.end(); ++timeout_it) {
        sessions_->erase(*timeout_it);
    }
}

bool UserManager::SaveUser(const User& user) {
    std::string key = FLAGS_nexus_root_path + FLAGS_users_store_path +
         "/" + user.uid();
    std::string value;
    user.SerializeToString(&value);
    ::galaxy::ins::sdk::SDKError err;
    bool ok = nexus_->Put(key, value, &err);
    if (!ok) {
        LOG(INFO, "fail to save user %s", user.name().c_str());
        return false;
    }
    return true;
}

bool UserManager::Login(const std::string& name, 
                        const std::string& password,
                        User* user,
                        std::string* sid) {
    MutexLock lock(&mutex_);
    if (user == NULL || sid == NULL) {
        return false;
    }
    const UserSetNameIndex& name_index = user_set_->get<name_tag>();
    UserSetNameIndex::const_iterator it = name_index.find(name);
    if (it != name_index.end()) {
        LOG(INFO, "user %s does exist", name.c_str());
        return false; 
    }
    //TODO md5sum
    if (it->user.password() == password) {
        user->CopyFrom(it->user);
        *sid = GenUuid();
        Session session;
        session.sid = *sid;
        session.last_update_time = ::baidu::common::timer::get_micros();
        session.uid = it->user.uid();
        sessions_->insert(std::make_pair(*sid, session));
        return true;
    }
    return false;
}

std::string UserManager::GenUuid() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::lexical_cast<std::string>(uuid); 
}


}
}
