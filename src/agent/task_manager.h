#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <map>
#include <vector>
#include <string>
#include "mutex.h"
#include "thread.h"
#include "pod_info.h"
#include "proto/galaxy.pb.h"
#include "proto/initd.pb.h"

namespace baidu {

namespace common {
class Thread;
}

namespace galaxy {

struct TaskDesc {
    TaskDescriptor task;
    int initd_port;
};

struct TaskInfo {
    // meta infomantion
    TaskDesc desc;

    // runtim information
    ProcessInfo process;
    TaskStatus status;
    std::string cgroup_path;
};

class TaskManager {
public:
    TaskManager();

    ~TaskManager();

    int CreateTasks(const std::vector<TaskDesc>& tasks);

    int DeleteTasks(const std::vector<std::string>& taskid);

    int UpdateTaskCpuLimit(const std::string& taskid, 
                           const uint32_t millicores);

    // copy from
    int QueryTasks(const std::vector<std::string>& taskid, 
                   std::vector<TaskInfo>* tasks);
private:

    int Execute(const std::string& command);

    int Kill(const std::string& taskid);

    // int Show(boost::shared_ptr<TaskInfo>& info);

    void LoopCheckTaskStatus();

private:

    Mutex task_mutex_;

    // key taskid 
    std::map<std::string, TaskInfo * > tasks_;
    // Thread  
    boost::scoped_ptr<common::Thread> monitor_thread_;
};

}
}

#endif
