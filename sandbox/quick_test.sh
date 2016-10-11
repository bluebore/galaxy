#! /bin/bash

hn=`hostname`

## prepare cgroups
#CGROUP_ROOT=/cgroups
#mkdir -p $CGROUP_ROOT/cpu && mount -t cgroup -ocpu none $CGROUP_ROOT/cpu >/dev/null 2>&1
#mkdir -p $CGROUP_ROOT/memory && mount -t cgroup -omemory none $CGROUP_ROOT/memory >/dev/null 2>&1
#mkdir -p $CGROUP_ROOT/cpuacct && mount -t cgroup -ocpuacct none $CGROUP_ROOT/cpuacct >/dev/null 2>&1
#mkdir -p $CGROUP_ROOT/freezer && mount -t cgroup -ofreezer none $CGROUP_ROOT/freezer >/dev/null 2>&1

# start nexus
rm -rf galaxy.flag
touch galaxy.flag
echo "--nexus_servers=$hn:8868,$hn:8869,$hn:8870,$hn:8871,$hn:8872" >> galaxy.flag
echo "--appworker_endpoint=127.0.0.1:8221" >> galaxy.flag
echo "--appworker_container_id=container-1" >> galaxy.flag
echo "--logbufsecs=0" >> galaxy.flag

## stop galaxy
#./stop_all.sh
#
## start galaxy
#./start_all.sh
#
### submit job
##../galaxy submit -f sample.json
##../galaxy jobs
##../galaxy agents

