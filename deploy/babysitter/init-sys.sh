#!/usr/bin/env sh

#check cgroup
CGROUP_ROOT=/cgroups
df -h | grep $CGROUP_ROOT >/dev/null 2>&1
ret=$?
if [ $ret -eq 0 ]; then
    echo "cgroups exists ,skip"
else
    echo "init cgroups"
    mkdir -p $CGROUP_ROOT && mount -t tmpfs cgroup $CGROUP_ROOT
    mkdir -p $CGROUP_ROOT/cpu && mount -t cgroup -ocpu none $CGROUP_ROOT/cpu
    mkdir -p $CGROUP_ROOT/memory && mount -t cgroup -omemory none $CGROUP_ROOT/memory
    mkdir -p $CGROUP_ROOT/cpuacct && mount -t cgroup -ocpuacct none $CGROUP_ROOT/cpuacct
fi

mkdir -p /home/galaxy/agent/log
mkdir -p /home/galaxy/master/log

cd /home/galaxy/agent
wget -O tmp.tar.gz ftp://cq01-ps-dev197.cq01.baidu.com/tmp/galaxy.tar.gz
