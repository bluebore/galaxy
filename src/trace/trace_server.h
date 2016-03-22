/* 
 * File:   trace_server.h
 * Author: haolifei
 *
 * Created on 2016年3月9日, 下午4:50
 */

#pragma once

#include "rpc/rpc_client.h"
#include "proto/trace_log.pb.h"

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <thread_pool.h>

namespace baidu {
    namespace galaxy {
        namespace trace {

            class DbFactory;
            class TraceServer : public Event {
            public:
                TraceServer();
                ~TraceServer();
                
                int Setup();
                int Teradown();
                
                void Trace(::google::protobuf::RpcController* cntl,
                        const ::baidu::galaxy::trace::TraceRequest* req,
                        ::baidu::galaxy::trace::TraceResponse* resp,
                        ::google::protobuf::Closure* done);
                
            private:
                boost::shared_ptr<google::protobuf::Message> CreateMessage(const std::string& type_name);
                void WriteDbProc(boost::shared_ptr<google::protobuf::Message> msg);
                boost::scoped_ptr<DbFactory> _db_factory;
                boost::scoped_ptr<baidu::common::ThreadPool> _thead_pool;

            };
        }
    }
}
