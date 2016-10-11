// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PS_SPI_BNS_SDK_H
#define PS_SPI_BNS_SDK_H

#include <vector>
#include <set>
#include <string>
#include <boost/function.hpp>
#include <thread_pool.h>
#include "naming/sdk.h"

namespace baidu {
namespace galaxy {
class PrivateSubscribeSdk : public SubscribeSdk {
public:
	PrivateSubscribeSdk(const std::string& service_name,
					int update_interval = 10000,
					int timeout_interval = 1000);
    ~PrivateSubscribeSdk() {};

    void Init();

    void GetServiceList(std::vector<std::string>* addr_list);

    void RegistAddHandler(HandleFunc func); 

    void RegistDelHandler(HandleFunc func);

    void RegistRefreshHandler(HandleFunc func);

private:
	void ReloadService();
	ThreadPool worker_;
    Mutex mutex_;
    std::string service_name_;
    std::set<std::string> addr_list_;
    int update_interval_;
    int timeout_interval_;
    bool init_;
    HandleFunc add_func_;
    HandleFunc del_func_;
    HandleFunc chg_func_;
};


class CurlThreadPool {
public:
	static CurlThreadPool* GetInstance() {
		if (instance_ == NULL) {
			instance_ = new CurlThreadPool();
		}
		return instance_;
	}
	ThreadPool* GetThreadPool() {
		return &worker_;
	}
private:
	CurlThreadPool() {}
	ThreadPool worker_;
	static CurlThreadPool* instance_;
};



class PrivatePublicSdk : public PublicSdk {
public:
	PrivatePublicSdk(std::string service_name,
				std::string token,
				std::string tag,
				std::string health_check,
				std::string health_script);
    ~PrivatePublicSdk();

    void Init();

    void AddServiceInstance(std::string podid, const ServiceInfo &service);

    void DelServiceInstance(const std::string service_name);

    void UpdateServiceInstance(const ServiceInfo & service);

    void Finish();

    bool IsRunning();

private:
	ThreadPool* curl_worker_;
	Mutex mutex_;
	std::string service_name_;
	std::string token_;
	std::string tag_;
	std::string health_check_;
	std::string health_script_;
	int64_t check_task_id_;
	bool init_;
	int time_out_;
};

}
}

#endif  //PS_SPI_BNS_SDK_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
