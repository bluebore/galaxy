/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file src/agent/cgroup/tcp_throt_subsystem.cc
 * @author haolifei(com@baidu.com)
 * @date 2016/04/25 15:13:03
 * @brief 
 *  
 **/

#include "tcp_throt_subsystem.h"
#include "protocol/galaxy.pb.h"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

namespace baidu {
    namespace galaxy {
        namespace cgroup {

            TcpThrotSubsystem::TcpThrotSubsystem() {}
            TcpThrotSubsystem::~TcpThrotSubsystem() {}

            std::string TcpThrotSubsystem::Name() {
                return "tcp_throt";
            }

            int TcpThrotSubsystem::Construct() {
                assert(NULL != cgroup_);
                assert(!container_id_.empty());
                
                boost::system::error_code ec;
                boost::filesystem::path path(this->Path());
                
                if (!boost::filesystem::exists(path, ec) && !boost::filesystem::create_directories(path, ec)) {
                    return -1;
                }
                
                // tcp_throt.recv_bps_quota & tcp_throt.recv_bps_limit
                boost::filesystem::path recv_bps_quota = path;
                recv_bps_quota.append("tcp_throt.recv_bps_quota");
                if (0 != baidu::galaxy::cgroup::Attach(recv_bps_quota.string(), cgroup_->tcp_throt().recv_bps_quota())) {
                    return -1;
                }
                
                int64_t limit = 0L;
                if (cgroup_->tcp_throt().recv_bps_excess()) {
                    limit = -1;
                }
                
                boost::filesystem::path recv_bps_limit = path;
                recv_bps_limit.append("tcp_throt.recv_bps_limit");
                if (0 != baidu::galaxy::cgroup::Attach(recv_bps_quota.string(), limit)) {
                    return -1;
                }
                
                // tcp_throt.send_bps_quota & tcp_throt.send_bps_limit
                boost::filesystem::path send_bps_quota = path;
                send_bps_quota.append("tcp_throt.send_bps_quota");
                if (0 != baidu::galaxy::cgroup::Attach(recv_bps_quota.string(), cgroup_->tcp_throt().send_bps_quota())) {
                    return -1L;
                }
                
                limit = 0L;
                if (cgroup_->tcp_throt().send_bps_excess()) {
                    limit = -1L;
                }
                
                boost::filesystem::path send_bps_limit = path;
                send_bps_limit.append("tcp_throt.send_bps_limit");
                if (0 != baidu::galaxy::cgroup::Attach(send_bps_quota.string(), limit)) {
                    return -1;
                }
                
                
                return 0;
            }

            boost::shared_ptr<google::protobuf::Message> TcpThrotSubsystem::Status() {
                boost::shared_ptr<google::protobuf::Message> ret;
                return ret;
            }

            boost::shared_ptr<Subsystem> TcpThrotSubsystem::Clone() {
                boost::shared_ptr<Subsystem> ret(new TcpThrotSubsystem());
                return ret;
            }

        }
    }
}





















/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
