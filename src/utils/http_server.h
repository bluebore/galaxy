// Copyright (c) 2014, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: sunjunyi01@baidu.com

#ifndef  BAIDU_GALAXY_HTTPSERVER_H_
#define  BAIDU_GALAXY_HTTPSERVER_H_

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <tprinter.h>
#include <algorithm>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <string>
#include <vector>
#include <string_util.h>
#include "logging.h"
#include "mutex.h"
#include "thread_pool.h"

namespace baidu{
namespace galaxy{

static bool KeyCompare(const std::pair<std::string, std::string>& key1, const std::pair<std::string, std::string>& key2) {
    return key1.first.length() < key2.first.length();
}

class HttpFileServer {
public:
    const static off_t TAIL_LIMIT = 8000; //bytes
    HttpFileServer(const std::vector<std::pair<std::string, std::string> >& router,
                   int port) : router_(router),
                               port_(port),
                               pool_(NULL),
                               listen_socket_(0),
                               stop_(false) {
        std::sort(router_.begin(), router_.end(), KeyCompare);
        for (size_t i = 0; i < router_.size(); i++) {
            reversed_router_.push_back(std::make_pair(router_[i].second, router_[i].first));
        }
        std::sort(reversed_router_.begin(), reversed_router_.end(), KeyCompare);
    }

    ~HttpFileServer() {
        if (listen_socket_ > 0) {
            close(listen_socket_);
        }
        stop_ = true;
        LOG(INFO, "wait threads");
        delete pool_;
        LOG(INFO, "~HttpFileServer()");
    }

    void Start(int thread_num) {
        assert(thread_num > 0);
        pool_ = new ThreadPool(thread_num+1);
        pool_->AddTask(boost::bind(&HttpFileServer::Run, this));
    }

    int Run() {
        fd_set readfd;
        int on;
        sockaddr_in server_addr, client_addr;
        ::signal(SIGPIPE, SIG_IGN);
        listen_socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket_ < 0) {
            LOG(WARNING, "create listen socket fail.");
            perror("create socket faild");
            return -1;
        }
        on = 1;
        setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = ::htons(port_);
        server_addr.sin_addr.s_addr = ::inet_addr("0.0.0.0");
        if (::bind(listen_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) < 0 ) {
            LOG(WARNING, "bind to %d faild", port_);
            perror("bind fail");
            return -1;
        }
        if (::listen(listen_socket_, 32) < 0) {
            LOG(WARNING, "listen faild");
            return -1;
        }
        while (!stop_) {
            struct timeval timeout={2,0};
            FD_ZERO(&readfd);
            FD_SET(listen_socket_, &readfd);
            ::select(listen_socket_ + 1, &readfd, NULL, NULL, &timeout);
            if (!FD_ISSET(listen_socket_, &readfd)) {
                continue;
            }
            socklen_t sock_len;
            int client_fd = ::accept(listen_socket_, (sockaddr*)&client_addr, &sock_len);
            if (client_fd < 0 ){
                LOG(WARNING, "invalid client fd:%d", client_fd);
                continue;
            }
            pool_->AddTask(boost::bind(&HttpFileServer::HandleClient, this, 
                           client_addr, client_fd));
        }
        return 0;
    }

private:
    struct FileInfo {
        std::string Size;
        std::string LastModified;
    };

    class Session {
    public:
        Session(FILE* in_stream, 
                FILE* out_stream,
                HttpFileServer* server) : in_stream_(in_stream),
                                    out_stream_(out_stream),
                                    server_(server){
            setlinebuf(in_stream_);
            setlinebuf(out_stream_);
        }
        ~Session() {
            fclose(out_stream_);
            ::shutdown(fileno(out_stream_), SHUT_RDWR);
            fclose(in_stream_);
        }

        bool IsDir(const std::string& path, int *err) {
            struct stat stat_buf;
            int ret = lstat(path.c_str(), &stat_buf);
            *err = ret;
            if (ret == -1) {
                LOG(WARNING, "stat path %s failed err[%d: %s]",
                    path.c_str(),
                    errno,
                    strerror(errno));
            }
            if (S_ISDIR(stat_buf.st_mode)) {
                return true;
            } else {
                return false;
            }
        }

        std::string ReadableFileSize(double size) {
            int i = 0;
            char buf[1024] = {'\0'};
            const char* units[] = {"B", "KB", "MB", "GB", "TB", 
                                   "PB", "EB", "ZB", "YB"};
            while (size > 1024) {
                size /= 1024;
                i++;
            }
            snprintf(buf, sizeof(buf), "%.*f%s", i, size, units[i]);
            return buf;
        }

        bool GetFileInfo(const char* file_name, FileInfo& info) {
            struct stat stat_buf;
            struct tm tm_info;
            char time_buf[1024] = {'\0'};
            int ret = lstat(file_name, &stat_buf);
            if (ret != 0) {
                return false;
            }
            info.Size = ReadableFileSize(stat_buf.st_size);
            time_t modified_time = stat_buf.st_mtime;
            struct tm* l_time = localtime_r(&modified_time, &tm_info);
            if (l_time) {
                strftime(time_buf, sizeof(time_buf),
                        "%d-%b-%y %H:%M:%S", l_time);
                info.LastModified = time_buf;
            }
            return true;
        }

        bool ShowDir(int file_fd, std::string& path) {
            std::vector<std::string> children;
            std::string out;
            std::string current_folder_url;
            server_->GenerateUrl(path, &current_folder_url);
           /* if (ok) {
                LOG(WARNING, "can GenerateUrl for path %s", path.c_str());
                return false;
            }*/
            ListDir(path, children);
            out += "<html><head><title>" + current_folder_url +"</title></head> <body>";
            out += "<h1>Index of " + current_folder_url +"</h1>";
            out += "<table> <thead> <tr><th style=\"text-align:left\">Name</th><th style=\"text-align:left\">Lastmodified</th> <th style=\"text-align:left\">Size</th> <th style=\"text-align:left\">Tail</th></tr> </thead><tbody>";
            for (size_t i=0; i < children.size(); i++) {
                std::string anchor = children[i];
                if (anchor == "." || anchor == "..") {
                    continue;
                }
                std::string row = "<tr>";
                std::string full_path;
                server_->SafePathJoin(path, anchor, &full_path);
                std::string href = "";
                server_->GenerateUrl(full_path, &href);
                row += "<td><a href=\"" + href + "\">" + anchor + "</a></td>" ;
                std::string tail_button = "";
                FileInfo info;
                int stat_err = 0;
                GetFileInfo(full_path.c_str(), info);
                row += "<td>" + info.LastModified + "</td>";
                if (!IsDir(full_path, &stat_err) && stat_err == 0) {
                    tail_button = "<a href=\"" + href 
                                  + "?tail\"> ... </a>";
                } else{
                    info.Size = "-";
                }
                row += "<td>" + info.Size + "</td>";
                row += "<td>" + tail_button + "</td>";
                row += "</tr>";
                out += row;
            }
            out += "</tbody></table></body></html>";
            fprintf(out_stream_, "HTTP/1.1 200 OK\n");
            fprintf(out_stream_, "Content-Type: text/html\n");
            fprintf(out_stream_, "Content-Length: %lu\n", out.size());
            fprintf(out_stream_, "Server: Galaxy\n");
            fprintf(out_stream_, "Connection: Close\n\n");
            fprintf(out_stream_, "%.*s", static_cast<int>(out.size()), out.data());
            close(file_fd);
            return true;
        }

        void SendFile(int file_fd, off_t file_size, off_t start_pos){
            off_t transfer_bytes = 0;
            int sock_fd = fileno(out_stream_);
            off_t pos = start_pos;
            fprintf(out_stream_, "HTTP/1.1 200 OK\n");
            fprintf(out_stream_, "Content-Length: %lu\n", file_size - start_pos);
            fprintf(out_stream_, "Server: Galaxy\n");
            fprintf(out_stream_, "Connection: Close\n\n");
            while (transfer_bytes < file_size - start_pos) {
                ssize_t ret = ::sendfile(sock_fd, file_fd, &pos, file_size - transfer_bytes);
                if (ret < 0) {
                    LOG(WARNING, "failed to sendfile");
                    perror("");
                    break;
                }
                transfer_bytes += ret;   
            }
            close(file_fd);
        }

        void Error(const char* msg) {
            LOG(WARNING, "msg: %s", msg);
            fprintf(out_stream_, "%s\n", msg);
        }

        private:
            FILE* in_stream_;
            FILE* out_stream_;
            HttpFileServer* server_;
    };

    static void ListDir(const std::string& path, std::vector<std::string>& children ) {
        DIR  *d;
        struct dirent *dir;
        d = opendir(path.c_str());
        if (d) {
            while ((dir = readdir(d)) != NULL)
            {
                children.push_back(dir->d_name);
            }
            closedir(d);
        }
        std::sort(children.begin(), children.end());
    }
    bool SafePathJoin(const std::string& parent, const std::string& child, std::string* full_path) {
        if (full_path == NULL) {
            return false;
        }
        if (parent.length() <= 0) {
            return false;
        }
        std::string copied_parent = parent;
        if (copied_parent[copied_parent.length() - 1] == '/') {
            copied_parent = copied_parent.substr(0, copied_parent.length() - 1); 
        }
        std::string copied_child = child;
        if (copied_child[0] == '/') {
            copied_child = copied_child.substr(1);
        }
        *full_path = copied_parent + "/" + copied_child;
        return true;
    }
    bool GenerateRealpath(std::string url_path, std::string* real_path) {
        if (real_path == NULL) {
            return false;
        }
        std::vector<std::pair<std::string, std::string> >::iterator router_it = router_.begin();
        for (; router_it !=  router_.end(); ++router_it) {
            LOG(INFO, "url path %s router key %s", url_path.c_str(),router_it->first.c_str());
            if (url_path.find(router_it->first) == 0) {
                std::string child = url_path.replace(0, router_it->first.length(),"");
                bool ok = SafePathJoin(router_it->second, child, real_path);
                if (!ok) {
                    LOG(WARNING, "fail to join path ,parent %s child %s", router_it->second.c_str(), child.c_str());
                    return false;
                }
                LOG(INFO, "url_path %s is rewrote to %s", url_path.c_str(), real_path->c_str());
                return true;
            }
        }
        return false;
    }

    bool GenerateUrl(std::string real_path, std::string* url_path) {
        if (url_path == NULL) {
            return false;
        }
        std::vector<std::pair<std::string, std::string> >::iterator reversed_router_it = reversed_router_.begin();
        for (; reversed_router_it !=  reversed_router_.end(); ++reversed_router_it) {
            LOG(INFO, "real path %s router key %s", real_path.c_str(), reversed_router_it->first.c_str());
            if (real_path.find(reversed_router_it->first) == 0) {
                std::string child = real_path.replace(0, reversed_router_it->first.length(),"");
                bool ok = SafePathJoin(reversed_router_it->second, child, url_path);
                if (!ok) {
                    LOG(WARNING, "fail to join path , parent %s, child %s", reversed_router_it->second.c_str(), child.c_str());
                    return false;
                }
                LOG(INFO, "real_path %s is rewrote  to %s", real_path.c_str(), url_path->c_str());
                return true;
            }
        }
        return false;
    }

    void HandleClient(sockaddr_in /*client_addr*/,
                      int client_fd) {
        char header[256] = {'\0'};
        char method[256] = {'\0'};
        char path[256] = {'\0'};
        char version[256] = {'\0'};
        FILE* in_stream = ::fdopen(client_fd, "r");
        FILE* out_stream = ::fdopen(dup(client_fd), "w");
        if (!in_stream || !out_stream) {
            perror("call fdopen failed");
            close(client_fd);
            return;
        }
        Session session(in_stream, out_stream, this); 
        fgets(header, sizeof(header), in_stream);
        if (sscanf(header, "%s %s %s", method, path, version) == 3){
            std::string real_path;
            std::string http_path(path);
            bool tail_only = false;
            std::size_t found = http_path.rfind("?tail");
            if (found != std::string::npos) {
                tail_only = true;
                http_path.erase(found);
            }
            bool ok = GenerateRealpath(http_path, &real_path);
            if (!ok) {
                LOG(WARNING, "can not rewrite url %s", http_path.c_str());
                std::string html = "<html> <head><title></title></head><body>";
                for (size_t i = 0; i < router_.size(); i++) {
                    html += "<a href=\""+ router_[i].first + "\">" + router_[i].first + "</a> <br>";
                }
                html += "</body></html>";
                session.Error(html.c_str());
                return;
            }
            if (::access(real_path.c_str(), F_OK) == 0) {
                int file_fd = ::open(real_path.c_str(), O_RDONLY);
                struct stat stat_buf;
                fstat(file_fd, &stat_buf);
                if (S_ISDIR(stat_buf.st_mode)) {
                    LOG(INFO, "show dir %s", real_path.c_str());
                    session.ShowDir(file_fd, real_path);
                    return ;
                }
                off_t file_size = stat_buf.st_size;
                off_t start_pos = 0;
                if (tail_only && file_size > TAIL_LIMIT) {
                    start_pos = (file_size - TAIL_LIMIT);
                }
                session.SendFile(file_fd, file_size, start_pos);
            } else {
                LOG(WARNING, "%s can not be found", real_path.c_str());
                session.Error("file not exists\n");
            }
        } else {
            LOG(WARNING, "bad header: %s", header);
            session.Error("bad method");
        }
    }
private:
    std::vector<std::pair<std::string, std::string> > router_;
    std::vector<std::pair<std::string, std::string> > reversed_router_;
    int port_;
    ThreadPool* pool_;
    int listen_socket_;
    bool stop_;
};

} // galaxy
} // baidu

#endif

