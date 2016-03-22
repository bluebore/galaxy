#include "trace_mysql_db_pool.h"
#include "trace_mysql_db.h"

#include <assert.h>
#include <vector>

namespace baidu {
    namespace galaxy {
        namespace trace {
            MysqlPool::MysqlPool(boost::shared_ptr<MysqlDb::Option> op, int pool_size) :
                _op(op),
                _size(pool_size),
                _opened(false) {
                assert(NULL != op.get());
            }
            
            MysqlPool::~MysqlPool() {
                if (_opened) {
                    Close();
                }
                
            }
            
            int MysqlPool::Open() {
                assert(!_opened);
                for (int i = 0; i < _size; i++) {
                    boost::shared_ptr<MysqlDb> db(new MysqlDb(_op));
                    
                    if (0 != db->Open()) {
                        _db_pool.clear();
                        return -1;
                    }
                    _db_pool.push_back(db);
                }
                _opened = true;
                return 0;
            }
            
            int MysqlPool::Write(boost::shared_ptr<google::protobuf::Message> msg) {
                assert(NULL != msg.get());
                {
                    baidu::MutexLock lock(&_mutex);
                    _index++;
                    if (_index >= _size) {
                        _index = 0;
                    }
                }
                return _db_pool[_index]->Write(msg);
            }
            
            int MysqlPool::Close() {
                for (int i = 0; i < _size; i++) {
                    _db_pool[i]->Close();
                }
                _opened = false;
                return 0;
            }
            
            int MysqlPool::PendingNum() {
                return 0;
            }
        }
    }
}
