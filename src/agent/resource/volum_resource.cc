// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "volum_resource.h"
#include "protocol/galaxy.pb.h"

namespace baidu {
    namespace galaxy {
        namespace resource {
            VolumResource::VolumResource() {
                
            }
            
            VolumResource::~VolumResource() {
                
            }
            
            int VolumResource::Load() {
                return 0;
            }

            int VolumResource::Allocat(const baidu::galaxy::proto::VolumRequired& require) {
                if (require.medium() == baidu::galaxy::proto::KTmpfs) {
                    return -1;
                }
                
                std::map<std::string, VolumResource::Volum>::iterator iter = resource_.find(require.source_path());
                if (iter == resource_.end()) {
                    return -1;
                }
                
                if (require.medium() != iter->second.medium_) {
                    return -1;
                }
                
                if (iter->second.assigned_ + require.size() > iter->second.total_) {
                    return -1;
                }
                
                iter->second.assigned_ += require.size();
                return 0;
            }
            
            int VolumResource::Release(const baidu::galaxy::proto::VolumRequired& require) {
                if (require.medium() == baidu::galaxy::proto::KTmpfs) {
                    return -1;
                }
                
                std::map<std::string, VolumResource::Volum>::iterator iter = resource_.find(require.source_path());
                if (iter == resource_.end()) {
                    return -1;
                }
                
                if (iter->second.assigned_ + require.size() > iter->second.total_) {
                    return -1;
                }
                
                iter->second.assigned_ -= require.size();
                return 0;
            }
            
            void VolumResource::Resource(std::map<std::string, VolumResource::Volum>& r) {
                std::map<std::string, VolumResource::Volum>::iterator iter = resource_.begin();
                while (iter != resource_.end()) {
                    r[iter->first] = iter->second;
                }
            }
        }
    }
}
