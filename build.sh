#!/bin/bash
echo "-->(1/2), start to install deps ..."
./build_deps.sh
if [ $? -ne 0 ]; then
    echo "fail to install deps!!!"
    exit 1
fi
echo "-->(2/2) call scons to build project"
scons -j 8

