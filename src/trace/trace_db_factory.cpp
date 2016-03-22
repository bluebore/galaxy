#include "trace_db_factory.h"
#include "trace_mysql_db_pool.h"
#include <assert.h>
#include <iostream>

namespace baidu {
    namespace galaxy {
        namespace trace {
            DbFactory* DbFactory::s_instance = NULL;

            DbFactory::DbFactory() {
                assert(NULL == s_instance);
                s_instance = this;
            }

            DbFactory::~DbFactory() {

            }

            int DbFactory::Init() {
                assert(NULL == _mysql.get());
                assert(NULL == _tera_db.get());

                boost::shared_ptr<MysqlDb::Option> op(new MysqlDb::Option());
                op->SetServer("127.0.0.1")
                    ->SetUser("root")
                    ->SetPasswd("galaxy")
                    ->SetPort(8306)
                    ->SetDatabase("lumia");
                _mysql.reset(new MysqlPool(op, 10));
                _tera_db.reset(new TeraDb());

                if (0 != _mysql->Open() || 0 != _tera_db->Open()) {
                    _mysql.reset();
                    _tera_db.reset();
                    return -1;
                } 
                
                _m_db["baidu.galaxy.trace.TracePod"] =  _mysql;
                _m_db["baidu.galaxy.trace.TraceJob"] = _mysql;
                _m_db["baidu.galaxy.trace.TracePodHistory"] = _mysql;
                _m_db["baidu.galaxy.trace.TraceJobMeta"] = _mysql;
                _m_db["baidu.galaxy.trace.TraceJobHistory"] = _mysql;
                _m_db["baidu.galaxy.trace.TraceClusterHistory"] = _mysql;
                _m_db["baidu.galaxy.trace.TraceCluster"] = _mysql;
                _m_db["baidu.galaxy.trace.TraceAgentError"] = _mysql;
                _m_db["baidu.galaxy.trace.TraceAgent"] = _mysql;
                return 0;
            }

            boost::shared_ptr<Db> DbFactory::GetDb(boost::shared_ptr<google::protobuf::Message> msg) {
                boost::shared_ptr<Db> ret;
                std::map<std::string, boost::shared_ptr<Db> >::iterator iter = _m_db.find(msg->GetDescriptor()->full_name());
                if (_m_db.end() != iter) {
                    ret = iter->second;
                }
                
                return ret;
            }
            
        }
    }
}
