#!/usr/bin/env sh
CONSOLE_HOME=`pwd`
cd backend && sh compile-proto.sh && sh local-run.sh
cd $CONSOLE_HOME
cd ui && nohup grunt serve >log 2>&1 &
cd $CONSOLE_HOME
go build proxy.go
echo "start proxy"
./proxy --host=http://localhost:8135
