// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "master/http_client.h"

#include <sstream>
#include <ostream>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include "logging.h"


namespace baidu {
namespace lumia {

HttpClient::HttpClient() {

}


bool HttpClient::SyncGet(const HttpGetRequest& request,
                         HttpResponse* response) {
    Url url;
    bool ok = ParseUrl(request.url, &url);
    if (!ok) {
        return false;
    } 
    boost::asio::streambuf req_buf;
    boost::asio::streambuf resp_buf;

}

bool HttpClient::BuildGetReq(const HttpGetRequest& req,
                             boost::asio::streambuf& req_buf) {

}

bool HttpClient::AsyncPost(const boost::shared_ptr<HttpPostRequest> request_ptr,
                           boost::shared_ptr<HttpResponse> response_ptr,
                           HttpCallback callback) {
    boost::shared_ptr<boost::asio::streambuf> request_buffer(new boost::asio::streambuf());
    Url url;
    bool ok = ParseUrl(request_ptr->url, &url);
    if (!ok) {
        return false;
    }
    ok = BuildPostForm(request_ptr, url, request_buffer);
    if (!ok) {
        return ok;
    }
    boost::shared_ptr<boost::asio::streambuf> response_buffer(new boost::asio::streambuf());
    boost::shared_ptr<ResponseMeta> meta_ptr(new ResponseMeta());
    Callback asio_callback = boost::bind(&HttpClient::HandleCallback, this, 
                                         callback, response_ptr, _1, _2, _3, _4);
    worker_.AddTask(boost::bind(&HttpClient::Execute, this, url, request_buffer,
                              response_buffer, meta_ptr, asio_callback));
    return true;
}

void HttpClient::Execute(const Url url,
                         const boost::shared_ptr<boost::asio::streambuf> request_buffer,
                         const boost::shared_ptr<boost::asio::streambuf> response_buffer,
                         const boost::shared_ptr<ResponseMeta> meta_ptr,
                         Callback callback) {
    LOG(INFO, "start req to %s", url.server.c_str());
    base_client_.AsyncRequest(url.server,
                              url.protocol,
                              request_buffer, 
                              response_buffer,
                              meta_ptr,
                              callback);
    base_client_.Run();
}

void HttpClient::HandleCallback(HttpCallback callback,
                                boost::shared_ptr<HttpResponse> response_ptr,
                                boost::shared_ptr<boost::asio::streambuf> request_buffer,
                                boost::shared_ptr<boost::asio::streambuf> response_buffer,
                                boost::shared_ptr<ResponseMeta> meta_ptr,
                                int32_t status) {
    if (status != 0) {
        response_ptr->error= "error";
    } else {
        std::ostringstream os;
        os << response_buffer.get();
        response_ptr->body = os.str();
    }
    callback(response_ptr);
}


bool HttpClient::BuildPostForm(const boost::shared_ptr<HttpPostRequest> request_ptr,
                               const Url& url,
                               boost::shared_ptr<boost::asio::streambuf> request_buffer) {
    std::ostream os(request_buffer.get());
    std::stringstream body;
    std::vector<std::pair<std::string, std::string> >::iterator it = request_ptr->data.begin();
    os << "POST "<< url.path << " HTTP/1.1\r\n";
    os << "Host: "<< url.server << "\r\n";
    os << "Content-Type: application/x-www-form-urlencoded\r\n";
    int flag = 0;
    for (; it != request_ptr->data.end(); ++it) {
        if (flag >0) {
            body << "&";
        }
        std::string encoded_str;
        UrlEncode(it->second, &encoded_str);
        body << it->first << "=" << encoded_str;
        ++flag;
    }

    std::vector<std::string>::iterator header_it = request_ptr->headers.begin();
    for (; header_it != request_ptr->headers.end(); ++header_it) {
        os << *header_it << "\r\n";
    }
    std::string encoded_body = body.str();
    os << "Content-Length: " << encoded_body.length() << "\r\n";
    os << "User-Agent: Lumia Master\r\n";
    os << "Connection: close\r\n";
    os << "Accept: *\r\n";
    os << "\r\n";
    os << encoded_body;
    return true;
}

void HttpClient::UrlEncode(const std::string& s, std::string* output) {
	const char *str = s.c_str();
    std::vector<char> v(s.size());
    v.clear();
    for (size_t i = 0, l = s.size(); i < l; i++) {
        char c = str[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')') {
            v.push_back(c);
        } else if (c == ' ') {
            v.push_back('+');
        } else {
            v.push_back('%');
            unsigned char d1, d2;
            d1 = c / 16;
    		d2 = c % 16;
    		d1 += d1 <= 9 ? '0' : 'a' - 10;
    		d2 += d2 <= 9 ? '0' : 'a' - 10;
            v.push_back(d1);
            v.push_back(d2);
        }
    }
    std::string ret(v.begin(), v.end());
    *output = ret;
}

bool HttpClient::ParseUrl(const std::string& surl, Url* url) {
    std::size_t protocol_pos = surl.find_first_of(':', 0);
    // http: or https: 
    if (protocol_pos != 4 && protocol_pos != 5) {
        LOG(WARNING, "invalid url %s protocol_pos is %d", surl.c_str(), protocol_pos);
        return false;
    }
    url->protocol = surl.substr(0, protocol_pos);
    if (url->protocol != "http" 
        && url->protocol != "https") {
        LOG(WARNING, "invalid url %s with protocol %s", surl.c_str(), url->protocol.c_str());
        return false;
    }
    std::size_t end_pos = surl.find_first_of("/", protocol_pos + 3);
    if (end_pos == std::string::npos) {
        if ((protocol_pos + 3) > surl.length()) {
            return false;
        }
        url->server = surl.substr(protocol_pos + 3, surl.length());
        url->path = "/";
    }else {
        url->server = surl.substr(protocol_pos + 3, end_pos - (protocol_pos + 3));
        url->path = surl.substr(end_pos, std::string::npos);
    }
    return true;
}



}
}
