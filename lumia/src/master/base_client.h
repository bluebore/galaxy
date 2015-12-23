// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BAIDU_LUMIA_UTIL_HTTP_CLIENT_H
#define BAIDU_LUMIA_UTIL_HTTP_CLIENT_H
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <map>

namespace baidu {
namespace lumia {
struct ResponseMeta {
    uint32_t status;
    std::string http_version;
    std::string http_msg;
    std::vector<std::string > headers;
};

typedef boost::function<void (const boost::shared_ptr<boost::asio::streambuf> request_ptr,
                              boost::shared_ptr<boost::asio::streambuf> response_ptr,
                              boost::shared_ptr<ResponseMeta> meta_ptr,
                              int32_t status)> Callback;


struct RequestContext {
    const boost::shared_ptr<boost::asio::streambuf> request_ptr;
    boost::shared_ptr<boost::asio::streambuf> response_ptr;
    boost::shared_ptr<ResponseMeta> meta_ptr;
    Callback callback;
    std::string server;
    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ip::tcp::socket socket;
    RequestContext(boost::asio::io_service& io_service,
                   const boost::shared_ptr<boost::asio::streambuf> request_ptr,
                   boost::shared_ptr<boost::asio::streambuf> response_ptr,
                   boost::shared_ptr<ResponseMeta> meta_ptr):request_ptr(request_ptr),
                   response_ptr(response_ptr),
                   meta_ptr(meta_ptr),
                   resolver(io_service),
                   socket(io_service){}
};


class BaseClient {

public:
    BaseClient();
    ~BaseClient();
    void AsyncRequest(const std::string& server,
                      const std::string& protocol,
                      const boost::shared_ptr<boost::asio::streambuf> request_ptr,
                      boost::shared_ptr<boost::asio::streambuf> response_ptr,
                      boost::shared_ptr<ResponseMeta> meta_ptr,
                      Callback callback);
    void Run();
private:
    void HandleResolveCompleted(const boost::system::error_code& err,
                       boost::asio::ip::tcp::resolver::iterator iterator,
                       boost::shared_ptr<RequestContext> context_ptr);
    void HandleReadStatusLine(const boost::system::error_code& err,
                              boost::shared_ptr<RequestContext> context_ptr);
    void HandleReadHead(const boost::system::error_code& err,
                        boost::shared_ptr<RequestContext> context_ptr);
    void HandleConnectCompleted(const boost::system::error_code& err,
                       boost::shared_ptr<RequestContext> context_ptr);
    void HandleWriteRequestCompleted(const boost::system::error_code& err,
                            boost::shared_ptr<RequestContext> context_ptr);
    void HandleReadBody(const boost::system::error_code& err,
                            boost::shared_ptr<RequestContext> context_ptr);
private:
    boost::asio::io_service io_service_;
};

}
}
#endif
