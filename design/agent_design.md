## Agent 需求 ##

Agent作为单机Demon程序，主要从如下角度进行管理工作：
 - **Instance** : 负责Instance本机内实例的创建，销毁，更新，具体表现为，接收Master任务后的创建，
 异常时数据回收销毁，环境清理，实例健康检查，Frozen模式支持可排查，环境Gc，用户自定义行为，以及与Instance
 相关的辅助服务（日志收集服务的起停，业务监控服务的起停）
 - **Container** : 负责Container环境的创建，更新，删除回收.具体表现为,如cgroup下的各子系统配置功能
 支持，container级别监控的收集等
 - **Machine** : Machine级别的整机监控，异常发现等机器级别的服务
 - **Agent** : 包含支持查询服务，屏蔽业务动态端口导致的查询问题（类似proxy），自身的异常恢复， 与master通信，心跳等
 - **下载器** : 支持各种下载源，下载方式，以及本地cache管理等
 

**TODO** ： 以上内容待讨论整理

### Agent 设计

#### 运行task设计
运行一个task主要使用一下几大对象：
* TaskRunner 每个task都会有一个TaskRunner,包含Start,Stop,IsRunning(health check),State(任务消耗资源情况)方法
* TaskManager 全局就一个TaskManager,管理所有的TaskRunner，负责TaskRunner的创建 启动，销毁 
* Workspace 每个task都会有个一个Workspace,包含Create,Clean,IsExpire(用于资源GC),GetPath(工作目录跟路径)
* WorkspaceManager 全局一个WorkspaceManager,管理所有Workspace 


#### TaskRunner
针对不同的执行方式可以实现CommandTaskRunner ContainerTaskRunner.
目前CommandTaskRunner实现方式：
* Start , fork 一个子进程，在子进程执行execl task的命令
* Stop , 在第一步fork是记住pid,作为任务的父pid,通过pid获取 process tree,如何优雅停止process tree,自上而下，还是自下而上？
* IsRuning , 目前实现方式是检查父pid是否存在

