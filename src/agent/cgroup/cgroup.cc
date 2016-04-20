// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cgroup.h"
#include "subsystem_factory.h"
#include "subsystem.h"
#include "freezer_subsystem.h"

namespace baidu {
    namespace galaxy {
        namespace cgroup {
            
            Cgroup::Cgroup(const boost::shared_ptr<SubsystemFactory> factory) :
                factory_(factory){
            }
            
            Cgroup::~Cgroup() {
                
            }
            
            void Cgroup::SetContainerId(const std::string& container_id) {
                container_id_ = container_id;
            }
            
            void Cgroup::SetDescrition(boost::shared_ptr<baidu::galaxy::proto::Cgroup> cgroup) {
                cgroup_ = cgroup;
            }

            int Cgroup::Construct() {
                std::vector<std::string> subsystems;
                factory_->GetSubsystems(subsystems);
           
                for (size_t i = 0; i < subsystems.size(); i++) {
                    
                    boost::shared_ptr<Subsystem> ss = factory_->CreateSubsystem(subsystems[i]);
                    if (ss.get() != NULL) {
                        if (subsystems[i] == "freezer") {
                            freezer_ = boost::dynamic_pointer_cast<FreezerSubsystem>(ss);
                            assert(NULL != freezer_.get());
                        } else {
                            subsystem_.push_back(ss);
                        }
                    } else {
                        break;
                    }
                }
                
                if (subsystems.size() != subsystem_.size() - 1 || NULL == freezer_.get()) {
                    // destroy
                }
                return 0;
            }
            
            // Fixme: freeze first, and than kill
            int Cgroup::Destroy() {
                int ret = 0;
                for (size_t i = 0; i < subsystem_.size(); i++) {
                    if (0 != subsystem_[i]->Destroy()) {
                        ret = -1;
                    }
                }
                return ret;
            }
            
            boost::shared_ptr<google::protobuf::Message> Report() {
                boost::shared_ptr<google::protobuf::Message> ret;
                return ret;
                
            }
            
            int ExportEnv(std::map<std::string, std::string>& evn) {
                return 0;
            }
            
        }
    }
}


