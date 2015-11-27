// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "master/user_manager.h"
#include <gflags/gflags.h>
#include "logging.h"

DECLARE_string(nexus_servers);
DECLARE_string(nexus_root_path);
DECLARE_string(users_store_path);
namespace baidu {
namespace galaxy {

UserManager::UserManager(){
    user_set_ = new UserSet();
    nexus_ = new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_servers);
}

UserManager::~UserManager(){}

UserManager::Add(const User& user) {
    UserIndex user_index;
    user_index.uid = GenUuid(); 
    user_index.name = user.name();
    //TODO md5sum password
    user_index.user.CopyFrom(user); 
    user_index.user.set_uid(user_index.uid);
    {
        MutexLock lock(&mutex);
        LOG(INFO, "add user %s", user.name().c_str());
        const UserSetNameIndex& name_index = user_set_->get<name_tag>();
        UserSetIdIndex::const_iterator it = name_index.find(user.name());
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
        MutexLock lock(&mutex);
        const UserSetNameIndex& name_index = user_set_->get<name_tag>();
        UserSetIdIndex::const_iterator it = name_index.find(user.name());
        if (it != name_index.end()) {
            LOG(WARNING, "user %s does exist", user.name().c_str());
            return false; 
        } 
        user_set_->insert(user_index);
    }
    return true;
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

bool UserManager::Auth(const std::string& name, 
                       const std::string& password,
                       User* user) {
    MutexLock lock(&mutex);
    if (user == NULL) {
        return false;
    }
    const UserSetNameIndex& name_index = user_set_->get<name_tag>();
    UserSetNameIndex::const_iterator it = name_index.find(user.name());
    if (it != name_index.end()) {
        LOG(INFO, "user %s does exist", user.name().c_str());
        return false; 
    }
    //TODO md5sum
    if (it->user.password() == password) {
        user->CopyFrom(it->user);
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
