// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "user.h"

#include <errno.h>
#include <pwd.h>

namespace baidu {
    namespace galaxy {
        namespace user {

            bool Su(const std::string& user_name) {
                uid_t uid;
                gid_t gid;
                if (!GetUidAndGid(user_name, &uid, &gid)) {
                    return false;
                }

                if (::setgid(gid) != 0) {
                    return false;
                }

                if (::setuid(uid) != 0) {
                    return false;
                }
                return true;
            }

            bool GetUidAndGid(const std::string& user_name, uid_t* uid, gid_t* gid) {
                if (user_name.empty() || uid == NULL || gid == NULL) {
                    return false;
                }
                bool rs = false;
                struct passwd user_passd_info;
                struct passwd* user_passd_rs;
                char* user_passd_buf = NULL;
                int user_passd_buf_len = ::sysconf(_SC_GETPW_R_SIZE_MAX);
                for (int i = 0; i < 2; i++) {
                    if (user_passd_buf != NULL) {
                        delete []user_passd_buf;
                        user_passd_buf = NULL;
                    }
                    user_passd_buf = new char[user_passd_buf_len];
                    int ret = ::getpwnam_r(user_name.c_str(), &user_passd_info,
                            user_passd_buf, user_passd_buf_len, &user_passd_rs);
                    if (ret == 0 && user_passd_rs != NULL) {
                        *uid = user_passd_rs->pw_uid;
                        *gid = user_passd_rs->pw_gid;
                        rs = true;
                        break;
                    } else if (errno == ERANGE) {
                        user_passd_buf_len *= 2;
                    }
                    break;
                }
                if (user_passd_buf != NULL) {
                    delete []user_passd_buf;
                    user_passd_buf = NULL;
                }
                return rs;
            }
        }
    }
}
