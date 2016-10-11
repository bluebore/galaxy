[![Build Status](https://travis-ci.org/baidu/galaxy.svg?branch=galaxy3)](https://travis-ci.org/baidu/galaxy)

galaxy 3.0

Galaxy 3.0设计
=============

# 背景

Galaxy3.0是对Galaxy2.0的重构，主要解决以下问题：  

1. 容器管理和服务管理紧耦合：服务的升级和启停都伴随容器的销毁和调度；
2. 没有磁盘管理，只能管理home盘；
3. 不支持用户quota和记账；
4. 机器管理功能缺失；
5. Naming功能可用性低；
6. Trace功能不完善；

# 系统架构

        Galaxy3.0架构上分为2层： 资源管理层和服务管理层，每层都是主从式架构  
        1. 资源管理层由ResMan(Resource Manager)和Agent构成  
        2. 服务管理层由AppMaster和AppWorker构成;

 
       +-------------------+-----------------------------+
       |                   |               |             |
       |                   |   MapReduce   |   Spark     |
       |                   |               |             |
       |                   +-----------------------------+
       |                                                 |
       |               Service Management                |  ---> {AppMaster +　AppWorkers}
       |                                                 |
       +-------------------------------------------------+
       |                                                 |
       |               Resource Management               |  ---> {ResMan + Agents}
       |                                                 |
       +-------------------------------------------------+

## 1. 资源管理层（Resource Management)
组件： ResMan + Agents  
一个Galaxy集群只有一个处于工作状态的ResMan，负责容器的调度，为每个容器找到满足部署资源要求的机器；  
ResMan通过和部署在各个机器上的Agent通信，来创建和销毁容器；  
容器： 一个基于linux cgroup和namspace技术的资源隔离环境；   
容器里默认会启动AppWorker进程，是容器内的第一个进程，也就是根进程；   
ResMan不暴露给普通用户接口， 仅供内部组件以及集群管理员使用；  

## 2. 服务管理层 (Service Management)
组件：　AppMaster + AppWorkers  
AppMaster是外界用户操作Galaxy的唯一入口；  
一个Galaxy集群通常只有一个AppMaster，负责服务的部署、更新、启停和状态管理，把服务实例分发到各个机器上的容器内启动并跟踪状态；  
AppMaster通过调用ResMan的RPC接口创建容器，容器内自动拉起AppWorker进程；  
容器内的AppWorker进程通过和AppMaster进程通信，获得需要在容器内执行的命令，包括部署、启停、更新等等；  
AppWorker会汇报服务的状态给AppMaster，例如托管的服务是否在正常运行，进程退出码等；  

## 调度逻辑

用户提交的Job内容主要是两部分：资源需求 +　程序描述  
资源需求： CPU核数、内存大小、磁盘容量、机器Lable、端口范围、mount路径  
程序描述： 部署命令、启动命令、停止命令、更新命令、版本号  

### 1. ResMan的调度逻辑
ResMan通过定时查询Agent，获得每个Agent上面可分配的资源  
ResMan不断检查当前是否有处于Pending状态的容器， 寻找有资源的Agent创建容器；  
创建失败的容器，又进入Pending状态，等待重新调度；  
不符合预期的容器， ResMan命令Agent销毁， 重新进入Pending状态；  
ResMan确保容器的个数始终符合用户的需求；  

### 2. AppMaster的调度逻辑
AppMaster等待AppWorkers的定时汇报；  
如果AppWorker汇报的服务状态不符合AppMaster的预期，则AppMaster返回一些命令让AppWorker执行；  
> a) 部署： AppWorker汇报目前没有运行任何服务， AppMaster返回部署命令给AppWorker;  
> b) 启动： AppWorker汇报部署成功了， AppMaster返回启动命令给AppWorker;  
> c) 更新： AppWorker汇报当前服务的版本号， AppMaster发现不匹配， 返回更新命令给AppWorker;  
> d) 失败处理： AppWorker汇报（部署失败 or 启动失败 or 更新失败）， AppMaster记录此次异常，并根据策略决定是否让AppWorker继续重试；  

## 容错

1. ResMan，AppMaster都有备份，通过Nexus抢锁来Standby;  
2. Agent跟踪每个容器的状态汇报给ResMan，当容器个数不够或者不符合ResMan的要求时，就需要调度：创建或删除容器；  
3. AppWorker负责跟踪用户程序的状态，当用户程序coredump、异常退出或者被cgroup kill后，反馈状态给AppMaster，AppMaster根据指定策略命令AppWorker是否再次拉起用户的服务； 
4. 由于机器缺陷或者网络分割，可能导致ResMan认为容器个数足够，但是AppMaster发现服务实例数不够的情况：  
> 例如： 磁盘坏了、端口被占用等， 导致用户服务始终无法拉起；  
> 这种情况下， AppMaster可以调用ResMan的接口，增大容器个数（有上限）；  

## 服务发现
1. SDK通过Nexus发现AppMaster地址；  
2. SDK请求AppMaster，发现每个Job实例的地址和当前的服务状态；  
3. AppMaster会定时同步服务地址和状态到第三方Naming系统（如BNS,Nexus,ZK等);  

## 服务更新
1. SDK通过Nexus发现指定的Job的AppMaster地址；  
2. SDK请求AppMaster, AppMaster将服务更新命令传播给AppWorker, AppWorker将更新状态反馈给AppMaster;  
3. AppWorker和AppMaster的通信方式是Pull的方式，因此AppMaster可以根据当前的情况来决定部署的暂停和步长控制；  
4. 服务的更新都在容器内进行，不涉及到容器的销毁和创建  

# 权限管理和quota管理模型
1. 集群（Cluster）：共用同一ResMan的host/agent及服务
2. 机器池（Pool）： 一个host/agent只能属于一个机器池，一个机器池通常有很多host/agent。一个集群中可能有多个机器池。机器池用于资源及环境的硬隔离， 也是权限分配的单位。
3. 用户（User）：galaxy用户
4. 权限（Authority）：某用户在某机器池上具有的某种操作权限，如对Job的增、删、改、查权限等。用户可以同时对多个机器池具有多项权限。
5. 配额（Quota）：配额是对用户在集群中拥有资源量的描述， 包含cpu配额，内存配额， 磁盘空间配额，可提交任务数量配额等。用户的配额和具体的机器池没有关系。
6. 标签（label）： 标签一般用来表征一批拥有某种特征的机器，标签和机器是多对多的关系。有权限的用户可以对机器池中的机器打标签，提交任务时可指定标签。

# 系统依赖
1. Nexus作为寻址和元信息保存  
2. MDT作为用户日志的Trace系统  
3. Sofa-PbRPC作为通信基础库  

