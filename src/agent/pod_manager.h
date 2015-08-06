#ifndef POD_MANAGER_H
#define POD_MANAGER_H

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
#include "mutex.h"
#include "pod_info.h"
#include "initd_handler.h"

namespace baidu {

namespace common {
class Thread;
}

namespace galaxy {

class PodManager {
public:
    PodManager();

    ~PodManager();

    int Run(const PodDesc& pod);

    int Kill(const PodDesc& pod);

    int Query(const std::string& podid, 
              boost::shared_ptr<PodInfo>& info);

    int List(std::vector<std::string>* pod_ids);

private:
    enum Operation {
        kCreate,
        kDelete,
    };

    typedef std::map<std::string, boost::shared_ptr<PodInfo> > PodInfosType; 

    // typedef std::map<std::string, boost::shared_ptr<InitdHandler> > PodHandlersType; 

private:
    int Load();

    int Dump();

    int Set(PodInfo* info);

    int Delete(const std::string& podid);

    void LoopCheckPodInfos();

    int FesibilityCheck(const Resource& resource);

    int DoPodOperation(const PodDesc& pod, const Operation op);

private:
    Mutex infos_mutex_;
    PodInfosType pod_infos_;

    // Mutex handlers_mutex_;
    // PodHandlersType pod_handlers_;

    // boost::scoped_ptr<common::Thread> monitor_thread_;
};

}
}

#endif
