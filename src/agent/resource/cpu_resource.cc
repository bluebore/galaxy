// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpu_resource.h"

#include <assert.h>

namespace baidu {
    namespace galaxy {
        namespace resource {
            CpuResource::CpuResource() :
                total_(0),
                assigned_(0) {
                
            }
            
            CpuResource::~CpuResource() {
                
            }

           
        }
    }
}
