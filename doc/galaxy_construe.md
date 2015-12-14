#Galaxy串讲

##综述
Galaxy是一个数据中心操作系统，系统的目标是最大化资源利用率和降低应用部署运维代价。

ter-slave结构，主要由总控节点Master和执行节点Agent构成，Master负责核心数据的存储与任务的分配，Agent负责任务的执行。并提供
统一的客户端galaxy_client完成用户操作。

系统的外围是监控与日志收集子系统，负责将系统内任务的状态与数据进行收集和展示。

系统底层依赖分布式文件系统持久化数据，使用leveldb。

![架构图](https://github.com/bluebore/galaxy/blob/master/images/galaxy_arch.png?raw=true)

Nexus的作用进行选主、服务发现和元数据存储。
 - 多台机器向nexus机器注册，作为备选master机器；
 - 若当前的master宕机或者挂掉，则在nexus注册的某台正常的机器从nexus获取job的信息（因为提交job的时候就会写nexus，当job完成或kill时，则从nexus上抹去此job的信息）
 - 若部署的nexus机器出现问题，则无法恢复
 - Nexus存储的是全局的信息job信息以及label信息，每次重启master的时候会从nexus上reload job信息和label信息

##主要模块
主要模块分为三大块：agent、master、scheduler；agent负责pod管理和task管理，task的真正运是在agent上；master作为整个系统的中心，负责job管理；scheduler负责job优先级计算以及资源分配的可行性计算。他们之间的关系如下：

![Master,agent和scheduler关系图](https://raw.githubusercontent.com/May2016/galaxy/work/images/MAS.png)

###Gce
Gce从属于agent模块，主要负责task的命令行的启动
![Gce模块流程图](https://raw.githubusercontent.com/May2016/galaxy/work/images/gce_flowchart.png)

###Agent
![Agent模块流程图](https://raw.githubusercontent.com/May2016/galaxy/work/images/agent_flowchart.png)
####具体步骤如下
 - 创建agent_service服务实例，agent_service创建时内部会生成MasterWatcher实例，用于监控master是否发生变化，并获取当前最新的master_stub
 - agent_service初始化
     ![agent_init流程图](https://raw.githubusercontent.com/May2016/galaxy/work/images/agent_init_flowchart.png)
     1. 运行环境初始化与隔离
        - 根据flag配置文件对resource_capacity_赋值:
        ```
        resource_capacity_millicores = FLAGS_agent_millicores_share;
        resource_capacity_.memory = FLAGS_agent_mem_share;
        ```
        - 创建工作目录FLAGS_agent_work_dir
        - 持久化实例PersistenceHandler persistence_handler初始化，使用leverldb接口打开FLAGS_agent_persistence_path文件
     2. pod_manager_初始化
        - 与gce模块通信用的端口初始化FLAGS_agent_initd_port_begin和FLAGS_agent_initd_port_end范围内的端口初始化IndexList<int> initd_free_ports_
        - 删除FLAGS_agent_gc_dir目录
        - task_manager_初始化
            - 初始化flag配置文件中的子系统:cpu, memory, tcp_throt, 其他
            - 每个task对应子系统目录的创建，cgroup中相关文件的配置操作，每个task的资源准备，各个子系统路径等
    3. RestorePods:使用persistence_handler_从本地文件中scan pod的信息存入到内存pods_descs_[pod.pod_id] = pod.pod_desc并load到pod_manager_中（pod_manager_.ReloadPod），调用AllocResource更新resource_capacity_中的资源信息：used_port，millicores，memory
        - task_manager_->ReloadTask,三种类型的进程：部署任务的进程：task_id_deploy，运行任务的进程：task_id，结束任务的进程：task_id_stop
        ![ReloadTask流程图](https://raw.githubusercontent.com/May2016/galaxy/work/images/reloadtask_flowchart.png)
        - DelayCheckTaskStageChange函数会根据上面task的stage状态来部署、运行、或者stoping app的task
            - 对于kTaskStagePENDING则会判断是是否有压缩包，有则按照wget且解包或直接解包，没有则直接运行app的task启动程序
            - 对于kTaskStageDEPLOYING则判断是否部署完成，部署完成则直接运行app的task启动程序
            - 对于kTaskStageRUNNING，如果请求gce模块失败次数没达到最大尝试次数，则重新运行app的task启动程序
            - 对于kTaskStageSTOPPING则如果停止超时或获取状态错误，则释放该task相关的资源，cgroup环境清理等
    4. 调用RegistToMaster函数
        - 绑定HandleMasterChange函数，用来切换mater 
        - PingMaster，使用rpc_client_向master发送请求，调用Master_Stub::HeartBeat函数
    5. 线程池加入AgentImpl::KeepHeartBeat，用于向master发送心跳
    6. 线程池加入AgentImpl::LoopCheckPods

###Master
- Master启动
    1. 创建mater接口实例
    2. 向rpc_server注册该接口实例
    3. 启动server服务，等待agent想master发送请求调用HeartBeat函数（请求中会将agent本身的地址和端口作为请求传送给master的参数），HeartBeat函数调用job_manager_.KeepAlive(agent_addr)函数,KeepAlive：
        - 遍历agents_查找是否有获取得到的地址agent_addr，若没有则agents_[agent_addr] = agent；对agent打tag信息，将此agent中的state设置为kAlive状态
        - 将HandleAgentOffline(agent_addr)加入线程池：根据获得的pod信息将需要拉起的pod的stage由kStageDeath改为kStagePending状态，并将此pod对应的jobid加入到集合scale_up_jobs_

    4. master_impl->Init():从nexus上scan job信息和label信息加载到内存
        - ReloadJobInfo->JobManager
            Pod信息:job->pod_desc_[desc.version()] = desc             
            Job信息：jobs_[job_id] = job
            Master对agent的query从job 管理实例存在开始，调用ScheduleNextQuery将JobManager::Query加入线程池Query调用QueryAgent,如果agent的状态是kAlive，则使用rpc_client_获取Agent_Stub，异步请求Agent_Stub::Query，获取agent上的资源分配情况，如总共的millicore，mem、已经使用了的millicores，mem，使用的端口情况，
            ```
            rpc_client_.AsyncRequest(stub, &Agent_Stub::Query, request, response,
                                          query_callback，FLAGS_master_agent_rpc_timeout, 0);

            ```
            JobManager::QueryAgentCallback使用上面返回的信息reponse里的agentinfo信息，调用UpdateAgent查看现在agent的版本号与返回的版本号是否一致，不一致则在原来的版本号上加1，还有判断资源、使用的端口号是否一致，如果都不一致，都分别将版本号加1

        - ReloadLabelInfo：label是agent上的标签信息，agent与label是多对多的关系
        
    5. 等待终止信号

###Schedule
1. scheduler从scheduler_main.cc启动
2. 调用SchedulerIO io Init()函数
    -master_watcher_初始化绑定HandleMasterChange函数，监听master是否发生变化
    -使用rpc_client获取master_stub_
3.  调用io.Sync()函数
    - SyncResources()同步资源

        - 使用master_stub_里的函数Master_Stub::GetResourceSnapshot获取agents以及deleted_agents同步资源情况，并将SchedulerIO::SyncResources加入到线程池

    - SyncPendingJob()同步jobs
        
        - 使用master_stub_里的函数GetPendingJobs获取scale_up_jobs（JobManager::ProcessScaleUp函数），scale_down_jobs（JobManager::ProcessScaleDown函数）以及need_update_jobs（JobManager::ProcessUpdateJob）三个集合中的数据。

        - 对于scale_up_jobs调用scheduler_.ScheduleScaleUp计算job的优先级，计算feasibility（可行性），其他两个集合也是如此，最后返回到std::vector<ScheduleInfo*> propose
    
        - 对于获得的propose，使用master_stub_向函数Master_Stub::Propose（调用job_manager_.Propose）发送请求，根据scheduler的计算结果propose来实际根据task的状态来启动、停止、更新task。

