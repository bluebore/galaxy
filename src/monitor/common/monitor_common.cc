/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file monitor_common.cc
 * @author zhoushiyong(zhoushiyong@baidu.com)
 * @date 2015/06/24 17:37:03
 * @version $Revision$ 
 * @brief 
 *  
 **/
#include "monitor_common.h"

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
/* vim: set ts=4 sw=4 sts=4 tw=100 */
