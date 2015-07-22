#! /bin/sh
export PYTHONPATH=`pwd`/src
python src/manage.py runserver 0.0.0.0:8989  >log 2>&1 &

