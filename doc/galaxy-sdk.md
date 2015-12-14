## galaxy sdk 使用文档

### sdk初始化
galaxy sdk 初始化需要两个参数
* nexus 服务地址，比如“xxx:8787,aaaa:8787”
* master_key galaxy master在nexus存储key ,比如"/baidu/galaxy/master"

```
::baidu::galaxy::Galaxy* galaxy = ::baidu::galaxy::Galaxy::ConnectGalaxy("xxxx:8787,aaaa:8787","/baidu/galaxy/master");
```

### 通过job name获取pod列表

```
std::vector<baidu::galaxy::PodInformation> pods;
bool ok = galaxy->GetPodsByName("jobname", &pods);
```


