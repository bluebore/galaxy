/* 
 * File:   tera_db.h
 * Author: haolifei
 *
 * Created on 2016年3月8日, 下午7:04
 */
#pragma once

#include "trace_db.h"

namespace baidu {
    namespace galaxy {
        namespace trace {
            class TeraDb : public Db {
                public:
                    TeraDb();
                    ~TeraDb();
                    int Open();
                    int Write(boost::shared_ptr<google::protobuf::Message> msg);
                    int Close();
                    int PendingNum();
            };
        }
    }
}
