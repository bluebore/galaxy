## Galaxy 集成测试框架

### 环境依赖
* python2.7
* pip
* sofapb-rpc python
* nose
以下是具体安装步骤
```
sh build-env-internal.sh
```

### 本地运行集成测试
sh inte-test-on-local.sh

### 集群测试部署
sh inte-test-on-cluster.sh

### python 怎么和master通讯
使用sofapb-rpc python实现和master 通讯，目前在[python-sdk](../../console/backend/src/galaxy/sdk.py),实现了一些sdk功能，如果要测试
agent,可以参照sdk生成python rpc client代码

