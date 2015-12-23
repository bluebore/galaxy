// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_LUMIA_SDK_LUMIA_H
#define BAIDU_LUMIA_SDK_LUMIA_H

#include <string>
#include <vector>

namespace baidu {
namespace lumia {

struct CpuDesc {
    int32_t count;
    std::string clock;
};

struct MemDesc {
    int64_t size;
    int32_t count;
};

struct DiskDesc {
    int64_t size;
    int32_t count;
    int32_t speed;
};

struct MinionDesc {
    std::string id;
    std::string ip;
    std::string rock_ip;
    std::string hostname;
    std::string state;

    CpuDesc cpu;
    MemDesc mem;
    DiskDesc disk;
    DiskDesc flash;

    int32_t bandwidth;
    std::string datacenter;
    bool mount_ok;
    bool device_ok;
};

class LumiaSdk {
public:
    LumiaSdk();
    virtual ~LumiaSdk();
    static LumiaSdk* ConnectLumia(const std::string& lumia_addr);
    virtual bool ReportDeadMinion(const std::string& ip, const std::string& reason) = 0;
    virtual bool GetMinion(const std::vector<std::string>& ips,
                           const std::vector<std::string>& hostnames,
                           const std::vector<std::string>& ids,
                           std::vector<MinionDesc>* minions) = 0;
    virtual bool ImportData(const std::string& dict_path,
                            const std::string& scripts_dir) = 0;
};

}
}
#endif


