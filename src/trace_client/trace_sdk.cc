#include "trace_sdk.h"
#include "rpc/rpc_client.h"
#include "proto/trace_log.pb.h"

#include <assert.h>
#include <boost/function.hpp>
#include <logging.h>

namespace baidu {
    namespace galaxy {
        namespace trace {

            
            class DefaultTraceImpl : public Trace {
            public:
                DefaultTraceImpl() {}
                ~DefaultTraceImpl() {}
                
                int Setup(const std::string& server) {
                    return 0;
                }
                
                int Teradown() {
                    return 0;
                }
                
                int Log(google::protobuf::Message* msg) {
                    // do nothing, just write debug msg to disk
                    assert(NULL != msg);
                    LOG(INFO, "%s", msg->DebugString().c_str());
                    return 0;
                }
            };
            
            class TraceSdkImpl : public Trace {
            private:
                bool _setup;
            public:
                TraceSdkImpl() :
                    _setup(false){
                    
                }
                
                ~TraceSdkImpl() {
                    
                }
                
                
                
                virtual int Setup(const std::string& server) {
                    _server = server;
                    _setup = true;
                    return 0;
                }
                
                virtual int Teradown() {
                    return 0;
                }
                
                int Log(google::protobuf::Message* msg) {
                    assert(_setup);
                    return AsynLog(msg);
                }
                
                int SynLog(google::protobuf::Message* msg) {
                    Event_Stub* stub;
                    int ret = -1;
                    if (rpc_client_.GetStub(_server, &stub)) {
                        TraceRequest request;
                        request.set_name(msg->GetDescriptor()->full_name());
                        request.set_value(msg->SerializeAsString());
                        
                        TraceResponse response;
                        if (rpc_client_.SendRequest(stub, &Event_Stub::Trace, &request, &response, 1, 3)) {
                            ret = 0;
                        }
                        delete stub;
                    }
                    return ret;
                }
                
                 int AsynLog(google::protobuf::Message* msg) {
                    Event_Stub* stub;
                    if (rpc_client_.GetStub(_server, &stub)) {
                        TraceRequest* request = new TraceRequest();
                        TraceResponse* response = new TraceResponse();
                        request->set_name(msg->GetDescriptor()->full_name());
                        request->set_value(msg->SerializeAsString());
                        
                        
                        boost::function<void (const TraceRequest*, 
                                    TraceResponse*, bool, int)> callback = 
                            boost::bind(&TraceSdkImpl::RequestCallback, this, _1, _2, _3, _4);
                        
                        rpc_client_.AsyncRequest(stub,
                                    &Event_Stub::Trace, 
                                    request, 
                                    response, 
                                    callback, 
                                    1, 
                                    0);

                        delete stub;
                    }
                    return 0;
                }
                 
                 void RequestCallback(const TraceRequest* req, TraceResponse* res, bool failed, int error) {
                     if (failed) {
                         LOG(WARNING, "trace failed: %s", req->GetDescriptor()->full_name().c_str());
                     }
                     delete req;
                     delete res;
                 }
                
            private:
                std::string _server;
                RpcClient rpc_client_;
                
            };

            
            Trace* Trace::s_instance = NULL;

            Trace* Trace::GetInstance() {
                if (NULL == s_instance) {
                    s_instance = new TraceSdkImpl();
                }
                return s_instance;
            }
            
            void Trace::DestroyTrace(Trace*& trace) {
                if (NULL != trace) {
                    delete trace;
                    trace = NULL;
                }
            }
        }
    }
}
