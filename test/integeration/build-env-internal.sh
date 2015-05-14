#!/usr/bin/env sh
check_install(){
    cmd=$1
    install_cmd=$2
    which $1 >/dev/null 2>&1
    ret=$?
    if [[ $ret != 0 ]];then
        echo "install $cmd"
        sh -c "$install_cmd"
    fi
}

check_install jumbo  'bash -c "$( curl http://jumbo.baidu.com/install_jumbo.sh )"; source ~/.bashrc'

check_install python 'jumbo install python'

check_install protoc 'jumbo install protobuf'

check_install git 'jumbo install git'


jumbo install python-pip

# config pip
mkdir -p $HOME/.pip/
echo -e "[global]\nindex-url = http://pip.baidu.com/pypi/simple" >> $HOME/.pip/pip.#conf
echo -e "[easy_install]\nindex-url = http://pip.baidu.com/pypi/simple" >> $HOME/.pydistutils.cfg

pip install nose protobuf

git clone http://gitlab.baidu.com/wangtaize/galaxy_deps.git
cd galaxy_deps/python && python setup.py install

