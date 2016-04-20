// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <string>

namespace baidu {
    namespace galaxy {
        namespace volum {
            
            class Mounter {
            public:
                Mounter();
                ~Mounter();
                
                Mounter* SetSource(const std::string& source);
                Mounter* SetTarget(const std::string& target);
                Mounter* SetBind(bool b);
                Mounter* SetRdonly(bool ro);
                Mounter* SetType(const std::string& type);
                Mounter* SetOption(const std::string& option);
                
                std::string Source() const;
                std::string Target() const;
                bool Bind() const;
                bool ReadOnly() const;
                const std::string Type() const;
                const std::string Option() const;
                
                const std::string ToString() const;
                
                static int Mount(const Mounter* mounter);
                static int Unmount(const Mounter* mounter);
                
            private:
                static int MountTmpfs(const Mounter* mounter);
                static int MountDir(const Mounter* mounter);
                static int MountProc(const Mounter* mounter);
                std::string source_;
                std::string target_;
                bool bind_;
                bool ro_;
                std::string option_;
                std::string type_;
            };
        }
    }
}
