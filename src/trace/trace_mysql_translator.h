/* 
 * File:   trace_translator.h
 * Author: haolifei
 *
 * Created on 2016年3月10日, 下午2:31
 */

#pragma once 
#include <boost/shared_ptr.hpp>
#include <google/protobuf/message.h>

#include <map>
#include <string>

namespace baidu {
    namespace galaxy {
        namespace trace {

            class MysqlTranslator {
            public:
                MysqlTranslator();
                ~MysqlTranslator();
                const std::string translate(boost::shared_ptr<google::protobuf::Message> msg);
                
            private:
                const std::string sql_value(const google::protobuf::Message* msg,
                    const google::protobuf::Reflection* ref,
                    const google::protobuf::FieldDescriptor* fd);
                
                const std::string table_name(const google::protobuf::Descriptor* des);
                std::map<std::string, std::string> _m_table;
            };
        }
    }
}
