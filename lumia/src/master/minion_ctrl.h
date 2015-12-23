// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_CTRL_MACHINE_H
#define BAIDU_CTRL_MACHINE_H

#include <boost/function.hpp>
#include "master/http_client.h"
#include "thread_pool.h"
#include "mutex.h"

namespace baidu {
namespace lumia {
typedef boost::function<void (const std::string sessionid, const std::vector<std::string> success, const std::vector<std::string> fails)> CallBack;
struct ExecContext {
    std::string sessionid;
    std::string script;
    std::vector<std::string> hosts;
    std::string account;
    int32_t concurrency;
    CallBack callback;
    std::string jobid;
    int32_t try_count;
};

class MinionCtrl {
public:
    MinionCtrl(const std::string& ccs_http_server,
               const std::string& rms_http_server);
    bool ExecOnNoah(const std::string& script,
              const std::vector<std::string>& hosts,
              const std::string& account,
              int32_t concurrency,
              std::string* sessionid,
              const CallBack& callback);
    bool Reboot(const std::vector<std::string>& hosts,
                const CallBack& callback,
                std::string* sessionid);
    ~MinionCtrl();
private:
    bool BuildJob(const std::string& script,
                  const std::vector<std::string>& hosts,
                  const std::string& account,
                  const std::string& service,
                  int32_t concurrency,
                  std::string* job);
    bool GenerateTicket(std::string* ticket, std::string* service);

    void CheckExecJob(const std::string& sessionid);
    void HandleJobFinished(const std::string& sessionid);

    void CheckRebootJob(const std::string& sessionid);

    bool BuildRebootJob(const std::vector<std::string>& hosts,
                        std::string* query_str);

    bool GetRmsAccessToken(std::string* access_token);

private:
    std::string ccs_http_server_;
    std::string rms_http_server_;
    HttpClient http_client_;
    ::baidu::common::ThreadPool http_workers_;
    ::baidu::common::ThreadPool call_back_workers_;
    std::map<std::string, ExecContext> exec_sessions_;
    Mutex mutex_;
};

}
}
#endif

