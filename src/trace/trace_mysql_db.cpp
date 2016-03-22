/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file src/trace/trace_mysql_db.cpp
 * @author haolifei(com@baidu.com)
 * @date 2016/03/09 18:35:38
 * @brief 
 *  
 **/

#include "trace_mysql_db.h"
#include "trace_mysql_translator.h"
#include <iostream>

namespace baidu {
    namespace galaxy {
        namespace trace {
            MysqlDb::MysqlDb(const boost::shared_ptr<Option> op) :
                _op(op),
                _mysql_handl(NULL),
                _translator(NULL),
                _opened(false) {
                assert(NULL != op.get());
            }

            MysqlDb::~MysqlDb() {
                if (_opened) {
                    mysql_close(_mysql_handl);
                    _mysql_handl = NULL;
                }

                if (NULL !=  _translator) {
                    delete  _translator;
                }
            }

            int MysqlDb::Open() {
                baidu::MutexLock lock(&_mutex);
                assert(!_opened);
                if ((_mysql_handl = mysql_init(NULL)) == NULL) {
                    return -1;
                }
                
                if (NULL == mysql_real_connect(_mysql_handl, 
                        _op->Server().c_str(),
                        _op->User().c_str(),
                        _op->PassWd().c_str(),
                        _op->Database().c_str(),
                        _op->Port(),
                        NULL,
                        0
                        )) {

                    std::cout << mysql_error(_mysql_handl) << std::endl;
                    mysql_close(_mysql_handl);
                    return -1;
                }
                 _translator = new MysqlTranslator();
                _opened = true;
                return 0;
            }

            int MysqlDb::Write(boost::shared_ptr<google::protobuf::Message> msg) {
                assert(_opened);
                std::cout << "=================" << _translator << std::endl;
                std::string sql = _translator->translate(msg);
                if (sql.empty()) {
                    return -1;
                }
                
                baidu::MutexLock lock(&_mutex);
                std::cout << "sql:" << sql << std::endl;
                if (0 != mysql_query(_mysql_handl, sql.c_str())) {
                    return -1;
                }
                return 0;
            }

            int MysqlDb::Close() {
                baidu::MutexLock lock(&_mutex);
                if (NULL != _mysql_handl) {
                    mysql_close(_mysql_handl);
                }
                _opened = false;
                return 0;
            }

            int MysqlDb::PendingNum() {
                return 0;
            }

        }
    }
}
