## galaxy client使用手册

### 配置galaxy 
编译galaxy会生成一个galaxy可执行文件,在galaxy 同级目录需要放置一个galaxy.flag配置文件，配置内容
只需配置nexus服务地址
```
--nexus_servers=xxx1:2222,xxx2:2222
```
测试galaxy配置是否正确
```
./galaxy jobs
-  id                                    name      state       stat(r/p/d)  replica  batch  cpu     memory   
-----------------------------------------------------------------------------------------------------------------
```
如果没有出现rpc错误说明配置正确

### 部署一个简单的job
部署job 采用json配置文件

#### 生成一个app
```
echo "sleep 100000000" > app.sh
tar -zcvf app.tar.gz app.sh
```

#### 配置json文件
```
vim app.json

{
    "name":"app0",
    "replica":1,
    "type":"kLongRun",
    "deploy_step":1,
    "pod":{
        "version":"1.0.0",
        "requirement":{
                "millicores":1000,
                "memory":"1G"
        },   
        "tasks":[
                    {   
                     "binary":"app.tar.gz",
                     "source_type":"kSourceTypeBinary",
                     "start_command":"sh app.sh",
                     "requirement":{
                         "millicores":1000,
                         "memory":"1G"
                     }  
                    } 
        ]
    }
}
```
#### 启动job

```
./galaxy submit -f app.json
```

### 查看一个job

job 表格显示job调度状态，以及pod 同级，比如running ,pending ,deploying 数，还有实时cpu,memory使用情况
```
./galaxy jobs
  -  id                                    name      state       stat(r/p/d)  replica  batch  cpu     memory   
-----------------------------------------------------------------------------------------------------------------
  1  33b9c1e8-b074-4838-8a88-71437978d286  flower    kJobNormal  10/0/0       10       -      99575   199.232 G
  2  a804ec42-a518-499b-86fa-fc31683b840e  tag_test  kJobNormal  1/1/0        2        -      1090    156.000 K
  3  ba19ff99-0eca-44ed-a8fe-65fa822794df  abc       kJobNormal  10/0/0       10       -      0       2.617 M  
  4  efb9113c-804d-4681-a0d4-7378712f97a0  burning   kJobNormal  181/0/0      181      -      186083  27.480 M 
```

### 查看job下面pod状态

pod 表格显示pod运行状态（state），以及处于调度状态(stage), 还有cpu ,mem 实时使用情况
```
./galaxy pods -j 33b9c1e8-b074-4838-8a88-71437978d286
  -   id                                    stage          state        cpu(used/assigned)  mem(used/assigned)  endpoint           version
--------------------------------------------------------------------------------------------------------------------------------------------
  1   12209f68-e58f-448c-be9f-4730fc913cd3  kStageRunning  kPodRunning  1020/17000          19.693 G/33.000 G   10.89.24.22:8221   1.0.1  
  2   1253e924-7a74-45d4-bac9-fcb04ed16a64  kStageRunning  kPodRunning  1290/17000          20.024 G/33.000 G   10.89.24.15:8221   1.0.1  
  3   4b24262d-1eea-4d22-9537-a252d9a13c6e  kStageRunning  kPodRunning  15814/17000         19.338 G/33.000 G   10.89.25.22:8221   1.0.1  
  4   a03d49f2-75fe-4c53-ba5f-43a92a36e42b  kStageRunning  kPodRunning  16384/17000         20.183 G/33.000 G   10.89.25.12:8221   1.0.1  
  5   b3f291ca-f53f-42ad-b4aa-f692dd261f75  kStageRunning  kPodRunning  15670/17000         20.268 G/33.000 G   10.89.25.34:8221   1.0.1  
  6   b4d546f2-295e-43ba-96ad-2b381445bd7a  kStageRunning  kPodRunning  1090/17000          20.078 G/33.000 G   10.89.109.20:8221  1.0.1  
  7   c45f7361-133e-4d52-9527-40024fea23a3  kStageRunning  kPodRunning  16189/17000         20.074 G/33.000 G   10.89.109.17:8221  1.0.1  
  8   eb293968-c10c-48be-a8e0-77ff2d4366be  kStageRunning  kPodRunning  1091/17000          19.767 G/33.000 G   10.89.109.11:8221  1.0.1  
  9   ec5978c6-982b-401b-ad58-dfcbfc7a63f9  kStageRunning  kPodRunning  16324/17000         20.039 G/33.000 G   10.89.109.13:8221  1.0.1  
  10  f1912b95-62ac-4e3f-9a3d-1e80059e5de7  kStageRunning  kPodRunning  15680/17000         20.419 G/33.000 G   10.89.25.27:8221   1.0.1 
```

### kill一个job

kill一个job 很简单
```
./galaxy kill -j 33b9c1e8-b074-4838-8a88-71437978d286
```
### 抢占调度手动档
目前galaxy只支持kLongRun对kBatch任务抢占，其他任务需要用户自己输入配置文件
配置文件preempt.json
```
{
    "pending_pod":{
        "jobid":"b38cf3b9-9885-4dff-bc43-2dea05decd94",
        "podid":"1fcf103b-e546-49a7-b5b2-74c215555a42"
    },
    "preempted_pods":[
        {"jobid":"72c42b5c-3360-41ca-9d02-8f022c118e55",
         "podid":"b64131a5-d430-4670-8726-3bcc58c4b522"}
    ],
    "addr":"127.0.0.1:7182"
}
```
./galaxy preempt -f preempt.json


