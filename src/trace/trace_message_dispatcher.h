/* 
 * File:   trace_message_dispatcher.h
 * Author: haolifei
 *
 * Created on 2016年3月9日, 下午4:56
 */

#pragma once

namespace baidu {
    namespace galaxy {
        namespace trace {
            
            class MessageDispatcher {
            public:
                int Setup();
                int TeraDown();
                int Dispatch(google::protobuf::Message* msg);
            };
        }
    }
}
