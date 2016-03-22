/* 
 * File:   mysql_db.h
 * Author: haolifei
 *
 * Created on 2016年3月8日, 下午7:02
 */

#pragma once

#include "trace_db.h"
#include <mysql/mysql.h>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <mutex.h>

namespace baidu {
    namespace galaxy {
        namespace trace {
            class MysqlTranslator;
            
            class MysqlDb : public Db{
            public:
                class Option{
                public:
                    const std::string& Server() const {
                        return _server;
                    }
                    
                    int Port() const {
                        return _port;
                    }
                    
                    const std::string& User() const {
                        return _user;
                    }
                    
                    const std::string& PassWd() const {
                        return _passwd;
                    }
                    
                    const std::string& Database() const {
                        return _db;
                    }
                    Option* SetServer(const std::string& server) {
                        _server = server;
                        return this;
                    }
                    
                    Option* SetUser(const std::string& user) {
                        _user = user;
                        return this;
                    }
                    
                    Option* SetPasswd(const std::string& passwd) {
                        _passwd = passwd;
                        return this;
                    }
                    
                    Option* SetDatabase(const std::string& db) {
                        _db = db;
                        return this;
                    }
                    Option* SetPort(const int port) {
                        _port = port;
                        return this;
                    }
                private:
                    std::string _server;
                    std::string _user;
                    std::string _passwd;
                    std::string _db;
                    int _port;
                };
                
            public:
                MysqlDb(const boost::shared_ptr<Option> op);
                ~MysqlDb();
                int Open();
                int Write(boost::shared_ptr<google::protobuf::Message> msg);
                int Close();
                int PendingNum();
                
            private:
                boost::shared_ptr<Option> _op;
                MYSQL* _mysql_handl;
                MysqlTranslator* _translator;
                bool _opened;
                baidu::Mutex _mutex;
            };
        }
    }
}
