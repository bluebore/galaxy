#!/usr/bin/env sh

TMP_PACK_DIR=/tmp/galaxy
if [ -d "$TEM_PACK_DIR" ];then
    rm -rf $TMP_PACK_DIR
fi
mkdir -p $TMP_PACK_DIR

cp -rf ../output/* $TMP_PACK_DIR/
cp *.sh $TMP_PACK_DIR/bin
cp babysitter/* $TMP_PACK_DIR/bin
CUR_DIR=`pwd`

cd /tmp && tar -zcvf $CUR_DIR/galaxy.tar.gz galaxy


