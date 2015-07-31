#! /bin/bash
set -e

./clear.sh

echo "--master_port=7810" > galaxy.flag

./start_all.sh
sleep 3

nc -v localhost 7810 -w 2
