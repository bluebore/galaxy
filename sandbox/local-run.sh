#!/usr/bin/env sh
echo "start master "
nohup ../output/bin/master --port=8102 >/tmp/master.log 2>&1 &
sleep 1
echo "start agent"
nohup ../output/bin/agent --port=8202 --master=localhost:8102 >/tmp/agent.log 2>&1 &



