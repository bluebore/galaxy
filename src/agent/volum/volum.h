// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "mounter.h"

#include <boost/shared_ptr.hpp>
#include <string>

namespace baidu {
    namespace galaxy {
        namespace proto {
            class VolumRequired;
        }
        
        namespace volum {
            
            class Volum {
            public:
                Volum();
                ~Volum();
                void SetContainerId(const std::string& container_id);
                void SetDescrition(const boost::shared_ptr<baidu::galaxy::proto::VolumRequired> vr);
                const boost::shared_ptr<baidu::galaxy::proto::VolumRequired> Descrition();
                int Construct();
                int Mount();
                int Destroy();
                int64_t Used();
                
                std::string ToString();
                
            private:
                std::string SourcePath();
                std::string TargePath();
                int ConstructTmpfs();
                int ConstructDir();

                boost::shared_ptr<baidu::galaxy::proto::VolumRequired> vr_;
                std::string container_id_;
                Mounter mounter_;
                
            };
        }
    }
}
