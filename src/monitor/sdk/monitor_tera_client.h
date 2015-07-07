/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file monitor_tera_client.h
 * @author zhoushiyong(zhoushiyong@baidu.com)
 * @date 2015/06/12 15:33:40
 * @version $Revision$ 
 * @brief 
 *  
 **/
#ifndef PS_SPI_MONITOR_TERA_CLIENT_H
#define PS_SPI_MONITOR_TERA_CLIENT_H

#include <string>
#include <map>
#include "tera.h"

namespace galaxy {

enum SdkCode {
    OK,
    ERR,
    FINISH,
    NOTEXIST,
};

struct monitor_record_t {
    std::string key;
    std::map<std::string, uint32_t> column_list;
};

typedef void (*CallBack)(monitor_record_t* rec);

class MonitorTeraClient {
public:
    MonitorTeraClient();
    ~MonitorTeraClient();
    int Init(const std::string& tera_conf, const std::string& tera_table);
    int Add(monitor_record_t* record);
    int Scan(const std::string& begin, const std::string& end);
    int GetNextRow(monitor_record_t* record);
    int ScannerClear();
private:
    static void OnAddFinish(tera::RowMutation* mutation);
    tera::Client* tera_client_;
    tera::Table* tera_table_;
    tera::ResultStream* scanner_;
    std::string tera_conf_;
    std::string tera_table_name_;
};
}
#endif  //PS_SPI_MONITOR_TERA_CLIENT_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
