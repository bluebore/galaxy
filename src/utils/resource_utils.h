// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_RESOURCE_UTILS_H
#define BAIDU_GALAXY_RESOURCE_UTILS_H

#include <algorithm>
#include "proto/galaxy.pb.h"


namespace baidu {
namespace galaxy {

typedef google::protobuf::RepeatedPtrField<baidu::galaxy::Volume> VolumeList;

class ResourceUtils {

public:

    // check if left is greater than or equal with right 
    // if that return true or return false
    // the check options include cpu , mem , disks, ssds
    // TODO add port check
    static bool GtOrEq(const Resource& left, 
                       const Resource& right);


    // alloc require resource from target 
    static bool Alloc(const Resource& require, 
                      Resource& target);
    static bool AllocPort(const Resource& require,
                     Resource& used);
    static void DeallocPort(const Resource& to_be_free,
                            Resource* used);
    static bool AllocResource(const Resource& require,
                              const std::vector<Resource>& alloc,
                              AgentInfo* agent);
    static void DeallocResource(const Resource& to_be_free,
                               AgentInfo* agent);

    // compare two resource object
    // the check options include cpu , mem ,disks , ssds
    // equal return 0
    // greater than return 1
    // less than return -1
    // TODO add port check
    static int32_t Compare(const Resource& left,
                       const Resource& right);
    static bool HasDiff(const Resource& left,
                        const Resource& right);
    // compare two vector
    template<class T, class Compare>
    static int32_t CompareVector(std::vector<T>& l, 
                                 std::vector<T>& r,
                                 Compare comp) {
        
        if (r.size() > l.size()) {
            return -1;
        } 
        
        std::sort(l.begin(), l.end(), comp);
        std::sort(r.begin(), r.end(), comp);
        
        int32_t check_ret = 0;
        for (size_t i = 0; i < r.size(); i++) {
            bool ret = comp(l[i], r[i]);
            if (ret) {
                // l > r
                check_ret = 1;
            }else if (!comp(l[i], r[i])) {
                // l == r or l > r
                continue;
            }else{
                // l < r
                return -1;
            }
        }
        if (check_ret == 0 && l.size() > r.size()){
            return 1;
        }
        return check_ret;
    }
private:
    static void VolumeAlloc(const VolumeList& total,
                            const VolumeList& require,
                            VolumeList* left);

};

} // galaxy
}// baidu
#endif
