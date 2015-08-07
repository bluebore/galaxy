echo "start nexus"
cd ../ins/sandbox && nohup ./start_all.sh > ins_start.log 2>&1 &
sleep 2
echo "start master"
nohup  ../master --flagfile=galaxy.flag >master.log 2>&1 &
sleep 2
echo "start gced"
nohup ../gced --gce_initd_bin=../../initd --flagfile=galaxy.flag > gced.log 2>&1 &

sleep 1
echo "start agent"
nohup  ../agent --agent_initd_bin=../../initd --flagfile=galaxy.flag >agent.log 2>&1 &

sleep 1
echo "start scheduler"
nohup ../scheduler --flagfile=galaxy.flag >scheduler.log 2>&1 &
