#ifndef BAIDU_RMS_SDK_H
#define BAIDU_RMS_SDK_H

#include "master/http_client.h"
namespace baidu {
namespace lumia {

class RmsSdk {

public:
    RmsSdk(HttpClient* http_client);
    ~RmsSdk();
    bool Reboot(const std::string& id);
    bool ReInstall(const std::string& id);
private:
    HttpClient* http_client_;
};

}
}
#endif
