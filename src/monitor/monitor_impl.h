// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: zhoushiyong@baidu.com

#ifndef PS_SPI_MONITOR_IMPL_H
#define PS_SPI_MONITOR_IMPL_H

#include <vector>
#include <map>
#include <string>
#include <boost/regex.hpp>
#include "common/thread_pool.h"
#include "sdk/monitor_tera_client.h"

namespace galaxy {

struct Watch {
    std::string item_name;
    std::string regex;
    boost::regex reg;
    int count;
    int cnt_sec;
    double last_rolling_time;
};

struct Trigger {
    std::string item_name;
    int threadhold;
    std::string relate;
    uint32_t range;
    double timestamp;
};

struct Action {
    std::vector<std::string> to_list;
    std::string title;
    std::string content;
    double timestamp;
};

struct Rule {
    Watch* watch;
    Trigger* trigger;
    Action* action;
};

class MonitorImpl {
public:
    MonitorImpl();
    ~MonitorImpl();
    bool ParseConfig(const std::string conf_path);
    void Run();
private:
    bool ExecRule(std::string src);
    bool Matching(std::string src, Watch* watch);
    void Reporting();
    void Saving();
    bool Judging(int *cnt, Trigger* trigger);
    bool Treating(Action* action);
    void Split(std::string& src, std::string& delim, std::vector<std::string>* ret);
private:
    common::ThreadPool thread_pool_;
    std::string log_path;
    std::map<std::string, Watch*> watch_map_;
    std::map<std::string, Trigger*> trigger_map_;
    std::map<std::string, Action*> action_map_;
    std::vector<Rule*> rule_list_;
    double msg_forbit_time_;
    bool running_;
    bool tera_running_;
    bool log_rolling_;
    std::string job_name_;
    std::string table_name_;
    std::string tera_flag_;
    MonitorTeraClient* tera_client_;
};
}

#endif  //PS_SPI_MONITOR_IMPL_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
