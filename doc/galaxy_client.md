Galaxy客户端使用
===================

#客户端名称
galaxy_client

#配置galaxy.flag
在galaxy客户端同级目录需要放置一个galaxy.flag配置文件,主要配置如下内容
```
--username=xxx  
--token=xxx
--nexus_addr=xxx1:8888,xxx2:8888
--nexus_root=xxx
--resman_path=xxx 
--appmaster_path=xxx
```
注：--nexus_root、--resman_path和--appmaster_path不需要强制配置，可以使用默认值

测试配置是否正确
```
./galaxy_client list -o ppp(whatever you want)
-  id  name     type         status   r/p/d/die/f  repli  create_time update_time
-----------------------------------------------------------------------------------
```

如果出现如果下错误信息，则配置错误
```
    1. get appmaster endpoint from nexus failed: Candidate
    2. SendRequest fail:RPC_ERROR_RESOLVE_ADDRESS
```

若直接运行./galaxy_res_client命令且当前目录没有galaxy.flag文件则会出现
```
./galaxy.flag: No such file or directory

两种方法：
    1. 在当前目录按照**配置galaxy.flag**中的方法构造galaxy.flag文件
    2. 按照**配置galaxy.flag**中的方法构造flag文件，并使用--flagfile=选项指明

```
提交job和更新job用到的json文件格式出错时，则会出现如下提示
```
./galaxy_client submit -f app.json

invalid config file, [app.json] is not a correct json format file
1: {
2:     "name": "example",
3:     "type": "kJobService",
4:     "deploy": {
5:         "replica": 1,
6:         "step": 1,
7:         "interval": 1,
8:         "max_per_host": 1,
9:         "tag": "",
10:         "pools": "main_pool"
11:     },
12:     "pod": {
13:         "workspace_volum": 
14:             "size"

[app.json] error: Missing a comma or '}' after an object member.
at overview offset [273], at line number [14]
```

两种方法：
1. 在当前目录按照**配置galaxy.flag**中的方法构造galaxy.flag文件
2. 按照**配置galaxy.flag**中的方法构造flag文件，并使用--flagfile=选项指明

#galaxy_client使用方法
运行./galaxy_client获取运行方法, flagfile的默认值是./galaxy.flag, 若需要指定别的flag文件，运行时加上--flagfile=选项
```
galaxy_client.
galaxy_client [--flagfile=flagfile]
Usage:
      galaxy_client submit -f jobconfig(json format)
      galaxy_client update -f jobconfig(json format) -i id [-t breakpoint -o pause|continue|rollback]
      galaxy_client remove -i id
      galaxy_client list [-o cpu,mem,volums]
      galaxy_client show -i id [-o cpu,mem,volums]
      galaxy_client recover -i id -I podid
      galaxy_client exec -i id -c cmd
      galaxy_client json [-i jobid -n jobname -t num_task -d num_data_volums -p num_port -a num_packages in data_package -s num_service]
Options: 
      -f specify config file, job config file or label config file.
      -c specify cmd.
      -i specify job id.
      -t specify specify task num or update breakpoint, default 0.
      -d spicify data_volums num, default 1
      -p specify port num, default 1
      -a specify packages num in data_package, default 1
      -s specify service num, default 1
      -n specify job name
      -o specify operation.
      --flagfile specify flag file, default ./galaxy.flag
```

## 使用说明
### submit 提交一个job
    参数：
        1. -f(必选)指定job描述配置文件，文件格式是json格式
        2. --flagfile(可选)，指定flag文件，默认是./galaxy.flag
    用法：
        ./galaxy_client submit -f job.json
        ./galaxy_client submit -f job.json --flagfile=./conf/galaxy_yq.flag
    说明:
        1. job的name只支持字母和数字，如果是其他特殊字符，则会被替换成下划线"_", 超过16个字符会被截断
        2. json配置文件的生成见 **json 生成json格式的job配置文件**
        3. job的需要的资源选项包括cpu(必选), mem(必选), tcp(必选), blkio(必选), ports(可选); 其中cpu, mem, tcp选项中如果excess为false表示硬限，为true则为软限
        4. ports选项中name命名端口名称，port指定端口号，端口可以由galaxy动态分配(dynamic)或者指定具体的端口号，galaxy中可使用的端口范围是1025-9999, 10000以上的端口号很容易冲突 
        5. 如果用户程序需要使用到端口，请在配置文件中加上ports配置，在使用到port的services中，注明使用的端口名称port_name, 否则会出现端口绑定失败，程序运行不起来
        6. 如果使用多个端口，端口号必须连续，dynamic不能在两个具体端口号的中间
        7. 端口名称必须唯一
        8. workspace_volum和data_volums配置中, dest_path的值在本配置中必须唯一
        9. services中所有的service_name的值是不能重复的且port_name必须是该service所属task中的ports中定义的

```
端口范例1:
 "ports": [
    {
        "name": "example_port00",
        "port": "1025"
    },
    {
        "name": "example_port01",
        "port": "1026"
    }
]

端口范例2:
 "ports": [
    {   
        "name": "example_port00",
        "port": "1025"
    },
    {   
        "name": "example_port01",
        "port": "1026"
    },
    {   
        "name": "example_port02",
        "port": "dynamic"
    }, 
]

端口范例3:
 "ports": [
    {   
        "name": "example_port00",
        "port": "dynamic"
    },
    {   
        "name": "example_port01",
        "port": "dynamic"
    },
    {   
        "name": "example_port02",
        "port": "dynamic"
    }, 
]
```

### update 更新一个job，支持容器、副本多断点更新；支持更新暂停，回滚
    参数：
        1. -f (必选) 指定job描述配置文件 ，文件格式是json格式
        2. -i (必选) 指定需要更新的jobid, 当仅需要批量更新job，不考虑暂停点，则-f和-i两个参数足够了
        3. -t (可选) 指定暂停点，更新job时，job更新的副本数达到这个值时则暂停更新
        4. -o (可选) 指定更新job时的操作，pause表示暂停，continue表示继续，rollback表示回滚
        5. --flagfile(可选)，指定flag文件，默认是./galaxy.flag
    用法:
        批量更新：./galaxy_client update -i jobid -f job.json  
        断点更新：./galaxy_client update -i jobid -f job.json -t 5
        继续更新：./galaxy_client update -i jobid -o continue
        多断点更新：./galaxy_client update -i jobid -t 5 -o continue
        暂停更新：./galaxy_client update -i jobid -o pause
        回滚：./galaxy_client update -i jobid -o rollback
    说明：
        如果需要停止一个job且保留容器，请更新json文件中的exec_package选项中的version内容并将启动脚本内容改为下面的内容，使用命令./galaxy_client update -i jobid -f job.json
```
#!/bin/bash
 
while true
do
    sleep 5
done
```

### remove 删除一个job
    参数：
        1. -i (必选) 指定需要删除的jobid
        2. --flagfile(可选)，指定flag文件，默认是./galaxy.flag 
    用法：./galaxy_client remove -i jobid

### list 列出所有的job
    参数: 
        1. -o（可选） 值为cpu,mem,volums(用逗号分隔)
            * 当不加这个参数时，会列出cpu、mem、volums的信息；
            * 当-o others(非cpu,mem,volums)则不会列出cpu、mem、volums的信息，仅列出基本信息
        2. --flagfile(可选)，指定flag文件，默认是./galaxy.flag
    用法：
        ./galaxy_client list
        ./galaxy_client --flagfile=./conf/galaxy_yq.flag list
        ./galaxy_client list -o cpu
        ./galaxy_client list -o base
    说明：
        r/p/d/die/f:
            1. r:处于running状态的副本树
            2. p:处于pending状态的副本数
            3. d:处于部署状态的副本数
            4. die:死亡的副本数
            5. f:失败的副本数

```
-  id                                 name     type         status   r/p/d/die/f  repli  cpu(a/u)      memory(a/u)        volums(med/a/u)          create_time          update_time        
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  0  job_20160612_192152_72_ts3       ts3      kJobService  Running  15/0/0/0/0   15     45.000/0.000  135.000G/294.938M  kSsd/150.000G/0.000      2016-06-12 19:21:52  2016-06-12 19:39:16
  -  -                                -        -            -        -            -      -             -                  kDisk/600.000G/469.068M  -                    -                  
  -  -                                -        -            -        -            -      -             -                  kTmpfs/120.000G/0.000    -                    -                  
```

```
-  id                               name     type         status   r/p/d/die/f  repli  cpu(a/u)      create_time          update_time        
-------------------------------------------------------------------------------------------------------------------------------------------------
0  job_20160612_192152_72_ts3       ts3      kJobService  Running  15/0/0/0/0   15     45.000/0.000  2016-06-12 19:21:52  2016-06-12 19:39:16
```

```
-  id                               name     type         status   r/p/d/die/f  repli  create_time          update_time        
-----------------------------------------------------------------------------------------------------------------------------------
0  job_20160612_192152_72_ts3       ts3      kJobService  Running  15/0/0/0/0   15     2016-06-12 19:21:52  2016-06-12 19:39:16

```

### show 展示一个job, 列出的信息包括:提交job时的job配置信息，pod运行信息podinfo，service信息
    参数：  
        1. -i（必选）指定需要更新的jobid
        2. -o（可选） 值为cpu,mem,volums(用逗号分隔)
        3. -b（可选） 有则表示显示job的meta信息
        4. --flagfile(可选)，指定flag文件，默认是./galaxy.flag
    用法:
        ./galaxy_client show -i job_20160612_192152_72_ts3 -o cpu
    说明:
        基本信息：base infomation，job description base infomation，job description deploy infomation，job description pod workspace_volum infomation
                  job description pod data_volums infomation等 
        pod信息: podinfo
        service信息：serviceinfo
        
```
base infomation
  id                          status   create_time          update_time        
---------------------------------------------------------------------------------
  job_20160612_192152_72_ts3  Running  2016-06-12 19:21:52  2016-06-12 19:39:16

job description base infomation
  name  type         run_user
-------------------------------
  ts3   kJobService  galaxy  

job description deploy infomation
  replica  step  interval  max_per_host  tag  pools
-----------------------------------------------------
  15       2     0         10            sjy  test 

job description pod workspace_volum infomation
  size     type       medium  dest_path   readonly  exclusive  use_symlink
----------------------------------------------------------------------------
  20.000G  kEmptyDir  kDisk   /home/work  false     false      false      

job description pod data_volums infomation
  size     type       medium  dest_path    readonly  exclusive  use_symlink
-----------------------------------------------------------------------------
  20.000G  kEmptyDir  kDisk   /home/disk1  false     false      false      
  10.000G  kEmptyDir  kSsd    /home/myssd  false     false      false      
  8.000G   kEmptyDir  kTmpfs  /home/ramfs  false     false      false      

job description pod task infomation
=========================================================
job description pod task [0] base infomation
  -  id  cpu(cores/excess)  memory(size/excess)  tcp_throt(r/re/s/se)       blkio  ports(name/port) 
------------------------------------------------------------------------------------------------------
  0  0   3.000/false        9.000G/false         55.000M/true/35.000M/true  2      REDIS/dynamic    
  -  -   -                  -                    -                          -      linkbase1/dynamic

job description pod task [0] exe_package infomation
-----------------------------------------------
start_cmd: cd bin && sh -x start.sh

stop_cmd: cd bin && sh -x stop.sh

dest_path: bin

version: 2.0.42

job description pod task [0] data_package infomation
-----------------------------------------------
reload_cmd: sh -x reload.sh
  -  version  dest_path
-------------------------
  0  1.1.4    data1    

job description pod task [0] services infomation
  -  name      port_name  use_bns
-----------------------------------
  0  service0  REDIS      false  
  1  service1  linkbase1  false  

podinfo infomation
  -   podid   endpoint   status   fails  container  last_error  update  cpu(a/u)     create_time          update_time        
--------------------------------------------------------------------------------------------------------------------------------------
  0   pod_0   XXXX:6666  Running  0      Ready      kResOk      Yes     3.000/0.000  2016-06-14 19:03:49  2016-06-12 19:39:16
  1   pod_1   XXXX:6666  Running  0      Ready      kResOk      Yes     3.000/0.000  2016-06-12 19:35:22  2016-06-12 19:39:16
  7   pod_2   XXXX:6666  Running  0      Ready      kResOk      Yes     3.000/0.000  2016-06-12 19:33:32  2016-06-12 19:39:16
  8   pod_3   XXXX:6666  Running  0      Ready      kResOk      Yes     3.000/0.000  2016-06-14 18:04:34  2016-06-12 19:39:16

services infomation
  -  podid  service_addr  name  port  status
---------------------------------------------
```

### recover 重新拉起一个失败的pod
    参数：
        1. -i（必选） 指定jobid
        2. -I（必选） 指定podid
    用法：
        ./galaxy_client -i jobid -I podid

### exec 执行命令
    参数:
        1. -i（必选） 指定jobid
        2. -c（必选）指定要执行的命令
        3. --flagfile(可选)，指定flag文件，默认是./galaxy.flag
    用法：
        ./galaxy_client -i jobid -c cmd
    
### json 生成json格式的job配置文件
    参数：
        1. -i(可选) 指定jobid, 可生成指定jobid的job配置
        2. -n(可选) 指定jobname，默认为example
        3. -t(可选) 指定task数，默认0
        4. -d(可选) 指定data_volums数，默认1
        5. -p(可选) 指定port数，默认1
        6. -a(可选) 指定data package数，默认1
        7. -s(可选) 指定services数，默认1
        8. --flagfile(可选)，指定flag文件，默认是./galaxy.flag
    用法:
        ./galaxy_client json -i jobid           ==>jobid对应的json内容
        ./galaxy_client json                    ==>共享盘容器模板
        ./galaxy_client json -t 1               ==> 标准模板
        ./galaxy_client json -n example -d 2
    说明:
        1. type的value必须为kJobMonitor, kJobService, kJobBatch, kJobBestEffort中的一个
        2. volum中medium的value必须为kSsd, kDisk, kBfs, kTmpfs中的一个
        3. volum中type的value:kEmptyDir, kHostDir
        4. 所有volums中的dest_path的value不能重复
        5. 配置中所有size以及recv_bps_quota, send_bps_quota的value支持的单位有K, M, G, T, P, E, 如1G
        6. data_package配置中的reload_cmd不能为空,这一项主要是支持词典的热升级
        7. services中所有的service_name的值是不能重复的且port_name必须是该service所属task中的ports中定义的
        8. services中tag, health_check_type, health_check_script, token均为注册bns时需要的参数, use_bns为true时，token不能为空
        9. volum_jobs指定依赖共享盘容器的id（此id由管理员提供）, 如果不需要依赖，请去掉此项

        提交job的json配置文件中tag, ports, data_volums, data_packages, stop_cmd, health_cmd, data_package, services这些选项如果不需要可以不写

```
标准模板
{
    "name": "example",
    "type": "kJobService",
    "volum_jobs": "",
    "deploy": {
        "replica": 1,
        "step": 1,
        "interval": 1,
        "max_per_host": 1,
        "tag": "",
        "pools": "example,test"
    },
    "pod": {
        "workspace_volum": {
            "size": "300M",
            "type": "kEmptyDir",
            "medium": "kDisk",
            "dest_path": "/home/work",
            "readonly": false,
            "exclusive": false,
            "use_symlink": false
        },
        "data_volums": [
            {
                "size": "800M",
                "type": "kEmptyDir",
                "medium": "kDisk",
                "dest_path": "/home/data/0",
                "readonly": false,
                "exclusive": false,
                "use_symlink": true
            }
        ],
        "tasks": [
            {
                "cpu": {
                    "millicores": 1000,
                    "excess": false
                },
                "mem": {
                    "size": "800M",
                    "excess": false
                },
                "tcp": {
                    "recv_bps_quota": "30M",
                    "recv_bps_excess": false,
                    "send_bps_quota": "30M",
                    "send_bps_excess": false
                },
                "blkio": {
                    "weight": 500
                },
                "ports": [
                    {
                        "name": "example_port00",
                        "port": "12300"
                    }
                ],
                "exec_package": {
                    "start_cmd": "sh app_start.sh",
                    "stop_cmd": "",
                    "health_cmd": "",
                    "package": {
                        "source_path": "ftp://***.baidu.com/home/users/***/exec/0/linkbase.tar.gz",
                        "dest_path": "/home/spider/0",
                        "version": "1.0.0"
                    }
                },
                "data_package": {
                    "reload_cmd": "",
                    "packages": [
                        {
                            "source_path": "ftp://***.baidu.com/home/users/***/data/00/linkbase.dict.tar.gz",
                            "dest_path": "/home/spider/00/dict",
                            "version": "1.0.0"
                        }
                    ]
                },
                "services": [
                    {
                        "service_name": "example_service00",
                        "port_name": "example_port00",
                        "user_bns": false,
                        "tag": "",
                        "health_check_type": "",
                        "health_check_script": "",
                        "token": ""
                    }
                ]
            }
        ]
    }
}
```

```
最简模板
{
    "name": "example",
    "type": "kJobService",
    "volum_jobs": "",
    "deploy": {
        "replica": 1,
        "step": 1,
        "interval": 1,
        "max_per_host": 1,
        "pools": "test"
    },
    "pod": {
        "workspace_volum": {
            "size": "300M",
            "type": "kEmptyDir",
            "medium": "kDisk",
            "dest_path": "/home/work",
            "readonly": false,
            "exclusive": false,
            "use_symlink": false
        },
        "tasks": [
            {
                "cpu": {
                    "millicores": 1000,
                    "excess": false
                },
                "mem": {
                    "size": "800M",
                    "excess": false
                },
                "tcp": {
                    "recv_bps_quota": "30M",
                    "recv_bps_excess": false,
                    "send_bps_quota": "30M",
                    "send_bps_excess": false
                },
                "blkio": {
                    "weight": 500
                },
                "exec_package": {
                    "start_cmd": "sh app_start.sh",
                    "package": {
                        "source_path": "ftp://***.baidu.com/home/users/***/exec/0/linkbase.tar.gz",
                        "dest_path": "/home/spider/0",
                        "version": "1.0.0"
                    }
                }
            }
        ]
    }
}
```
