/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file monitor_tera_client.cc
 * @author zhoushiyong(zhoushiyong@baidu.com)
 * @date 2015/06/15 13:49:38
 * @version $Revision$ 
 * @brief 
 *  
 **/
#include "monitor_tera_client.h"
#include <sstream>
namespace galaxy {
MonitorTeraClient::MonitorTeraClient() {
    scanner_ = NULL;
    tera_client_ = NULL;
    tera_table_ = NULL;
}

MonitorTeraClient::~MonitorTeraClient() {
    if (tera_client_) {
        delete tera_client_;
    }
    if (tera_table_) {
        delete tera_table_;
    }
    if (NULL == scanner_) {
        delete scanner_;
    }
}

int MonitorTeraClient::Init(const std::string& tera_conf, 
        const std::string& tera_table) {
    do {
        tera_table_name_ = tera_table;
        tera_conf_ = tera_conf;
        tera::ErrorCode newclient_errcode;
        tera_client_ = tera::Client::NewClient(tera_conf_,
                &newclient_errcode);
        if (newclient_errcode.GetType() != tera::ErrorCode::kOK) {
            break;
        }
        tera::ErrorCode opentable_errcode;
        tera_table_ = tera_client_->OpenTable(tera_table_name_,
                &opentable_errcode);
        if (opentable_errcode.GetType() != tera::ErrorCode::kOK) {
            break;
        }
        return OK;
    } while (0);
    delete tera_client_;
    delete tera_table_;
    tera_client_ = NULL;
    tera_table_ = NULL;
    return ERR;
}

int MonitorTeraClient::Add(monitor_record_t* record) {
    tera::RowMutation* mutation = tera_table_->NewRowMutation(record->key);
    for (std::map<std::string, uint32_t>::iterator it = record->column_list.begin();
            it != record->column_list.end(); it++) {
        std::stringstream ss_value;
        ss_value << it->second;
        std::string qualifier = "";
        mutation->Put(it->first, qualifier, ss_value.str());
    }
    mutation->SetContext(record);
    mutation->SetCallBack(OnAddFinish);
    tera_table_->ApplyMutation(mutation);
    return OK;
}

void MonitorTeraClient::OnAddFinish(tera::RowMutation* mutation) {
    monitor_record_t* rec = (monitor_record_t*)mutation->GetContext();
    if (NULL != rec) {
        delete rec;
    }
    return;
}
int MonitorTeraClient::Scan(const std::string& begin, const std::string& end) {
    if (NULL != scanner_) {
        return ERR;
    }
    tera::ErrorCode errcode;
    tera::ScanDescriptor desc(begin);
    desc.SetEnd(end);
    if ((scanner_ = tera_table_->Scan(desc, &errcode)) == NULL) {
        return ERR;
    }
    return OK;
}

int MonitorTeraClient::GetNextRow(monitor_record_t* record) {
    if (scanner_ == NULL) {
        return ERR;
    }
    if (scanner_->Done()) {
        delete scanner_;
        return FINISH;
    }
    std::string cur_key = scanner_->RowName();
    record->key = scanner_->RowName();
    while (!scanner_->Done() && (cur_key.compare(scanner_->RowName()) != 0)) {
        record->column_list[scanner_->ColumnName()] = atoi(scanner_->Value().c_str());
    }
    return OK;
}

int MonitorTeraClient::ScannerClear() {
    if (scanner_ == NULL) {
        return ERR;
    }
    delete scanner_;
    return OK;
}

}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
