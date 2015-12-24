// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_HTTP_CLIENT_H
#define BAIDU_HTTP_CLIENT_H

#include <sstream>
#include <vector>
#include <map>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include "master/base_client.h"
#include "thread_pool.h"

namespace baidu {
namespace lumia {

struct HttpPostRequest {
    std::vector<std::pair<std::string, std::string> > data;
    std::string url;
    std::vector<std::string> headers;
};

struct HttpResponse {
    std::vector<std::string> headers;
    std::string body;
    std::string error;
    uint32_t status;
};

struct HttpGetRequest {
    std::string url;
    std::vector<std::string> headers;
};

typedef boost::function<void (const boost::shared_ptr<HttpResponse> response_ptr)> HttpCallback;

struct Url{
    std::string protocol;
    std::string server;
    std::string path;
};

class HttpClient {

public:
    HttpClient();
    ~HttpClient(){}
    bool AsyncPost(const boost::shared_ptr<HttpPostRequest> request_ptr,
                   boost::shared_ptr<HttpResponse> response_ptr,
                   HttpCallback callback);
    bool SyncGet(const HttpGetRequest& request,
                 HttpResponse* response);
    bool SyncPost(const HttpPostRequest& request,
                 HttpResponse* response);

private:
    bool BuildPostForm(const boost::shared_ptr<HttpPostRequest> request_ptr, 
                       const Url& url,
                       boost::shared_ptr<boost::asio::streambuf> request_buffer);
    bool BuildSyncPostForm(const HttpPostRequest& request,
                           const Url& url,
                           boost::asio::streambuf& request_buffer);
    bool ParseUrl(const std::string& surl, Url* url);
    void UrlEncode(const std::string& str, std::string* output);
    void HandleCallback(HttpCallback callback, boost::shared_ptr<HttpResponse> response_ptr,
                       boost::shared_ptr<boost::asio::streambuf> request_buffer,
                       boost::shared_ptr<boost::asio::streambuf> response_buffer,
                       boost::shared_ptr<ResponseMeta> meta_ptr,
                       int32_t status);
    void Execute(const Url url,
                 const boost::shared_ptr<boost::asio::streambuf> request_buffer,
                 const boost::shared_ptr<boost::asio::streambuf> response_buffer,
                 const boost::shared_ptr<ResponseMeta> meta_ptr,
                 Callback callback);
    bool BuildGetReq(const Url& url,
                     const HttpGetRequest& req,
                     boost::asio::streambuf& req_buf);
private:
    BaseClient base_client_;
    ::baidu::common::ThreadPool worker_;
};


}
}
#endif
