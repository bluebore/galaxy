// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "volum.h"
#include "mounter.h"
#include "protocol/galaxy.pb.h"

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <sstream>


namespace baidu {
    namespace galaxy {
        namespace volum {

            Volum::Volum() {
            }

            Volum::~Volum() {
            }

            void Volum::SetDescrition(const boost::shared_ptr<baidu::galaxy::proto::VolumRequired> vr) {
                assert(NULL != vr.get());
                vr_ = vr;
            }

            void Volum::SetContainerId(const std::string& container_id) {
                container_id_ = container_id;
            }

            const boost::shared_ptr<baidu::galaxy::proto::VolumRequired> Volum::Descrition() {
                return vr_;
            }

            std::string Volum::ToString() {
                if (vr_.get() != NULL) {
                    return container_id_ + "||" + vr_->DebugString();
                } else {
                    return "empty volum";
                }
            }

            int Volum::Construct() {
                assert(NULL != vr_.get());
                // 创建原路径和目的路径
                boost::system::error_code ec;
                boost::filesystem::path targe_path(this->TargePath());
                int ret = -1;
                do {
                    if (vr_->medium() == baidu::galaxy::proto::KTmpfs) {
                        if (!boost::filesystem::exists(targe_path, ec) && !boost::filesystem::create_directories(targe_path, ec)) {
                            ret = -1;
                            break;
                        }

                        std::stringstream ss;
                        ss << "size=" << vr_->size();
                        mounter_.SetOption(ss.str());
                        mounter_.SetType(Mounter::FT_TMPFS);
                        mounter_.SetRdonly(vr_->readonly());
                        mounter_.SetTarget(targe_path.string());
                        ret = 0;

                    } else if (vr_->has_use_symlink() && vr_->use_symlink()) {
                        boost::filesystem::path source_path(this->SourcePath());
                        if (!boost::filesystem::exists(source_path, ec) && !boost::filesystem::create_directories(source_path, ec)) {
                            ret = -1;
                            break;
                        }
                        mounter_.SetType(Mounter::FT_SYMLINK);
                        ret = 0;

                    } else {
                        boost::filesystem::path source_path(this->SourcePath());

                        if (!boost::filesystem::exists(source_path, ec) && !boost::filesystem::create_directories(source_path, ec)) {
                            ret = -1;
                            break;
                        }

                        mounter_.SetType(Mounter::FT_DIR);
                        mounter_.SetBind(true);
                        mounter_.SetRdonly(vr_->readonly());
                        mounter_.SetSource(source_path.string());
                        mounter_.SetTarget(targe_path.string());
                        ret = 0;
                    }
                } while (0);
                
                return ret;
            }

            int Volum::Mount() {
                if (0 != Mounter::Mount(&mounter_)) {
                    return -1;
                }

                return 0;
            }

            int Volum::Destroy() {
                if (vr_->type() == baidu::galaxy::proto::KEmptyDir && vr_->medium() != baidu::galaxy::proto::KTmpfs) {
                    boost::filesystem::path targe_path(this->TargePath());
                    boost::filesystem::path source_path(this->SourcePath());
                    boost::system::error_code ec;
                    return boost::filesystem::remove_all(source_path, ec);
                }

                return 0;
            }

            int64_t Volum::Used() {
                assert(0);
                return 0;
            }

            std::string Volum::SourcePath() {
                assert(0);
                return "";
            }

            std::string Volum::TargePath() {
                assert(0);
                return "";
            }


        }
    }
}

