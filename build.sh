sudo apt-get install libleveldb-dev
sudo apt-get install libboost-dev libsnappy-dev
wget https://github.com/google/protobuf/releases/download/v2.6.0/protobuf-2.6.0.tar.gz
tar xf protobuf-2.6.0.tar.gz
cd protobuf-2.6.0 && ./configure && make -j4 && sudo make install && sudo ldconfig
cd -

git clone https://github.com/google/snappy
cd snappy && sh ./autogen.sh && ./configure && make -j2 && sudo make install
cd -

sudo apt-get install zlib1g-dev
git clone https://github.com/bluebore/sofa-pbrpc ./thirdparty/sofa-pbrpc
cd thirdparty/sofa-pbrpc && make -j4 && make install && cd python && sudo python setup.py install
cd -

wget https://github.com/gflags/gflags/archive/v2.1.2.tar.gz
tar xf v2.1.2.tar.gz
cd gflags-2.1.2 && cmake -DGFLAGS_NAMESPACE=google && make -j4 && sudo make install
cd -

git clone https://github.com/baidu/common
cd common && make -j4
cd -

git clone https://github.com/fxsjy/ins
cd ins && PBRPC_PATH=../thirdparty/sofa-pbrpc/output/ make sdk

make -j4 && make install
