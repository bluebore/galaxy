// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mounter.h"
#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"

#include <sys/mount.h>
#include <assert.h>
#include <sstream>

namespace baidu {
    namespace galaxy {
        namespace volum {

            Mounter::Mounter() :
            bind_(false) {
            }

            Mounter::~Mounter() {
            }

            Mounter* Mounter::SetSource(const std::string& source) {
                source_ = source;
                return this;
            }

            Mounter* Mounter::SetTarget(const std::string& target) {
                target_ = target;
                return this;
            }

            Mounter* Mounter::SetBind(bool b) {
                bind_ = b;
                return this;
            }

            Mounter* Mounter::SetRdonly(bool ro) {
                ro_ = ro;
                return this;
            }

            Mounter* Mounter::SetType(FsType type) {
                type_ = type;
                return this;
            }

            Mounter* Mounter::SetOption(const std::string& option) {
                option_ = option;
                return this;
            }

            const std::string Mounter::ToString() const {
                std::stringstream ss;
                ss << "mouter option:\t"
                        << "source:" << source_ << "\t"
                        << "target:" << target_ << "\t"
                        << "option:" << option_ << "\t"
                        << "type:" << type_;
                return ss.str();
            }

            std::string Mounter::Source() const {
                return source_;
            }

            std::string Mounter::Target() const {
                return target_;
            }

            bool Mounter::Bind() const {
                return bind_;
            }

            bool Mounter::ReadOnly() const {
                return ro_;
            }

            const Mounter::FsType Mounter::Type() const {
                return type_;
            }

            const std::string Mounter::Option() const {
                return option_;
            }

            int Mounter::Mount(const Mounter* mounter) {
                assert(NULL != mounter);

                if (mounter->Type() == Mounter::FT_TMPFS) {
                    return MountTmpfs(mounter);
                } else if (mounter->Type() == Mounter::FT_DIR) {
                    return MountDir(mounter);
                } else if (mounter->Type() == Mounter::FT_PROC) {
                    return MountProc(mounter);
                } else if (mounter->Type() == Mounter::FT_SYMLINK) {
                    return MountSymlink(mounter);
                }
                return -1;
            }

            int Mounter::MountTmpfs(const Mounter* mounter) {
                int flag = 0;

                if (mounter->ReadOnly()) {
                    flag |= MS_RDONLY;
                }

                return ::mount("tmpfs", mounter->Target().c_str(), "tmpfs", flag, (void*) mounter->Option().c_str());
            }

            int Mounter::MountDir(const Mounter* mounter) {
                int flag = 0;
                flag |= MS_BIND;

                if (mounter->ReadOnly()) {
                    flag |= MS_RDONLY;
                }

                return ::mount(mounter->Source().c_str(), mounter->Target().c_str(), "", flag, "");
            }

            int Mounter::MountProc(const Mounter* mounter) {
                return ::mount("proc", mounter->Target().c_str(), "proc", 0, "");
            }

            int Mounter::MountSymlink(const Mounter* mounter) {
                boost::filesystem::path symlink(mounter->Target());
                boost::filesystem::path source(mounter->Source());
                boost::system::error_code ec;
                if (!boost::filesystem::exists(symlink, ec)) {
                    if (!boost::filesystem::exists(symlink.parent_path()) && !boost::filesystem::create_directories(symlink.parent_path())) {
                        return -1;
                    }
                    boost::filesystem::create_symlink(source, symlink, ec);
                    if (ec.value() != 0) {
                        return -1;
                    }
                } else {
                    if (boost::filesystem::is_symlink(symlink, ec)) {
                        if (!boost::filesystem::remove(symlink, ec)) {
                            return -1;
                        }
                        boost::filesystem::create_symlink(source, symlink, ec);
                        if (ec.value() != 0) {
                            return -1;
                        }
                    } else {
                        return -1;
                    }
                }
                return 0;
            }

        }
    }
}

