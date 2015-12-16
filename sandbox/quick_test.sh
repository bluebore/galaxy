#! /bin/bash
set -e

./clear.sh >/dev/null 2>&1 
[ -d gc_dir ] || mkdir gc_dir 
hn=`hostname`
#CGROUP_ROOT=/cgroups
#mkdir -p $CGROUP_ROOT/cpu && mount -t cgroup -ocpu none $CGROUP_ROOT/cpu >/dev/null 2>&1
#mkdir -p $CGROUP_ROOT/memory && mount -t cgroup -omemory none $CGROUP_ROOT/memory >/dev/null 2>&1
#mkdir -p $CGROUP_ROOT/cpuacct && mount -t cgroup -ocpuacct none $CGROUP_ROOT/cpuacct >/dev/null 2>&1
#mkdir -p $CGROUP_ROOT/freezer && mount -t cgroup -ofreezer none $CGROUP_ROOT/freezer >/dev/null 2>&1

echo "--master_port=7810" > galaxy.flag
echo "--master_host=127.0.0.1" >> galaxy.flag
echo "--agent_port=7182" >> galaxy.flag
echo "--agent_millicores_share=15000" >> galaxy.flag
echo "--agent_mem_share=68719476736" >> galaxy.flag

echo "--nexus_servers=$hn:8868,$hn:8869,$hn:8870,$hn:8871,$hn:8872" >> galaxy.flag
echo "--agent_initd_bin=`pwd`/../initd" >> galaxy.flag
echo "--agent_work_dir=`pwd`/work_dir" >> galaxy.flag
echo "--gce_bind_config=`pwd`/mount_bind.template" >> galaxy.flag
echo "--gce_support_subsystems=cpu,memory,cpuacct,freezer" >> galaxy.flag

test -e work_dir && rm -rf work_dir
mkdir work_dir
./start_all.sh
sleep 3

tar zcf batch.tar.gz ../galaxy
echo "sleep 10000000000" > longrun.sh
tar zcf longrun.tar.gz longrun.sh
python SimpleHTTPServer 8181 &
../galaxy submit -f sample.json

../galaxy jobs
../galaxy agents
