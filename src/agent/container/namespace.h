// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#ifndef set_ns
#define set_ns(fd) syscall(308, 0, fd)
#endif

namespace baidu {
namespace galaxy {
namespace ns {

int Attach(pid_t pid);

} //namespace ns
} //namespace galaxy
} //namespace baidu
