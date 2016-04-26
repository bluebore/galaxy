// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "blkio_subsystem.h"
#include "protocol/galaxy.pb.h"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/error_code.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/kdev_t.h>

namespace baidu {
    namespace galaxy {
        namespace cgroup {

            BlkioSubsystem::BlkioSubsystem() {
                
            }
            
            BlkioSubsystem::~BlkioSubsystem() {
                
            }
            
            std::string BlkioSubsystem::Name() {
                return "blkio";
            }
            
            int BlkioSubsystem::Construct() {
                assert(NULL != cgroup_);
                assert(!container_id_.empty());
                
                boost::system::error_code ec;
                boost::filesystem::path path(this->Path());
                
                if (!boost::filesystem::exists(path, ec) && !boost::filesystem::create_directories(path, ec)) {
                    return -1;
                }
                
                // tcp_throt.recv_bps_quota & tcp_throt.recv_bps_limit
                boost::filesystem::path blkio_weight = path;
                blkio_weight.append("blkio.weight");
                if (0 != baidu::galaxy::cgroup::Attach(blkio_weight.string(), (int64_t)cgroup_->blkio().weight())) {
                    return -1;
                }
                return 0;
            }
            
            boost::shared_ptr<Subsystem> BlkioSubsystem::Clone() {
                boost::shared_ptr<Subsystem> ret(new BlkioSubsystem());
                return ret;
            }
            
            boost::shared_ptr<google::protobuf::Message> BlkioSubsystem::Status() {
                boost::shared_ptr<google::protobuf::Message> ret;
                return ret;
            }
            
            int BlkioSubsystem::GetDeviceNum(const std::string& path, int& major, int& minor) {
                struct stat st;
                if (0 != ::stat(path.c_str(), &st)) {
                    return -1;
                }
                
                major = MAJOR(st.st_dev);
                minor = MINOR(st.st_dev);
                return 0;
            }

        }
    }
}
