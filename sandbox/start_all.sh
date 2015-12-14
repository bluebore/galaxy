echo "start nexus"
cd ../thirdsrc/ins/sandbox && nohup ./start_all.sh > ins_start.log 2>&1 &
sleep 2
galaxyflag=`pwd`/galaxy.flag
echo "start master"
nohup  ../master --flagfile=$galaxyflag >master.log 2>&1 &

sleep 1
echo "start agent"
nohup  ../agent --flagfile=$galaxyflag >agent.log 2>&1 &

sleep 1
echo "start scheduler"
nohup ../scheduler --flagfile=$galaxyflag >scheduler.log 2>&1 &
