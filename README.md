# Galaxy
[![Build Status](https://travis-ci.org/baidu/galaxy.svg?branch=master)](https://travis-ci.org/baidu/galaxy)  
Copyright 2015, Baidu, Inc.  
（当前分支refactor是重构版本，查找之前的代码、文档请切换到master分支）

## 综述
Galaxy是一个数据中心操作系统，目标是最大化资源的利用率与降低应用部署运维代价。

## 架构
![架构图](https://github.com/baidu/galaxy/blob/master/images/galaxy_arch.png?raw=true)  

## 构建
外部或者内部同学请都用build.sh,如果在ubuntu上面编译
请先安装
```
sudo apt-get install libreadline-dev
```
sh build.sh

## 使用client
参照[galaxy-cli.md](doc/galaxy-cli.md)
