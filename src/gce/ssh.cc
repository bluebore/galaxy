// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "gflags/gflags.h"
#include "proto/agent.pb.h"
#include "proto/initd.pb.h"
#include "rpc/rpc_client.h"
#include "tprinter.h"
#include "string_util.h"
#include "logging.h"

#include "libwebsockets.h"
DEFINE_string(initd_endpoint, "", "initd endpoint");
DEFINE_string(user, "", "use user");
DEFINE_string(chroot, "", "chroot path");
DEFINE_string(LINES, "30", "env values");
DEFINE_string(COLUMNS, "80", "env values");
DEFINE_string(TERM, "xterm-256color", "env values");
DECLARE_string(agent_port);
DECLARE_string(agent_default_user);
DECLARE_string(flagfile);
DECLARE_int32(cli_server_port);
DEFINE_string(pod_id, "", "pod id");

using baidu::common::INFO;
using baidu::common::WARNING;


bool PreparePty(int* fdm, std::string* pty_file) {
    if (pty_file == NULL || fdm == NULL) {
        return false; 
    }
    *fdm = -1;
    *fdm = ::posix_openpt(O_RDWR);
    if (*fdm < 0) {
        LOG(WARNING, "posix_openpt err[%d: %s]",
                errno, strerror(errno)); 
        return false;
    }

    int ret = ::grantpt(*fdm);
    if (ret != 0) {
        LOG(WARNING, "grantpt err[%d: %s]", 
                errno, strerror(errno)); 
        ::close(*fdm);
        return false;
    }

    ret = ::unlockpt(*fdm);
    if (ret != 0) {
        LOG(WARNING, "unlockpt err[%d: %s]", 
                errno, strerror(errno));
        ::close(*fdm);
        return false;
    }
    pty_file->clear();
    pty_file->append(::ptsname(*fdm));
    return true;
}


int GetPodFd(std::string pod_id) {
    ::baidu::galaxy::Agent_Stub* agent;    
    ::baidu::galaxy::RpcClient* rpc_client = 
        new ::baidu::galaxy::RpcClient();
    std::string endpoint("127.0.0.1:");
    endpoint.append(FLAGS_agent_port);
    rpc_client->GetStub(endpoint, &agent);

    ::baidu::galaxy::ShowPodsRequest request;
    ::baidu::galaxy::ShowPodsResponse response;
    request.set_podid(pod_id);
    bool ret = rpc_client->SendRequest(agent,
            &::baidu::galaxy::Agent_Stub::ShowPods,
            &request,
            &response, 5, 1);
    if (!ret) {
        LOG(WARNING, "rpc failed\n");
        return -1;
    } else if (response.has_status()
            && response.status() != ::baidu::galaxy::kOk) {
        LOG(WARNING, "response status %s\n",
                ::baidu::galaxy::Status_Name(response.status()).c_str()); 
        return -1;
    }
    if (response.pods_size() != 1) {
       LOG(WARNING, "pod size not 1[%d]\n", 
                        response.pods_size()); 
        return -1;
    }
    const ::baidu::galaxy::PodPropertiy& pod = response.pods(0);

    FLAGS_initd_endpoint = pod.initd_endpoint();
    FLAGS_chroot = pod.pod_path();
    ::baidu::galaxy::Initd_Stub* initd;
    std::string initd_endpoint(FLAGS_initd_endpoint);
    rpc_client->GetStub(initd_endpoint, &initd);

    std::string pty_file;
    int pty_fdm = -1;
    if (!PreparePty(&pty_fdm, &pty_file)) {
        LOG(WARNING, "prepare pty failed\n"); 
        return -1;
    }

    baidu::galaxy::ExecuteRequest exec_request;
    // TODO unqic key 
    exec_request.set_key("client");
    exec_request.set_commands("/bin/bash");
    exec_request.set_path(".");
    exec_request.set_pty_file(pty_file);
    if (FLAGS_user != "") {
        exec_request.set_user(FLAGS_user);
    }
    if (FLAGS_chroot != "") {
        LOG(INFO, "%s\n", FLAGS_chroot.c_str());
        exec_request.set_chroot_path(FLAGS_chroot); 
    }
    std::string* lines_env = exec_request.add_envs();
    lines_env->append("LINES=");
    lines_env->append(FLAGS_LINES);
    std::string* columns_env = exec_request.add_envs();
    columns_env->append("COLUMNS=");
    columns_env->append(FLAGS_COLUMNS);
    std::string* xterm_env = exec_request.add_envs();
    xterm_env->append("TERM=");
    xterm_env->append(FLAGS_TERM);
    baidu::galaxy::ExecuteResponse exec_response;
    ret = rpc_client->SendRequest(initd,
                            &baidu::galaxy::Initd_Stub::Execute,
                            &exec_request,
                            &exec_response, 5, 1);
    if (ret && exec_response.status() == baidu::galaxy::kOk) {
        LOG(INFO, "terminate starting...\n");
        return pty_fdm;
    } 
    LOG(WARNING, "exec in initd failed %s\n", 
            baidu::galaxy::Status_Name(exec_response.status()).c_str());
    return -1;
}


std::map<int, struct libwebsocket*> pty2wsi;

struct session {
    int pty_fd;
};

struct libwebsocket_pollfd pollfds[1000]; // suopport max 500 connection
int pollfd_cnt = 0;

// remove some fd from the poll list
void ClearPollFd(int fd) { 
    int n = 0;
    for (; n < pollfd_cnt; n++) {
        if (pollfds[n].fd == fd) {
            for (; n < pollfd_cnt - 1; n++)
                pollfds[n] = pollfds[n + 1];
            break;
        }
    }
    pollfd_cnt = n;
}

static int callback_http(struct libwebsocket_context* context, 
                         struct libwebsocket* wsi,
                         enum libwebsocket_callback_reasons reason,
                         void* user, void* in, size_t len) {
    struct session* pss = (session*)user;
    struct libwebsocket_pollargs* pa = (struct libwebsocket_pollargs*)in;
    LOG(INFO, "HTTP");
    switch (reason) {
    case LWS_CALLBACK_ADD_POLL_FD:
        pollfds[pollfd_cnt].fd = pa->fd;
        pollfds[pollfd_cnt].events = pa->events;
        pollfds[pollfd_cnt++].revents = 0;
        break;
    case LWS_CALLBACK_DEL_POLL_FD:
        ClearPollFd(pa->fd);
        if (pss->pty_fd != -1) {
            LOG(INFO, "close pty fd %d", pss->pty_fd);
            close(pss->pty_fd);
            ClearPollFd(pss->pty_fd);
            pty2wsi.erase(pss->pty_fd);
            pss->pty_fd = -1;
        }
        break;
    case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
        for (int i = 0; i < pollfd_cnt; i ++) { //too complicated
            if (pollfds[i].fd == pa->fd) {
                pollfds[i].events = pa->events;
                break;
            }
        }
        break;
    default:
        break;
    }
    return 0;
}

int writable = 0;
const int BUF_LEN = 1024 * 1000;
unsigned char buf_[LWS_SEND_BUFFER_PRE_PADDING + BUF_LEN + 
                      LWS_SEND_BUFFER_POST_PADDING];
unsigned char *buf = &buf_[LWS_SEND_BUFFER_PRE_PADDING];

static int callback_terminal_contact(struct libwebsocket_context* context, 
                                      struct libwebsocket* wsi,
                                      enum libwebsocket_callback_reasons reason,
                                      void* user, void* in, size_t len) {

    session* pss = (session*)user;
    int ret = 0;
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        LOG(INFO, "connection established");
        pss->pty_fd = -1;
        break;
    case LWS_CALLBACK_RECEIVE:
        if (pss->pty_fd == -1) {
            if (len != 36) {
                LOG(WARNING, "wrong header %.*s", len, (char*)in);
                break;
            }
            std::string pod_id((char*)in, 36);
            LOG(INFO, "receive pod-id %s", pod_id.c_str());
            int pty = pss->pty_fd= GetPodFd(pod_id);
            if (pty < 0) {
                break;
            }
            pty2wsi[pty] = wsi;
            pollfds[pollfd_cnt].fd = pty;
            pollfds[pollfd_cnt].events = POLLIN;
            pollfds[pollfd_cnt++].revents = 0;
        }
        else {
            if (pss->pty_fd > 0) {
                write(pss->pty_fd, in, len);
            }
        }
        break;
    case LWS_CALLBACK_SERVER_WRITEABLE:
        break; break; break;
        //it doesn't work
        if (writable == 0) break;
        LOG(INFO, "read from pty fd %d ", pss->pty_fd);
        // following might block due to the stupid libwebsockets mechanism
        if (pss->pty_fd != -1) { 
            ret = read(pss->pty_fd, buf, 1024 * 10);
            writable = 0; 
            LOG(INFO, "read size %d", ret);
            if (ret <= 0) {
                buf[0] = 0x00;
                libwebsocket_write(wsi, buf, 1, LWS_WRITE_TEXT);
                close(pss->pty_fd);
                ClearPollFd(pss->pty_fd);
                pty2wsi.erase(pss->pty_fd);
                pss->pty_fd = -1;
            }
            else libwebsocket_write(wsi, buf, ret, LWS_WRITE_TEXT);
        }
        else {
            LOG(WARNING, "write without callback");
        }
        break;
    default:
        break;
    }
    return 0;
}


static struct libwebsocket_protocols protocols[] = {
    {
        "http-only",
        callback_http,
        0
    },
    {
        "terminal-contact",
        callback_terminal_contact,
        sizeof(struct session)
    },
    {NULL, NULL, 0}
};

int force_exit = 0;
void sighandler(int sig) {
	force_exit = 1;
}


int main(int argc, char* argv[]) {
    FLAGS_flagfile = "galaxy.flag";
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    
    lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = FLAGS_cli_server_port;
    info.protocols = protocols;
    info.extensions = libwebsocket_get_internal_extensions();
    info.gid = info.uid = -1;
    libwebsocket_context* context = libwebsocket_create_context(&info);
    assert(context != NULL);
    signal(SIGINT, sighandler);
    while (!force_exit) {
        int ret = poll(pollfds, pollfd_cnt, -1);
        LOG(INFO, "new event, poll size %d", pollfd_cnt);
        if (ret < 0) 
            continue;
        for (int i = 0; i < pollfd_cnt; i ++) {
            if (pollfds[i].revents) {
                libwebsocket_service_fd(context, &pollfds[i]);
                if (pollfds[i].revents) {
                    int fd = pollfds[i].fd;
                    if (pty2wsi.count(fd)) {
                        LOG(INFO, "to read from a pty and write back to socket");
                        //libwebsocket_callback_on_writable(context, pty2wsi[pollfds[i].fd]);
                        
                        //it might cause some trouble to write directly 
                        LOG(INFO, "read from pty: fd %d ", fd);
                        if (fd) { 
                            int ret = read(fd, buf, BUF_LEN);
                            LOG(INFO, "read from pty: size %d", ret);
                            if (ret <= 0) {
                                //send a one byte response to tell client to close websocket
                                libwebsocket_write(pty2wsi[fd], buf, 1, LWS_WRITE_BINARY);
                                ClearPollFd(fd); i --;
                                pty2wsi.erase(fd);
                                LOG(INFO, "close connection to pty fd %d", fd);
                            }
                            else libwebsocket_write(pty2wsi[fd], buf, ret, LWS_WRITE_TEXT);
                        }
                    }
                    else {
                        LOG(WARNING, "no wsi, it's bad");
                    }
                }
            }
        }
    }
    libwebsocket_context_destroy(context);
    return 0;
}


