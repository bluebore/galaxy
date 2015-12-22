// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_GALAXY_USER_MANAGER_H
#define BAIDU_GALAXY_USER_MANAGER_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/unordered_map.hpp>
#include "proto/galaxy.pb.h"
#include "mutex.h"
#include "ins_sdk.h"
#include "thread_pool.h"
#include "master/idx_tag.h"

using ::galaxy::ins::sdk::InsSDK;
namespace baidu {
namespace galaxy {

struct UserIndex {
    std::string uid;
    std::string name;
    User user;
};

struct Session {
    std::string sid;
    int64_t last_update_time;
    std::string uid;
};

struct QuotaIndex {
    std::string qid;
    std::string name;
    std::string target;
    Quota quota;
};

typedef boost::multi_index_container<
    UserIndex,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<
            boost::multi_index::tag<id_tag>,  
            BOOST_MULTI_INDEX_MEMBER(UserIndex , std::string, uid)
        >,
        boost::multi_index::hashed_unique<
            boost::multi_index::tag<name_tag>, 
            BOOST_MULTI_INDEX_MEMBER(UserIndex, std::string, name)
        >
    >
> UserSet;

typedef boost::multi_index::index<UserSet, id_tag>::type UserSetIdIndex;
typedef boost::multi_index::index<UserSet, name_tag>::type UserSetNameIndex;

typedef boost::multi_index_container<
    QuotaIndex,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<
            boost::multi_index::tag<id_tag>,  
            BOOST_MULTI_INDEX_MEMBER(QuotaIndex , std::string, qid)
        >,
        boost::multi_index::hashed_unique<
            boost::multi_index::tag<name_tag>, 
            BOOST_MULTI_INDEX_MEMBER(QuotaIndex, std::string, name)
        >,
        boost::multi_index::hashed_unique<
            boost::multi_index::tag<target_tag>, 
            BOOST_MULTI_INDEX_MEMBER(QuotaIndex, std::string, target)
        >
    >
> QuotaSet;

typedef boost::multi_index::index<QuotaSet, id_tag>::type QuotaIdIndex;
typedef boost::multi_index::index<QuotaSet, name_tag>::type QuotaNameIndex;
typedef boost::multi_index::index<QuotaSet, target_tag>::type QuotaTargetIndex;

class UserManager {

public:
    UserManager();
    ~UserManager();
    bool Init();
    bool GetSuperUser(User* user);
    bool GetUserById(const std::string& uid, User* user);
    bool AddUser(const User& user);
    bool Login(const std::string& name, 
               const std::string& password,
               User* user,
               std::string* sid);
    bool Auth(const std::string& sid, User* user);
    bool GetQuota(const std::string& uid, Quota* quota);
    // assign quota from root
    bool AssignQuota(const std::string& uid, const std::string& op, const Quota& quota);
    bool AcquireQuota(const std::string& uid, int64_t millicores, int64_t memory);
private:
    std::string GenUuid();
    bool SaveUser(const User& user);
    void CleanSession();
    void ReloadUser(const User& user);
    void ReloadQuota(const Quota& quota);
    bool GetQuotaByIndex(const std::string& sid, Quota* quota);
    bool AssignExistQuota(const std::string& uid, const Quota& quota);
    bool AssignNoneExistQuota(const std::string& uid, const Quota& quota);
private:
    ::baidu::common::Mutex mutex_;
    UserSet* user_set_;
    QuotaSet* quota_set_;
    InsSDK* nexus_;
    typedef boost::unordered_map<std::string, Session> Sessions;
    Sessions* sessions_;
    ::baidu::common::ThreadPool* worker_;
    std::string super_uid_;
};

}
}
#endif


