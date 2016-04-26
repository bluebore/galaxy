// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "protocol/galaxy.pb.h"

#include <map>
#include <string>
#include <vector>

namespace baidu {
    namespace galaxy {
        namespace resource {
            
            class VolumResource {
            public:
                class Volum {
                public:
                    Volum() :
                        total_(0),
                        assigned_(0) {}
                        
                    int64_t total_;
                    int64_t assigned_;
                    baidu::galaxy::proto::VolumMedium medium_;
                };

            public:
                VolumResource();
                ~VolumResource();
                int Load();
                
                int Allocat(const baidu::galaxy::proto::VolumRequired& require);
                int Release(const baidu::galaxy::proto::VolumRequired& require);
                void Resource(std::map<std::string, Volum>& r);

            private:
                std::map<std::string, Volum> resource_;
                
            };
        }
    }
}
