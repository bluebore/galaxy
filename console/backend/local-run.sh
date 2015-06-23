#! /bin/sh
export PYTHONPATH=`pwd`/src
nohup python src/manage.py runserver 0.0.0.0:8989  >log 2>&1 &

