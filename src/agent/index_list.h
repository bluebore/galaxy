// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _SRC_AGENT_INDEX_LIST_H
#define _SRC_AGENT_INDEX_LIST_H

#include <map>
#include <list>

namespace baidu {
namespace galaxy {

// NOTE no threadsafe
template<typename T>
class IndexList {
public:
    T& Back() {
        return list_.back();         
    }

    void PopBack() {
        const T& val = list_.back();
        map_.erase(val);   
        list_.pop_back();
        return;
    }

    T& Front() {
        return list_.front(); 
    }

    void PopFront() {
        const T& val = list_.front(); 
        map_.erase(val);
        list_.pop_front();
        return;
    }

    void PushBack(const T& val) {
        list_.push_back(val); 
        typename std::list<T>::iterator it = list_.end();
        it --;
        map_[val] = it;
        return;
    }

    void PushFront(const T& val) {
        list_.push_front(val); 
        map_[val] = list_.begin();
        return;
    }

    size_t Size() {
        return list_.size(); 
    }

    void Erase(const T& val) {
        typename std::list<T>::iterator it = map_[val]; 
        list_.erase(it);
        map_.erase(val);
        return;
    }
private:
    std::list<T> list_;
    std::map<T, typename std::list<T>::iterator > map_;
};

}   // ending namespace galaxy
}   // ending namespace baidu

#endif  //_SRC_AGENT_INDEX_LIST_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
