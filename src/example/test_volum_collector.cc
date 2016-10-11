/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file src/example/test_volum_collector.cc
 * @author haolifei(com@baidu.com)
 * @date 2016/06/06 13:06:40
 * @brief 
 *  
 **/

#include <iostream>
#include "gflags/gflags.h"
#include "agent/volum/volum_collector.h"

DECLARE_int64(volum_collect_cycle);

int main(int argc, char** argv) {
    baidu::galaxy::volum::VolumCollector c(argv[1]);
    c.Collect();
    std::cerr << "du: " << c.Size() << std::endl;
    return 0;
}





















/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
