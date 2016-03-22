#include "trace_mysql_translator.h"

#include <google/protobuf/descriptor.h>

#include <assert.h>

#include <iostream>
#include <sstream>

namespace baidu {
    namespace galaxy {
        namespace trace {

            typedef union ProtoValue {
                bool bool_value;
                int32_t int32_value;
                int64_t int64_value;
                uint32_t uint32_value;
                uint64_t uint64_value;
                float float_value;
                double double_value;
            };
            
            MysqlTranslator::MysqlTranslator() {
                _m_table["baidu.galaxy.trace.TracePod"] = "lumia_pod";
                _m_table["baidu.galaxy.trace.TraceJob"] = "lumia_job";
                _m_table["baidu.galaxy.trace.TraceJobMeta"] = "lumia_jobmeta";
                _m_table["baidu.galaxy.trace.TraceJobHistory"] = "lumia_jobhistory";
                _m_table["baidu.galaxy.trace.TraceClusterHistory"] = "lumia_clusterhistory";
                _m_table["baidu.galaxy.trace.TraceCluster"] = "lumia_cluster";
                _m_table["baidu.galaxy.trace.TraceAgentError"] = "lumia_agenterror";
                _m_table["baidu.galaxy.trace.TracePodHistory"] = "lumia_podhistory";    
                _m_table["baidu.galaxy.trace.TraceAgent"] = "lumia_agent";

            }

            MysqlTranslator::~MysqlTranslator() {

            }

            const std::string MysqlTranslator::translate(boost::shared_ptr<google::protobuf::Message> msg) {
                assert(NULL != msg.get());
                
                const google::protobuf::Reflection* ref = msg->GetReflection();
                const google::protobuf::Descriptor* des = msg->GetDescriptor();
                
                std::string sql_names;
                std::string sql_values;
                std::string update_filed;
                
                int field_size = des->field_count();
                for (int i = 0; i < field_size; i++) {
                    const google::protobuf::FieldDescriptor* fd = des->field(i);
                    if (ref->HasField(*msg, fd)) {
                        if (!sql_names.empty()) {
                            sql_names += ",";
                            sql_values += ",";
                            update_filed += ",";
                        }
                        std::string v = sql_value(msg.get(), ref, fd);
                        sql_names += fd->name();
                        sql_values += v;
                        update_filed = update_filed + fd->name() + "=" + v;
                    }
                }
                std::cerr << "===================" << des->full_name() << std::endl;

                std::string table = table_name(des);
                if (table.empty()) {
                    return "";
                }
                
                std::string sql = "insert " + table + "(" + sql_names + ") values("
                        + sql_values + ") ON DUPLICATE KEY UPDATE "
                        + update_filed;
                return sql;
            }
            

            const std::string MysqlTranslator::table_name(const google::protobuf::Descriptor* des) {
                assert(NULL != des);

                std::map<std::string, std::string>::iterator iter = _m_table.find(des->full_name());
                if (_m_table.end() != iter) {
                    return iter->second;
                }
                return "";
            }
            
            // Fix Me: exception
            const std::string MysqlTranslator::sql_value(const google::protobuf::Message* msg,
                    const google::protobuf::Reflection* ref,
                    const google::protobuf::FieldDescriptor* fd) {
                assert(NULL != fd);
                assert(NULL != msg);
                assert(NULL != ref);
                
                ProtoValue value;
                std::string str_value;
                std::stringstream stream;

                std::string quote = "'";

                switch(fd->type()) {
                    case google::protobuf::FieldDescriptor::TYPE_BOOL:
                        value.uint32_value = ref->GetBool(*msg, fd);
                        stream << value.uint32_value;
                        break;
                    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
                        value.double_value = ref->GetDouble(*msg, fd);
                        stream << value.double_value;
                        break;
                    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
                        value.float_value = ref->GetFloat(*msg, fd);
                        stream << value.float_value;
                        break;
                    case google::protobuf::FieldDescriptor::TYPE_INT32:
                        value.int32_value = ref->GetInt32(*msg, fd);
                        stream << value.int32_value;
                        break;
                    case google::protobuf::FieldDescriptor::TYPE_INT64:
                        value.int64_value =  ref->GetInt64(*msg, fd);
                        stream << value.int64_value;
                        break;
                    case google::protobuf::FieldDescriptor::TYPE_STRING:
                    case google::protobuf::FieldDescriptor::TYPE_BYTES:
                        str_value = ref->GetString(*msg, fd);
                        stream << quote << str_value << quote;
                        
                        break;
                    case google::protobuf::FieldDescriptor::TYPE_UINT32:
                        value.uint32_value = ref->GetUInt32(*msg, fd);
                        stream << value.uint32_value;
                        break;
                    case google::protobuf::FieldDescriptor::TYPE_UINT64:
                        value.uint64_value =  ref->GetUInt64(*msg, fd);
                        stream << value.uint64_value;
                        break;
                    default:
                        break;
                        
                }

                return stream.str();
            }

        }
    }
}
