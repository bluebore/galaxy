/* 
 * File:   mysql_db_pool.h
 * Author: haolifei
 *
 * Created on 2016年3月8日, 下午7:12
 */

#pragma once
#include "trace_db.h"
#include "trace_mysql_db.h"

#include <mutex.h>

#include <mutex.h>

#include <vector>

namespace baidu {
    namespace galaxy {
        namespace trace {
            
            class MysqlPool : public Db {
            public:
                MysqlPool(boost::shared_ptr<MysqlDb::Option> op, int pool_size);
                ~MysqlPool();
                
                int Open();
                int Write(boost::shared_ptr<google::protobuf::Message> msg);
                int Close();
                int PendingNum();
               
            private:
                std::vector<boost::shared_ptr<MysqlDb> >  _db_pool;
                boost::shared_ptr<MysqlDb::Option> _op;
                baidu::common::Mutex _mutex;
                int _size;
                int _index;
                bool _opened;
                
            };
        }
    }
}
