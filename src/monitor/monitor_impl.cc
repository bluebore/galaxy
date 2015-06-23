// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: zhoushiyong@baidu.com

#include <fstream>
#include <sstream>
#include <time.h>
#include <vector>
#include <sys/stat.h>
#include <errno.h>
#include <boost/bind.hpp>
#include <gflags/gflags.h>
#include "common/asm_atomic.h"
#include "common/logging.h"
#include "common/util.h"
#include "monitor_impl.h"

DECLARE_int32(task_id);

namespace galaxy {
MonitorImpl::MonitorImpl() {
    running_ = false;
    tera_running_ = false;
    log_rolling_ = false;
    msg_forbit_time_ = 60;
    tera_client_ = new MonitorTeraClient();
    thread_pool_.AddTask(boost::bind(&MonitorImpl::Reporting, this));
    thread_pool_.AddTask(boost::bind(&MonitorImpl::Saving, this));
}
MonitorImpl::~MonitorImpl() {
    std::map<std::string, Watch*>::iterator watch_it;
    for (watch_it == watch_map_.begin(); watch_it != watch_map_.end(); watch_it++) {
        delete watch_it->second;
    }
    
    std::map<std::string, Trigger*>::iterator trigger_it;
    for (trigger_it == trigger_map_.begin(); trigger_it != trigger_map_.end();
            trigger_it++) {
        delete trigger_it->second;
    }

    std::map<std::string, Action*>::iterator action_it;
    for (action_it == action_map_.begin(); action_it != action_map_.end(); 
            action_it++) {
        delete action_it->second;
    }

    std::vector<Rule*>::iterator rule_it;
    for (rule_it == rule_list_.begin(); rule_it != rule_list_.end();
            rule_it++) {
        delete *rule_it;
    }
    if (NULL != tera_client_) {
        delete tera_client_;
        tera_client_ = NULL;
    }
    return;
}

void MonitorImpl::Split(std::string& src, std::string& delim, std::vector<std::string>* ret) {
    size_t last = 0;  
    size_t index=src.find_first_of(delim, last);  
    while (index!=std::string::npos) {  
        ret->push_back(src.substr(last,index-last));
        last=index+1;  
        index=src.find_first_of(delim,last);  
    }  
    if (index-last>0) {  
        ret->push_back(src.substr(last,index-last));  
    }
    return;
}
bool MonitorImpl::ParseConfig(const std::string conf_path) {
    std::ifstream fin(conf_path.c_str());
    if (!fin) {
        LOG(WARNING, "open conf_path err %s", conf_path.c_str());
        return false;
    }
    std::string line;
    char value[1024];
    while (getline(fin, line)) {
        if (line.size() == 0) {
            continue;
        }
        if (sscanf(line.c_str(), "<input>:%s", value)) {
            log_path.assign(value);
        } else if (sscanf(line.c_str(), "<watch>:%s", value)) {
            std::vector<std::string> args;
            std::string input(value);
            std::string delim("|");
            Split(input, delim, &args);
            Watch *watch_ptr = new Watch();
            watch_ptr->item_name.assign(args[1]);
            watch_ptr->regex.assign(args[2]);
            watch_ptr->reg.assign(args[2]);
            watch_ptr->count = 0;
            watch_map_[args[0]] = watch_ptr;
        } else if (sscanf(line.c_str(), "<alive>:%s", value)) {
            Watch *watch_ptr = new Watch();
            watch_ptr->item_name.assign("alive");
            watch_ptr->count = 0;
            watch_ptr->last_rolling_time = time(NULL);
            watch_map_[watch_ptr->item_name] = watch_ptr;
        } else if (sscanf(line.c_str(), "<trigger>:%s", value)) {
            std::vector<std::string> args;
            std::string input(value);
            std::string delim("|");
            Split(input, delim, &args);
            Trigger* trigger_ptr = new Trigger();
            trigger_ptr->item_name.assign(args[0]);
            trigger_ptr->threadhold = atoi(args[2].c_str());
            trigger_ptr->relate.assign(args[1]);
            trigger_ptr->range = atoi(args[3].c_str());
            trigger_ptr->timestamp = time(NULL);
            trigger_map_[args[0]] = trigger_ptr;
        } else if (sscanf(line.c_str(), "<action>:%s", value)) {
            std::vector<std::string> args;
            std::string input(value);
            std::string delim("|");
            Split(input, delim, &args);
            Action* action_ptr = new Action();
            action_ptr->title.assign(args[2]);
            action_ptr->content.assign(args[3]);
            action_ptr->timestamp = 0;
            delim.assign(":");
            Split(args[1], delim, &(action_ptr->to_list));
            action_map_[args[0]] = action_ptr;
        } else if (sscanf(line.c_str(), "<rule>:%s", value)) {
            std::vector<std::string> args;
            std::string input(value);
            std::string delim("|");
            Split(input, delim, &args);
            Rule* rule_ptr = new Rule();
            rule_ptr->watch = watch_map_.find(args[0])->second;
            rule_ptr->trigger = trigger_map_.find(args[1])->second;
            rule_ptr->action = action_map_.find(args[2])->second;
            rule_list_.push_back(rule_ptr);
        } else if (sscanf(line.c_str(), "<tera_conf>:%s", value)) {
            tera_flag_.assign(value);
        } else if (sscanf(line.c_str(), "<task_name>:%s", value)) {
            job_name_.assign(value);
        } else if (sscanf(line.c_str(), "<table_name>:%s", value)) {
            table_name_.assign(value);
        }
    }
    if (table_name_.size() == 0 || job_name_.size() == 0) {
        return false;
    }
    
    if (OK == tera_client_->Init(tera_flag_, table_name_)) {
        tera_running_ = true;
    }
    return true;
}

void MonitorImpl::Run() {
    size_t seek = 0;
    struct stat* st_mark = new struct stat;
    while (0 != stat(log_path.c_str(), st_mark)) {
        LOG(WARNING, "stat log file err %s [%d:%s]", log_path.c_str(),
                errno, strerror(errno));
        sleep(1);
        continue;
    }
    std::ifstream fin(log_path.c_str());
    fin.seekg(0, std::ios::end);
    seek = fin.tellg();
    fin.seekg(seek, std::ios::beg);
    std::string line;
    running_ = true;
    log_rolling_ = false;

    while (running_) {
        if (fin.peek() == EOF) {  
            struct stat* st_tmp = new struct stat;
            if (0 != stat(log_path.c_str(), st_tmp)) {
                LOG(WARNING, "stat log file err %s [%d:%s]", log_path.c_str(),
                     errno, strerror(errno));
                delete st_tmp;
                sleep(1);
                continue;
            } else if (st_tmp->st_ino != st_mark->st_ino) {
                fin.close();
                fin.clear();
                seek = 0;
                fin.open(log_path.c_str());
                delete st_mark;
                st_mark = st_tmp;
                continue;
            } else if (st_tmp->st_mtime == st_mark->st_mtime) {
                log_rolling_ = false;
                delete st_mark;
                st_mark = st_tmp;
                continue;
            } else {
                log_rolling_ = true;
                delete st_mark;
                st_mark = st_tmp;
                if (seek != 0) {
                    fin.clear();  
                    fin.seekg(seek, std::ios::beg);  
                    sleep(1);  
                    continue;  
                } else {
                    sleep(1);
                    continue;
                }
            }
        }
        getline(fin, line);
        seek = fin.tellg();
        ExecRule(std::string(line));
    }
    delete st_mark;
    fin.close();
    return;
}

bool MonitorImpl::ExecRule(std::string src) {
    for (std::vector<Rule*>::iterator it = rule_list_.begin();
            it != rule_list_.end(); it++) {
        if (Matching(src, (*it)->watch)) {
            //LOG(INFO, "Matching hit %s", src.c_str());
        }
    }
    return true;
}

bool MonitorImpl::Matching(std::string src, Watch* watch) {
    assert(watch != NULL);
    if (watch->item_name == "alive" && log_rolling_ == true)
    {
        watch->last_rolling_time = time(NULL);
        return true;
    }
    boost::cmatch mat;
    if (boost::regex_search(src.c_str(), mat, watch->reg)) {
        common::atomic_inc(&watch->count);
        common::atomic_inc(&watch->cnt_sec);
        return true;
    }
    return false;
}

bool MonitorImpl::Judging( int* cnt, Trigger* trigger) {
    assert(trigger != NULL);
    bool ret = false;
    if (trigger->relate == "<") {
        ret = (*cnt < trigger->threadhold);
    } else if (trigger->relate == std::string("=")) {
        ret = (*cnt == trigger->threadhold);
    } else if (trigger->relate == std::string(">")) {
        ret = (*cnt > trigger->threadhold);
    } else {
        LOG(WARNING, "unsupported relate %s", trigger->relate.c_str());       
    }

    if (time(NULL) - trigger->timestamp >= trigger->range) {
        common::atomic_swap(cnt, 0);
        trigger->timestamp = time(NULL);
        return ret;
    }
    return false;
}

bool MonitorImpl::Treating(Action* act) {
    assert(act != NULL);
    if (act->timestamp == 0) {
        act->timestamp = time(NULL);
    } else if ((time(NULL) - act->timestamp) < msg_forbit_time_) {
        return true;
    }
    for (std::vector<std::string>::iterator it = act->to_list.begin();
            it != act->to_list.end(); it++) { 
        std::string cmd = std::string("/home/galaxy/tools/gsmsend ")
            + std::string("-s emp01.baidu.com:15001 -semp02.baidu.com:15001 ") 
            + *it + "@" + "\"from:" + common::util::GetLocalHostName() 
            + "\n" + act->title + ":" + act->content + "\"";
        if (0 != system(cmd.c_str())) {
            LOG(WARNING, "gsmsend msg err %s", cmd.c_str());
        }
        act->timestamp = time(NULL);
    }
    return true;
}

void MonitorImpl::Reporting() {
    if (running_) {
        for (std::vector<Rule*>::iterator it = rule_list_.begin();
                it != rule_list_.end(); it++) {
            if ((*it)->watch->item_name == "alive") {
                if (time(NULL) - (*it)->watch->last_rolling_time >= 
                        (*it)->trigger->range) {
                    (*it)->watch->last_rolling_time = time(NULL);
                } else {
                    continue;
                }
            } else if (!Judging(&((*it)->watch->count), (*it)->trigger) 
                    && log_rolling_ == true) {
                continue;
            }
            if (!Treating((*it)->action)) {
                continue;
            }
        }
    }
    thread_pool_.DelayTask(3000, boost::bind(&MonitorImpl::Reporting, this));
    return;
}


void MonitorImpl::Saving() {
    if (running_ && tera_running_) {
        monitor_record_t* rec = new monitor_record_t();
        std::stringstream ss_task_id;
        ss_task_id << FLAGS_task_id;
        std::stringstream ss_group_id;
        ss_group_id << FLAGS_task_id%10;
        std::stringstream ss_time_stamp;
        ss_time_stamp << time(NULL);
        rec->key = ss_group_id.str() + job_name_ + ss_time_stamp.str() 
            + ss_task_id.str();
        for (std::vector<Rule*>::iterator it = rule_list_.begin();
                it != rule_list_.end(); it++) {
            rec->column_list[(*it)->watch->item_name] = (*it)->watch->cnt_sec;
            common::atomic_swap(&((*it)->watch->cnt_sec), 0);
        }
        tera_client_->Add(rec);
    }
    thread_pool_.DelayTask(1000, boost::bind(&MonitorImpl::Saving, this));
}
}


/* vim: set ts=4 sw=4 sts=4 tw=100 */
