// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#include <boost/shared_ptr.hpp>
#include <google/protobuf/message.h>

namespace baidu{
namespace galaxy {
namespace resource {

class Resource {
    
public:
    void Allocate(int64_t resource);
    void Release(int64_t resource);
    void* LockResource(int64_t resource);
    void UnlockResource(void* handle);
    void Allocate(void* handle);
    
    int64_t TotalResource();
    RemainResource();
    
};

} //namespace resource
} //namespace galaxy
} //namespace baidu
