#ifndef BAIDU_RMS_SDK_H
#define BAIDU_RMS_SDK_H

#include "master/http_client.h"
#include <boost/function.hpp>

namespace baidu {
namespace lumia {

typedef boost::function<void (const std::string& id, bool fails)> RmsCallback;

class RmsSdk {

public:
    RmsSdk(HttpClient* http_client);
    ~RmsSdk();
    bool Reboot(const std::string& id, RmsCallback callback);
    bool ReInstall(const std::string& id,
                   const std::string& kernel,
                   RmsCallback callback);
private:
    HttpClient* http_client_;
};

}
}
#endif
