#ifndef BAIDU_RMS_SDK_H
#define BAIDU_RMS_SDK_H

#include "master/rms_sdk.h"

DECLARE_string(rms_token);
DECLARE_string(rms_app_key);
DECLARE_string(rms_auth_user);
DECLARE_string(rms_api_check_job);

namespace baidu {
namespace lumia {


RmsSdk::RmsSdk(HttpClient* http_client):http_client_(http_client) {}

RmsSdk::~RmsSdk(){}

bool Reboot(const std::string& id, RmsCallback callback) {


}

}
}
#endif
