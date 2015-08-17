#! /bin/bash

./stop_all.sh

rm -rf *.log
rm -rf *.flag

rm -rf ../ins/sandbox/data/*
rm -rf ../ins/sandbox/binlog/*

rm -rf ./work_dir/
