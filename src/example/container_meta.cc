/***************************************************************************
 *
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/



/**
 * @file example/container_meta.cc
 * @author haolifei(com@baidu.com)
 * @date 2016/05/31 09:27:24
 * @brief
 *
 **/

#include "agent/container/serializer.h"
#include "protocol/galaxy.pb.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " path" << std::endl;
        exit(1);
    }

    std::string path = argv[1];
    baidu::galaxy::container::Serializer ser;
    baidu::galaxy::util::ErrorCode ec = ser.Setup(path);

    if (ec.Code() != 0) {
        std::cerr << "set up failed: " << ec.Message() << std::endl;
        return -1;
    }

    std::vector<boost::shared_ptr<baidu::galaxy::proto::ContainerMeta> > metas;
    ec = ser.LoadWork(metas);

    if (ec.Code() != 0) {
        std::cerr << "load failed: " << ec.Message() << std::endl;
        return -1;
    }

    std::cout << "total: " << metas.size() << std::endl;

    for (size_t i = 0; i < metas.size(); i++) {
        std::cout << "--------------------------------------" << std::endl;
        std::cout << metas[i]->DebugString() << std::endl;
    }

    return 0;
}
