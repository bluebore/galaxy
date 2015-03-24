# Galaxy
Galaxy
## Todolist
###主线：
1. Agent启动子进程时能关闭自己所有的文件     -- **Done**(FD_CLOEXEC解决)
2. Agent启动任务时，放在本任务专用一个文件夹里，而不是现在的/home/下


###非主线：
1. Agent能收集当期机器的硬件信息（CPU核数、内存、磁盘个数&大小）
2. Agent限制任务的资源占用
3. Agent能够自己去下载任务二进制包，而不是现在master分发的方式。

### 尝鲜
```
comake2 && make -j12
cd sandbox
sh local-run.sh
sh attach-task.sh
#clean
sh local-killall.sh
```
