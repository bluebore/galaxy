/* 
 * File:   newfile.h
 * Author: haolifei
 *
 * Created on 2016年3月8日, 下午4:50
 */

#pragma once

#include <boost/shared_ptr.hpp>

#include <google/protobuf/message.h>

namespace baidu {
    namespace galaxy {
        namespace trace {
            
            class Db {
            public:
                Db() {}
                virtual ~Db() {}
                virtual int Open() = 0;
                virtual int Write(boost::shared_ptr<google::protobuf::Message> msg) = 0;
                virtual int Close() = 0;
                virtual int PendingNum() = 0;
            };
        }
    }
}
