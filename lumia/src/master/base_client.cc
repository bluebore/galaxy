// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base_client.h"
#include "logging.h"
#include <iostream>
#include <sstream>
namespace baidu {
namespace lumia {

BaseClient::BaseClient():io_service_(){

}

BaseClient::~BaseClient(){}

void BaseClient::Run() {
    io_service_.run();
}

void BaseClient::AsyncRequest(const std::string& server,
                              const std::string& protocol,
                              const boost::shared_ptr<boost::asio::streambuf> request_ptr,
                              boost::shared_ptr<boost::asio::streambuf> response_ptr,
                              boost::shared_ptr<ResponseMeta> meta_ptr,
                              Callback callback) {
    boost::asio::ip::tcp::resolver::query query(server, protocol);
    boost::shared_ptr<RequestContext> context_ptr(new RequestContext(io_service_,
                                    request_ptr, response_ptr, meta_ptr));
    context_ptr->callback = callback;
    context_ptr->server = server;
    context_ptr->resolver.async_resolve(query,
            boost::bind(&BaseClient::HandleResolveCompleted, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator,
            context_ptr));
}

void BaseClient::SyncRequest(const std::string& server,
                             const std::string& protocol,
                             const boost::asio::streambuf& request,
                             boost::asio::streambuf& response,
                             ResponseMeta& meta) {
    boost::asio::ip::tcp::resolver resolver(io_service_);
    boost::asio::ip::tcp::resolver::query query(server, protocol);
    boost::asio::ip::tcp::resolver::iterator endpoint_it = resolver.resolve(query);
    boost::asio::ip::tcp::socket socket(io_service_);
    boost::asio::connect(socket, endpoint_it);
    boost::asio::write(socket, request);
    boost::asio::read_util(socket, response, "\r\n");
    std::istream response_stream(&response);
    response_stream >> meta.http_version;
    response_stream >> meta.status_code;
    std::getline(response_stream, meta.http_msg);
    boost::asio::read_until(socket, response, "\r\n\r\n");
    std::string header;
    while (std::getline(response_stream, header) && header != "\r") {
        meta.headers.push_back(header);
    }
    if (response.size() > 0) {
        std::ostringstream os;
        os << &response;
        LOG(WARNING, "unused chars %s ", os.str().c_str()); 
    }
    boost::system::error_code error;
    while (boost::asio::read(socket, response,
     boost::asio::transfer_at_least(1), error)) {}
    if (error == boost::asio::error::eof) {
        return true;
    }
    return false;
}

void BaseClient::HandleResolveCompleted(const boost::system::error_code& err,
                               boost::asio::ip::tcp::resolver::iterator iterator,
                               boost::shared_ptr<RequestContext> context_ptr) {
    LOG(INFO, "relove completed");
    if (!err) {
        boost::asio::async_connect(context_ptr->socket, iterator,
                                   boost::bind(&BaseClient::HandleConnectCompleted, this,
                                   boost::asio::placeholders::error,
                                   context_ptr));
    }else {
        LOG(WARNING, "fail to resolve for err %d", err);
        context_ptr->callback(context_ptr->request_ptr,
                              context_ptr->response_ptr,
                              context_ptr->meta_ptr, -1);
    }
}

void BaseClient::HandleConnectCompleted(const boost::system::error_code& err,
                                        boost::shared_ptr<RequestContext> context_ptr)  {
    LOG(INFO, "connect completed");
    if (!err) {
        boost::asio::async_write(context_ptr->socket, 
                                *context_ptr->request_ptr,
                                boost::bind(&BaseClient::HandleWriteRequestCompleted, this,
                                boost::asio::placeholders::error,
                                context_ptr));

    } else {
        context_ptr->callback(context_ptr->request_ptr,
                              context_ptr->response_ptr,
                              context_ptr->meta_ptr, -1);

    }
}

void BaseClient::HandleWriteRequestCompleted(const boost::system::error_code& err,
                                             boost::shared_ptr<RequestContext> context_ptr) {
    LOG(INFO, "write request completed");
    if (!err) {
        boost::asio::async_read_until(context_ptr->socket, 
                                      *context_ptr->response_ptr,
                                      "\r\n",
                                      boost::bind(&BaseClient::HandleReadStatusLine, this,
                                      boost::asio::placeholders::error,
                                      context_ptr));
    } else {
        context_ptr->callback(context_ptr->request_ptr,
                              context_ptr->response_ptr, 
                              context_ptr->meta_ptr,
                              -1);
    }
}

void BaseClient::HandleReadStatusLine(const boost::system::error_code& err,
                                  boost::shared_ptr<RequestContext> context_ptr) {
    LOG(INFO, "read status completed");
    if (!err) {
        std::istream response_stream(context_ptr->response_ptr.get());
        std::string http_version;
        response_stream >> context_ptr->meta_ptr->http_version;
        response_stream >> context_ptr->meta_ptr->status;
        std::getline(response_stream, context_ptr->meta_ptr->http_msg);
        boost::asio::async_read_until(context_ptr->socket, 
                                      *context_ptr->response_ptr,
                                      "\r\n\r\n",
                                      boost::bind(&BaseClient::HandleReadHead, this,
                                      boost::asio::placeholders::error,
                                      context_ptr));

    }else {
        context_ptr->callback(context_ptr->request_ptr,
                              context_ptr->response_ptr, 
                              context_ptr->meta_ptr,
                              -1);
    }
}

void BaseClient::HandleReadHead(const boost::system::error_code& err,
                                boost::shared_ptr<RequestContext> context_ptr) {
    LOG(INFO, "read head completed");
    if (!err) {
        std::istream response_stream(context_ptr->response_ptr.get());
        std::string header;
        while (std::getline(response_stream, header) && header != "\r") {
            context_ptr->meta_ptr->headers.push_back(header);
        }
        if (context_ptr->response_ptr->size() > 0) {
            std::ostringstream os;
            os << context_ptr->response_ptr.get();
            LOG(WARNING, "unused chars %s ", os.str().c_str());
        }
        boost::asio::async_read(context_ptr->socket,
                                *context_ptr->response_ptr,
                                boost::asio::transfer_at_least(1),
                                boost::bind(&BaseClient::HandleReadBody, this,
                                boost::asio::placeholders::error,
                                context_ptr));
    }
}

void BaseClient::HandleReadBody(const boost::system::error_code& err,
                                boost::shared_ptr<RequestContext> context_ptr) {
    if (!err) {
        boost::asio::async_read(context_ptr->socket, 
                              *context_ptr->response_ptr,
                              boost::asio::transfer_at_least(1),
                              boost::bind(&BaseClient::HandleReadBody, this,
                              boost::asio::placeholders::error,
                              context_ptr));

    } else if (err == boost::asio::error::eof) {
        LOG(INFO, "read body completed");
        context_ptr->callback(context_ptr->request_ptr,
                              context_ptr->response_ptr,
                              context_ptr->meta_ptr,
                              0);
    } else {
        context_ptr->callback(context_ptr->request_ptr,
                              context_ptr->response_ptr,
                              context_ptr->meta_ptr,
                              -1);
    }
}

}
}
