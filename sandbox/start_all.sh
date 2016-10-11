#!/bin/bash

echo "1.start nexus"
cd ../thirdsrc/ins/sandbox && nohup ./start_all.sh > ins_start.log 2>&1 &
sleep 2

#galaxyflag=`pwd`/galaxy.flag
##echo "2.start appmaster"
##nohup ../appmaster --flagfile=$galaxyflag >appmaster.log 2>&1 &
#
#echo "3.start appworker"
#nohup ../appworker --flagfile=$galaxyflag >/dev/null 2>&1 &

