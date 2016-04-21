// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

namespace baidu {
namespace galaxy {
namespace agent {

class ContainerManager {
public:
    boost::shared_ptr<baidu::galaxy::container::Container> CreateContainer() {
        // check resource
        // alloc resource
        // create crgoup
        // command
        // process
    }

    int ReleaseContainer();

    void ListContainer();

};

} //namespace agent
} //namespace galaxy
} //namespace baidu
