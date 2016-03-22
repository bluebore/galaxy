/* 
 * File:   dispatcher.h
 * Author: haolifei
 *
 */

//#pragma once
#ifndef TRACE_LOG_CLIENT
#define TRACE_LOG_CLIENT

#include <google/protobuf/message.h>
#include <string>

namespace baidu {
    namespace galaxy {
        namespace trace {
            
            class Trace {
            public:
                static Trace* GetInstance();
                static void DestroyTrace(Trace*& trace);
            public:
                virtual ~Trace() {}
                virtual int Setup(const std::string& server) = 0;
                virtual int Teradown() = 0;
                virtual int Log(google::protobuf::Message* msg) = 0;
                static Trace* s_instance;
            };
            
        }
    }
}
#endif
