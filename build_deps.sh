#!/bin/bash
set -e -u -E

########################################
# download & build depend software
########################################

WORK_DIR=$(cd $(dirname $0); pwd)
DEPS_SOURCE=$WORK_DIR/thirdsrc
DEPS_PREFIX=$WORK_DIR/thirdparty
DEPS_CONFIG="--prefix=${DEPS_PREFIX} --disable-shared --with-pic"
FLAG_DIR=$WORK_DIR/.build

export PATH=${DEPS_PREFIX}/bin:$PATH
mkdir -p ${DEPS_SOURCE} ${DEPS_PREFIX} ${FLAG_DIR}

cd ${DEPS_SOURCE}

# boost
if [ ! -f "${FLAG_DIR}/boost_1_57_0" ] \
    || [ ! -d "${DEPS_PREFIX}/boost_1_57_0/boost" ]; then
    wget -O boost_1_57_0.tar.gz --no-check-certificate http://jaist.dl.sourceforge.net/project/boost/boost/1.57.0/boost_1_57_0.tar.gz
    tar zxf boost_1_57_0.tar.gz
    rm -rf ${DEPS_PREFIX}/boost_1_57_0
    mv boost_1_57_0 ${DEPS_PREFIX}
    cd ${DEPS_PREFIX}/boost_1_57_0 && ./bootstrap.sh && ./b2 --with-filesystem link=static
    cd -
    touch "${FLAG_DIR}/boost_1_57_0"
fi

#rapidjson
if [ ! -f "${FLAG_DIR}/rapidjson" ] \
    || [ ! -d "${DEPS_PREFIX}/rapidjson" ]; then
    git clone https://github.com/miloyip/rapidjson.git >/dev/null
    rm -rf ${DEPS_PREFIX}/rapidjson
    cp -rf rapidjson ${DEPS_PREFIX}
    touch "${FLAG_DIR}/rapidjson"
fi

# protobuf
if [ ! -f "${FLAG_DIR}/protobuf_2_6_1" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libprotobuf.a" ] \
    || [ ! -d "${DEPS_PREFIX}/include/google/protobuf" ]; then
    # wget --no-check-certificate https://github.com/google/protobuf/releases/download/v2.6.1/protobuf-2.6.1.tar.gz
    rm -rf protobuf
    git clone --depth=1 https://github.com/00k/protobuf
    mv protobuf/protobuf-2.6.1.tar.gz .
    tar zxf protobuf-2.6.1.tar.gz
    cd protobuf-2.6.1
    ./configure ${DEPS_CONFIG}
    make -j4
    make install
    cd -
    touch "${FLAG_DIR}/protobuf_2_6_1"
fi

# snappy
if [ ! -f "${FLAG_DIR}/snappy_1_1_1" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libsnappy.a" ] \
    || [ ! -f "${DEPS_PREFIX}/include/snappy.h" ]; then
    # wget --no-check-certificate https://snappy.googlecode.com/files/snappy-1.1.1.tar.gz
    rm -rf snappy
    git clone --depth=1 https://github.com/00k/snappy
    mv snappy/snappy-1.1.1.tar.gz .
    tar zxf snappy-1.1.1.tar.gz
    cd snappy-1.1.1
    ./configure ${DEPS_CONFIG}
    make -j4
    make install
    cd -
    touch "${FLAG_DIR}/snappy_1_1_1"
fi

# sofa-pbrpc
if [ ! -f "${FLAG_DIR}/sofa-pbrpc_1_0_0" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libsofa-pbrpc.a" ] \
    || [ ! -d "${DEPS_PREFIX}/include/sofa/pbrpc" ]; then
    wget --no-check-certificate -O sofa-pbrpc-1.0.0.tar.gz https://github.com/baidu/sofa-pbrpc/archive/v1.0.0.tar.gz
    tar zxf sofa-pbrpc-1.0.0.tar.gz
    cd sofa-pbrpc-1.0.0
    sed -i '/BOOST_HEADER_DIR=/ d' depends.mk
    sed -i '/PROTOBUF_DIR=/ d' depends.mk
    sed -i '/SNAPPY_DIR=/ d' depends.mk
    echo "BOOST_HEADER_DIR=${DEPS_PREFIX}/boost_1_57_0" >> depends.mk
    echo "PROTOBUF_DIR=${DEPS_PREFIX}" >> depends.mk
    echo "SNAPPY_DIR=${DEPS_PREFIX}" >> depends.mk
    echo "PREFIX=${DEPS_PREFIX}" >> depends.mk
    cd src
    PROTOBUF_DIR=${DEPS_PREFIX} sh compile_proto.sh
    cd ..
    make -j4
    make install
    cd ..
    touch "${FLAG_DIR}/sofa-pbrpc_1_0_0"
fi

# cmake for gflags
if ! which cmake ; then
    wget --no-check-certificate -O CMake-3.2.1.tar.gz https://github.com/Kitware/CMake/archive/v3.2.1.tar.gz
    tar zxf CMake-3.2.1.tar.gz
    cd CMake-3.2.1
    ./configure --prefix=${DEPS_PREFIX}
    make -j4
    make install
    cd -
fi

# gflags
if [ ! -f "${FLAG_DIR}/gflags_2_1_1" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libgflags.a" ] \
    || [ ! -d "${DEPS_PREFIX}/include/gflags" ]; then
    wget --no-check-certificate -O gflags-2.1.1.tar.gz https://github.com/schuhschuh/gflags/archive/v2.1.1.tar.gz
    tar zxf gflags-2.1.1.tar.gz
    cd gflags-2.1.1
    cmake -DCMAKE_INSTALL_PREFIX=${DEPS_PREFIX} -DGFLAGS_NAMESPACE=google -DCMAKE_CXX_FLAGS=-fPIC
    make -j4
    make install
    cd -
    touch "${FLAG_DIR}/gflags_2_1_1"
fi

# glog
if [ ! -f "${FLAG_DIR}/glog_0_3_3" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libglog.a" ] \
    || [ ! -d "${DEPS_PREFIX}/include/glog" ]; then
    wget --no-check-certificate -O glog-0.3.3.tar.gz https://github.com/google/glog/archive/v0.3.3.tar.gz
    tar zxf glog-0.3.3.tar.gz
    cd glog-0.3.3
    ./configure ${DEPS_CONFIG} CPPFLAGS=-I${DEPS_PREFIX}/include LDFLAGS=-L${DEPS_PREFIX}/lib
    make -j4
    make install
    cd -
    touch "${FLAG_DIR}/glog_0_3_3"
fi

# gtest
if [ ! -f "${FLAG_DIR}/gtest_1_7_0" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libgtest.a" ] \
    || [ ! -d "${DEPS_PREFIX}/include/gtest" ]; then
    # wget --no-check-certificate https://googletest.googlecode.com/files/gtest-1.7.0.zip
    rm -rf gtest_archive gtest-1.7.0.zip gtest-1.7.0
    git clone --depth=1 https://github.com/xupeilin/gtest_archive
    mv gtest_archive/gtest-1.7.0.zip .
    unzip gtest-1.7.0.zip
    cd gtest-1.7.0
    ./configure ${DEPS_CONFIG}
    make
    cp -af lib/.libs/* ${DEPS_PREFIX}/lib
    cp -af include/gtest ${DEPS_PREFIX}/include
    cd -
    touch "${FLAG_DIR}/gtest_1_7_0"
fi

# libunwind for gperftools
if [ ! -f "${FLAG_DIR}/libunwind_0_99_beta" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libunwind.a" ] \
    || [ ! -f "${DEPS_PREFIX}/include/libunwind.h" ]; then
    wget -O libunwind-0.99-beta.tar.gz http://download.savannah.gnu.org/releases/libunwind/libunwind-0.99-beta.tar.gz
    tar zxf libunwind-0.99-beta.tar.gz
    cd libunwind-0.99-beta
    ./configure ${DEPS_CONFIG}
    make CFLAGS=-fPIC -j4
    make CFLAGS=-fPIC install
    cd -
    touch "${FLAG_DIR}/libunwind_0_99_beta"
fi

# gperftools (tcmalloc)
if [ ! -f "${FLAG_DIR}/gperftools_2_2_1" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libtcmalloc_minimal.a" ]; then
    # wget --no-check-certificate https://googledrive.com/host/0B6NtGsLhIcf7MWxMMF9JdTN3UVk/gperftools-2.2.1.tar.gz
    rm -rf gperftools
    git clone --depth=1 https://github.com/00k/gperftools
    mv gperftools/gperftools-2.2.1.tar.gz .
    tar zxf gperftools-2.2.1.tar.gz
    cd gperftools-2.2.1
    ./configure ${DEPS_CONFIG} CPPFLAGS=-I${DEPS_PREFIX}/include LDFLAGS=-L${DEPS_PREFIX}/lib
    make -j4
    make install
    cd -
    touch "${FLAG_DIR}/gperftools_2_2_1"
fi

# ins
if [ ! -f "${FLAG_DIR}/ins" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libins_sdk.a" ] \
    || [ ! -f "${DEPS_PREFIX}/include/ins_sdk.h" ]; then
    rm -rf ins
    git clone https://github.com/fxsjy/ins
    cd ins
    sed -i "s|^PREFIX=.*|PREFIX=${DEPS_PREFIX}|" Makefile
    sed -i "s|^PROTOC=.*|PROTOC=${DEPS_PREFIX}/bin/protoc|" Makefile
    export BOOST_PATH=${DEPS_PREFIX}/boost_1_57_0
    make -j4 ins >/dev/null && make -j4 install_sdk >/dev/null
    mkdir -p output/bin && cp ins output/bin
    cd -
    touch "${FLAG_DIR}/ins"
fi

# common
if [ ! -f "${FLAG_DIR}/common" ] \
    || [ ! -f "${DEPS_PREFIX}/lib/libcommon.a" ] \
    || [ ! -f "${DEPS_PREFIX}/include/mutex.h" ]; then
    rm -rf common
    git clone https://github.com/baidu/common
    cd common
    sed -i 's/^INCLUDE_PATH=.*/INCLUDE_PATH=-Iinclude -I..\/..\/thirdparty\/boost_1_57_0/' Makefile
    make -j4
    cp -rf include/* ${DEPS_PREFIX}/include/
    cp libcommon.a ${DEPS_PREFIX}/lib
    cd -
    touch "${FLAG_DIR}/common"
fi

if [ -f "${FLAG_DIR}/leveldb" ] \
    && [ -f "${DEPS_PREFIX}/lib/libleveldb.a" ]
then
    echo "leveldb exist"
else
    # leveldb
    rm -rf leveldb
    git clone https://github.com/google/leveldb.git
    cd leveldb
    make -j8 out-static/libleveldb.a OPT=-fno-access-control >/dev/null 
    cp -rf include/* ${DEPS_PREFIX}/include
    cp out-static/libleveldb.a ${DEPS_PREFIX}/lib
    cd -
    touch "${FLAG_DIR}/leveldb"
fi

cd ${WORK_DIR}
echo "build deps done!"

