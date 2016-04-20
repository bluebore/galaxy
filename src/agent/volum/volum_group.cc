// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "volum_group.h"
#include "volum.h"

#include "protocol/galaxy.pb.h"
#include "agent/volum/volum.h"

namespace baidu {
    namespace galaxy {
        namespace volum {
            VolumGroup::VolumGroup() {
                
            }
            
            VolumGroup::~VolumGroup() {
                
            }

            void VolumGroup::AddDataVolum(const baidu::galaxy::proto::VolumRequired& data_volum) {
                boost::shared_ptr<baidu::galaxy::proto::VolumRequired> volum(new baidu::galaxy::proto::VolumRequired());
                volum->CopyFrom(data_volum);
                dv_description_.push_back(volum);
            }
            
            void VolumGroup::SetWorkspaceVolum(const baidu::galaxy::proto::VolumRequired& ws_volum) {
                ws_description_.reset(new baidu::galaxy::proto::VolumRequired);
                ws_description_->CopyFrom(ws_volum);
            }
            
            void VolumGroup::SetContainerId(const std::string& container_id) {
                container_id_ = container_id;
            }

            int VolumGroup::Construct() {
                workspace_volum_ = Construct(this->ws_description_);
                if (NULL == workspace_volum_.get()) {
                    return -1;
                }
                
                for (size_t i = 0; i < dv_description_.size(); i++) {
                    boost::shared_ptr<Volum> v = Construct(dv_description_[i]);
                    if (v.get() == NULL) {
                        break;
                    } 
                    data_volum_.push_back(v);
                }
                
                if (data_volum_.size() != dv_description_.size()) {
                    for (size_t i = 0; i < data_volum_.size(); i++) {
                        data_volum_[i]->Destroy();
                    }
                    return -1;
                }
                return 0;
            }
            
            int VolumGroup::Mount() {
                if (0 != workspace_volum_->Mount()) {
                    return -1;
                }
                
                for (size_t i = 0; i < data_volum_.size(); i++) {
                    if (0 != data_volum_[i]->Mount()) {
                        return -1;
                    }
                }
                return 0;
            }
            
            int VolumGroup::Destroy() {
                int ret = 0;
                for (size_t i = 0; i < data_volum_.size(); i++) {
                    if (0 != data_volum_[i]->Destroy()) {
                        ret = -1;
                    }
                }
                return ret;
            }
            
            int VolumGroup::ExportEnv(std::map<std::string, std::string>& env) {
                
                return 0;
            }
            
            boost::shared_ptr<google::protobuf::Message> VolumGroup::Report() {
                boost::shared_ptr<google::protobuf::Message> ret;
                return ret;
            }
            
            boost::shared_ptr<Volum> VolumGroup::Construct(boost::shared_ptr<baidu::galaxy::proto::VolumRequired> dp) {
                assert(NULL != dp.get());
                boost::shared_ptr<Volum> volum(new Volum());
                volum->SetDescrition(dp);
                volum->SetContainerId(this->container_id_);
                
                if (0 != volum->Construct()) {
                    volum.reset();
                }
                return volum;
            }
            
        }
    }
}
