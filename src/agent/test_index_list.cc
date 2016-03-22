// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent/index_list.h"

int main (int argc, char* argv[]) {
    baidu::galaxy::IndexList<int> int_index_list;
    for (int i = 0; i < 10; i++) {
        int_index_list.PushBack(i); 
    }
    for (int i = 0; i < 10; i++) {
        int val = int_index_list.Front();
        int_index_list.PopFront();
        assert(val == i);
    }

    for (int i = 0; i < 10; i++) {
        int_index_list.PushFront(i); 
    }
    assert(int_index_list.Size() == 10);

    for (int i = 0; i < 10; i++) {
        if (i % 2 == 0) {
            int_index_list.Erase(i); 
        } 
    }
    while (int_index_list.Size() > 0) {
        int val = int_index_list.Front(); 
        int_index_list.PopFront();
        fprintf(stdout, "val %d\n", val);
    }
    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
