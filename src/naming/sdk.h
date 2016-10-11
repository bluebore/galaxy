// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PS_SPI_SDK_H
#define PS_SPI_SDK_H

#include <string>
#include <boost/function.hpp>
#include "protocol/galaxy.pb.h"

namespace baidu {
namespace galaxy {
using ::baidu::galaxy::proto::ServiceInfo;
typedef boost::function<void (std::vector<std::string>&)> HandleFunc;
class SubscribeSdk {
public:
    virtual ~SubscribeSdk() {}

    virtual void Init() = 0;

    virtual void GetServiceList(std::vector<std::string>* addr_list) = 0;

    virtual void RegistHandler(HandleFunc* func) = 0;
};

class PublicSdk {
public:
    virtual ~PublicSdk() {}

    virtual void Init() = 0;

    virtual void AddServiceInstance(std::string podid, const ServiceInfo& service) = 0;

    virtual void DelServiceInstance(const std::string service_name) = 0;

    virtual void Finish() = 0;

    virtual bool IsRunning() = 0;
};

}
}

#endif  //PS_SPI_SDK_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
