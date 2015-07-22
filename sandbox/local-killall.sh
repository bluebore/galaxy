#!/usr/bin/env sh

test -e data && rm -rf data
func_kill_by_name(){
   pid_list=`ps -ef | grep $1 | grep -v grep | awk '{print $2}'`
   if [ -n "$pid_list" ];then
    echo "[INFO] close the existing $1"
        for pid in $pid_list;do
            kill -9 $pid
            echo "[INFO] $1 with pid $pid closed"
        done
        echo "[INFO] close $1 successfully"
   else
        echo "[INFO] no $1 is existing"
   fi
}

func_kill_by_name output/bin/master
func_kill_by_name output/bin/agent
func_kill_by_name SimpleHTTPServer
