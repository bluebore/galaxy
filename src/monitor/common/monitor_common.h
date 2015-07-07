/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file monitor_common.h
 * @author zhoushiyong(zhoushiyong@baidu.com)
 * @date 2015/06/24 17:12:10
 * @version $Revision$ 
 * @brief 
 *  
 **/
#ifndef PS_SPI_MONITOR_COMMON_H
#define PS_SPI_MONITOR_COMMON_H

#include <string>
#include <boost/regex.hpp>

namespace galaxy {

struct Watch{
    std::string item_name;
    std::string regex;
    boost::regex reg;
    int count;
    int cnt_sec;
    double last_rolling_time;
};

struct Trigger {
    std::string item_name;
    int threshold;
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

bool IsReachThreshold(int* cnt, Trigger* trigger);
}
//PS_SPI_MONITOR_COMMON_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
