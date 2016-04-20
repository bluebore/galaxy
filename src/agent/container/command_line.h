// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

namespace baidu {
namespace galaxy {
namespace container {
            
class CommandLine {
public:
    CommandLine* SetId(const std::string& id);
    CommandLine* SetUser(const std::string& user);
    CommandLine* Append(const std::string& item);
    std::string Str();
private:
    std::string _cmd;
};

} //namespace container
} //namespace galaxy
} //namespace baidu
