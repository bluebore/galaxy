/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file src/trace/trace_tera_db.cpp
 * @author haolifei(com@baidu.com)
 * @date 2016/03/09 17:20:20
 * @brief 
 *  
 **/

#include "trace_tera_db.h"

namespace baidu {
    namespace galaxy {
        namespace trace {
            TeraDb::TeraDb() {
            }

            TeraDb::~TeraDb() {
            }

            int TeraDb::Open() {
                return 0;
            }

            int TeraDb::Write(boost::shared_ptr<google::protobuf::Message> msg) {
                return 0;
            }


            int TeraDb::Close() {
                return 0;
            }


            int TeraDb::PendingNum() {
                return 0;
            }
        }
    }
}





















/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
