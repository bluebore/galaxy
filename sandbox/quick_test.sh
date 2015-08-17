#! /bin/bash
set -e

./clear.sh

hn=`hostname`
echo "--master_port=7810" > galaxy.flag
echo "--master_host=127.0.0.1" >> galaxy.flag
echo "--agent_port=7182" >> galaxy.flag
echo "--agent_cpu_share=1000000" >> galaxy.flag
echo "--agent_mem_share=1000000" >> galaxy.flag

echo "--nexus_servers=$hn:8868,$hn:8869,$hn:8870,$hn:8871,$hn:8872" >> galaxy.flag
echo "--agent_initd_bin=`pwd`/../initd" >> galaxy.flag
test -e work_dir && rm -rf work_dir && mkdir work_dir
echo "--agent_work_dir=`pwd`/work_dir" >> galaxy.flag

./start_all.sh
sleep 3

tar zcf batch.tar.gz ../galaxy
echo "yes" > longrun.sh
tar zcf longrun.tar.gz longrun.sh

../galaxy submit job_batch batch.tar.gz 3 3 3 "./galaxy list" batch
../galaxy submit job_longrun longrun.tar.gz 3 3 3 "sh longrun.sh"

#rm -rf longrun*
#rm -rf batch.tar.gz
../galaxy list
../galaxy listagent
