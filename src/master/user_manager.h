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

#include "mutex.h"
#include "ins_sdk.h"

using ::galaxy::ins::sdk::InsSDK;
namespace baidu {
namespace galaxy {

struct id_tag{};

struct name_tag{};

struct UserIndex {
    std::string uid;
    std::string name;
    User user;
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

class UserManager {

public:
    UserManager();
    ~UserManager();
    bool AddUser(const User& user);
    bool Auth(const std::string& name, 
         const std::string& password,
         User* user);
private:
    std::string GenUuid();
    bool SaveUser(const User& user);
private:
    Mutex mutex_;
    UserSet* user_set_;
    InsSDK* nexus_;
};

}
}
#endif


