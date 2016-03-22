
#include "trace_server.h"
#include "trace_db_factory.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include <iostream>

namespace baidu {
    namespace galaxy {
        namespace trace {

            TraceServer::TraceServer() {
            }

            TraceServer::~TraceServer() {
            }



            int TraceServer::Setup() {
                _thead_pool.reset(new ThreadPool(10));
                _db_factory.reset(new DbFactory());
                return _db_factory->Init();
            }

            int TraceServer::Teradown() {
                
                return 0;
            }

            void TraceServer::Trace(::google::protobuf::RpcController* cntl,
                    const ::baidu::galaxy::trace::TraceRequest* req,
                    ::baidu::galaxy::trace::TraceResponse* resp,
                    ::google::protobuf::Closure* done) {
                
                boost::shared_ptr<google::protobuf::Message> msg = CreateMessage(req->name());
                if (msg.get() != NULL) {
                    if (msg->ParseFromString(req->value())) {
                        _thead_pool->AddTask(boost::bind(&TraceServer::WriteDbProc, this, msg));

                    }
                }
                done->Run();
            }
            
             void TraceServer::WriteDbProc(boost::shared_ptr<google::protobuf::Message> msg) {
                 boost::shared_ptr<Db> db = _db_factory->GetDb(msg);
                 std::cout << "=============================" << msg->GetDescriptor()->full_name() << std::endl;
                 if (NULL != db.get()) {
                     if(0 != db->Write(msg)) {
                         std::cout << "write db failed" << msg->GetDescriptor()->full_name() << std::endl;
                     }
                 } else {
                     std::cout << "donot support msg: " << msg->GetDescriptor()->full_name() << std::endl;
                 }
                 
             }
            
            boost::shared_ptr<google::protobuf::Message> TraceServer::CreateMessage(const std::string& type_name) {
                boost::shared_ptr<google::protobuf::Message>  msg;
                const google::protobuf::Descriptor* des = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
                if (NULL == des) {
                    return msg;
                }
                const google::protobuf::Message* prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(des);
                if (NULL == prototype) {
                    return msg;
                }
                
                msg.reset(prototype->New());
                return msg;
            }
        }
    }
}
