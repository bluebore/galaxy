// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <cstdlib>
#include <boost/filesystem.hpp>

#include "boost/system/error_code.hpp"
#include "boost/type_traits/integral_constant.hpp"
#include "boost/filesystem/operations.hpp"
#include <iostream>
using namespace std;

int main(int argc, char** argv) {

    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " exist/create/readlink/rename/remove path\n";
        return -1; 
    }   

    std::string action = argv[1];
    std::string path = argv[2];
    boost::system::error_code ec; 

    if (action == "exist") {
        if (!boost::filesystem::exists(path, ec)) {
            std::cerr << path << " do exist " << ec.value() << " " << ec.message() << std::endl;
        } else {
            std::cerr << path << " exist" << ec.value() << " " << ec.message() << std::endl;
        }   
    } else if (action == "create") {
        if (boost::filesystem::create_directories(path, ec)) {
            std::cerr << "create dir: " << path << " successfully :" << ec.message(); 
        } else {
            std::cerr << "create dir: " << path << " failed :" << ec.message(); 
        }
    } else if (action == "remove") {
        boost::filesystem::remove_all(path, ec);
        if (ec.value() == 0) {
            std::cerr << path << " is removed successfully" << ec.message() << std::endl;
        } else {
            std::cerr << path << " is removed failed" << ec.message() << std::endl;
        } 
    }  else if (action == "readlink") {
        boost::filesystem::path p = boost::filesystem::read_symlink(path, ec);
        if (ec.value() == 0) {
            std::cerr << "read link ok: " << path << "  ->  " << p << std::endl;
        } else {
            std::cerr << "read link failed: " << path << " " << ec.message();
        }
    }


    return 0;
}
