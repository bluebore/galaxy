// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mounter.h"

#include <sys/mount.h>
#include <assert.h>
#include <sstream>

namespace baidu {
    namespace galaxy {
        namespace volum {

            Mounter::Mounter() :
            bind_(false),
            type_(0) {
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


            Mounter* Mounter::SetType(const std::string& type) {
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

            const std::string Mounter::Type() const {
                return type_;
            }

            const std::string Mounter::Option() const {
                return option_;
            }

            int Mounter::Mount(const Mounter* mounter) {
                assert(NULL != mounter);
                
                if (mounter->Type() == "tmpfs") {
                    return MountTmpfs(mounter);
                } else if (mounter->Type() == "dir") {
                    return MountDir(mounter);
                } else if (mounter->Type() == "proc") {
                    return MountProc(mounter);
                }
                return -1;
            }

            int Mounter::MountTmpfs(const Mounter* mounter) {
                int flag = 0;
                if (mounter->ReadOnly()) {
                    flag |= MS_RDONLY;
                }
                return ::mount("tmpfs", mounter->Target().c_str(), "tmpfs", flag, (void*)mounter->Option().c_str());
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

        }
    }
}

