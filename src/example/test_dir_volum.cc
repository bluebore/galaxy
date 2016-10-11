// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "volum/volum.h"
#include "protocol/galaxy.pb.h"
#include "util/path_tree.h"

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " source target" << std::endl;
        return -1;
    }

    baidu::galaxy::path::SetRootPath("./");

    boost::shared_ptr<baidu::galaxy::proto::VolumRequired> vr(new baidu::galaxy::proto::VolumRequired());
    vr->set_dest_path(argv[2]);
    vr->set_source_path(argv[1]);
    vr->set_size(1024);
    vr->set_readonly(false);
    vr->set_medium(baidu::galaxy::proto::KDisk);
    vr->set_type(baidu::galaxy::proto::KEmptyDir);


    baidu::galaxy::volum::Volum volum;
    volum.SetContainerId("container_id");
    volum.SetDescrition(vr);
    

    if (0 != volum.Construct()) {
        std::cerr << "construct volum failed" << std::endl;
        return -1;
    } 

    std::cerr << "construct volum sucessfully" << std::endl;

    if (0 != volum.Mount()) {
        std::cerr << "mount volum failed" << std::endl;
        return -1;
    } 
    std::cerr << "mout volum sucessfully" << std::endl;

    //if (0 != volum.Destroy()) {
    //    std::cerr << "destroy volum failed" << std::endl;
    //}
    return 0;

}
