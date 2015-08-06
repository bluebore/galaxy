#include "task_manager.h"

namespace baidu {
namespace galaxy {

TaskManager::TaskManager() {
}

TaskManager::~TaskManager() {
}

int TaskManager::CreatePodTasks(const PodDesc& pod) {
    return 0;
}

int TaskManager::DeletePodTasks(const std::string& podid) {
    return 0;
}

int TaskManager::UpdatePodCpuLimit(const std::string& podid, 
                                   const uint32_t millicores) {
    return 0;
}

int TaskManager::QueryPodTasks(const std::string& podid, 
                               std::vector<TaskInfo>* tasks) {
    return 0;
}

int TaskManager::Execute(const std::string& command) {
    return 0;
}

int Update(const std::string& taskid, const uint32_t millicores) {
    return 0;
}

void TaskManager::LoopCheckTaskStatus() {
    
}

}
}
