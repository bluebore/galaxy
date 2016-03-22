/* 
 * File:   db_factory.h
 * Author: haolifei
 *
 * Created on 2016年3月8日, 下午4:55
 */

#pragma once
#include "trace_mysql_db.h"
#include "trace_mysql_db_pool.h"
#include "trace_tera_db.h"

#include <boost/shared_ptr.hpp>

#include <map>
#include <string>

namespace baidu {
    namespace galaxy {
        namespace trace {
            
            class DbFactory {
            public:
                DbFactory();
                ~DbFactory();
                int Init();
                boost::shared_ptr<Db> GetDb(boost::shared_ptr<google::protobuf::Message> msg);
                
            private:
                static DbFactory* s_instance;
                std::map<std::string, boost::shared_ptr<Db> > _m_db;
                boost::shared_ptr<MysqlPool> _mysql;
                boost::shared_ptr<TeraDb> _tera_db;
            };
        }
    }
}
