// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <google/protobuf/message.h>
#include <boost/shared_ptr.hpp>

#include <map>
#include <string>
#include <vector>

namespace baidu {
    namespace galaxy {
        namespace proto {
            class VolumRequired;
        }
        
        namespace volum {
            class Volum;
            
            class VolumGroup {
            public:
                VolumGroup();
                ~VolumGroup();
                
                void AddDataVolum(const baidu::galaxy::proto::VolumRequired& data_volum);
                void SetWorkspaceVolum(const baidu::galaxy::proto::VolumRequired& ws_volum);
                 void SetContainerId(const std::string& container_id);
                 
                int Construct();
                int Mount();
                int Destroy();
                int ExportEnv(std::map<std::string, std::string>& env);
                boost::shared_ptr<google::protobuf::Message> Report();
                
            private:
                boost::shared_ptr<Volum> Construct(boost::shared_ptr<baidu::galaxy::proto::VolumRequired> volum);
                
                boost::shared_ptr<baidu::galaxy::proto::VolumRequired> ws_description_;
                std::vector<boost::shared_ptr<baidu::galaxy::proto::VolumRequired> > dv_description_;
                
                std::vector<boost::shared_ptr<Volum> > data_volum_;
                boost::shared_ptr<Volum> workspace_volum_;
                
                std::string container_id_;
                
            };
        }
    }
}
