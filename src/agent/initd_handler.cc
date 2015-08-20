#include "initd_handler.h"
#include <sstream>
#include <boost/lexical_cast.hpp>
#include "gflags/gflags.h"
#include "proto/initd.pb.h"
#include "logging.h"
#include "mutex.h"
#include "pod_info.h"
#include "thread.h"
#include "utils.h"

// DECLARE_int32(agent_monitor_initd_interval);
DECLARE_int32(agent_rpc_initd_timeout);
DECLARE_string(agent_initd_bin);
DECLARE_string(gce_work_dir);
namespace baidu {
namespace galaxy {

InitdHandler::InitdHandler() {
    // pod_info_.reset(new PodInfo());
    // pod_info_->desc = desc;

    // monitor_thread_.reset(new common::Thread());
    // monitor_thread_->Start(boost::bind(&InitdHandler::LoopCheckPodInfo, 
    //                                   this));

   // TODO mvoe to pod manager
   port_ = RandRange(5000, 7999);
   rpc_client_.reset(new RpcClient());
}

InitdHandler::~InitdHandler() {

}

int InitdHandler::Create(const PodDesc& pod) {
    
    // current dirertor as work_dir
    const static std::string current_dir(FLAGS_gce_work_dir + "/");
    std::string work_dir(current_dir + pod.id);

    // create work dir
    int ret = file::Mkdir(work_dir.c_str());
    if (ret == EEXIST) {
        LOG(WARNING, "work dir already exist[%s]", work_dir.c_str());
        return 0;
    } 

    {
    MutexLock lock(&info_mutex_);
    pod_info_.desc = pod;
    }

    // initd startup command
    std::stringstream command;
    command << FLAGS_agent_initd_bin;
    command << " --gce_initd_port=";
    command << port_;

    //////////////////// begin to create initd process
    int stdout_fd = 0;
    int stderr_fd = 0;
    std::vector<int> fd_vector;
    // TODO
    // agent current director
    if (!process::PrepareStdFds(current_dir.c_str(), 
                                &stdout_fd, &stderr_fd)) {
        if (stdout_fd != -1) {
            ::close(stdout_fd); 
        }

        if (stderr_fd != -1) {
            ::close(stderr_fd); 
        }
        LOG(WARNING, "prepare for std file failed"); 
        return -1;
    }

    pid_t child_pid = ::fork();
    if (child_pid == -1) {
        LOG(WARNING, "fork %s failed err[%d: %s]",
            pod.id.c_str(), errno, strerror(errno)); 
        return -1;
    } else if (child_pid == 0) {
        // setpgid  & chdir
        pid_t my_pid = ::getpid();
        process::PrepareChildProcessEnvStep1(my_pid, 
                                             work_dir.c_str());

        process::PrepareChildProcessEnvStep2(stdout_fd, 
                                             stderr_fd, 
                                             fd_vector);

        // prepare argv
        char* argv[] = {
            const_cast<char*>("sh"),
            const_cast<char*>("-c"),
            const_cast<char*>(command.str().c_str()),
            NULL};
        ::execve("/bin/sh", argv, NULL);
        assert(0);
    }

    // close child's std fds
    ::close(stdout_fd); 
    ::close(stderr_fd);
    //////////////////// end to create initd process

    // send execute request
    CreatePodRequest* request = new CreatePodRequest();
    CreatePodResponse* response = new CreatePodResponse();
    request->mutable_pod()->CopyFrom(pod.desc);

    SendRequestToInitd(&Initd_Stub::CreatePod, request, response);

    LOG(INFO, "create initd work_dir[%s]", work_dir.c_str());
    return 0;
}

int InitdHandler::Delete(const PodDesc& pod) {
    return 0;
}

int InitdHandler::UpdateCpuLimit(const PodDesc& pod) {
    return 0;
}


int InitdHandler::Show(boost::shared_ptr<PodInfo>& info) {
    if (info == NULL) {
        return -1;
    }

    // copy pod info
    MutexLock lock(&info_mutex_);
    *info = pod_info_;
    return 0;
}

void InitdHandler::CheckPodInfo() {
    GetPodStatusRequest* request = new GetPodStatusRequest();
    GetPodStatusResponse* response = new GetPodStatusResponse();
    request->set_key(podid_);
    SendRequestToInitd(&Initd_Stub::GetPodStatus, 
                       request, 
                       response);
}

template <class Request, class Response>
void InitdHandler::SendRequestToInitd(
    void(Initd_Stub::*func)(google::protobuf::RpcController*,
                            const Request*, Response*, 
                            ::google::protobuf::Closure*), 
    const Request* request, 
    Response* response) {

    Initd_Stub* initd; 
    std::string endpoint("localhost:");
    endpoint += boost::lexical_cast<std::string>(port_);
    rpc_client_->GetStub(endpoint, &initd);

    boost::function<void(const Request*, Response*, bool, int)> callback;
    callback = boost::bind(&InitdHandler::InitdCallback<Request, Response>, 
                           this, _1, _2, _3, _4);
    rpc_client_->AsyncRequest(initd, func, request, response, 
                              callback, FLAGS_agent_rpc_initd_timeout, 0);
    delete initd;
}

template <>
void InitdHandler::InitdCallback(const GetPodStatusRequest* request, 
                                 GetPodStatusResponse* response, 
                                 bool failed, int error) {
    boost::scoped_ptr<const GetPodStatusRequest> ptr_request(request);
    boost::scoped_ptr<GetPodStatusResponse> ptr_response(response);

    if (failed || error != 0 || ptr_response->status() != kOk) {
        LOG(WARNING, "initd rpc error[%d]", error);
    }

    const PodStatus& status = ptr_response->pod_status();
    {
    MutexLock lock(&info_mutex_);
    pod_info_.usage = status;
    pod_info_.usage.set_jobid(pod_info_.desc.jobid); 
    }
    return;
}

template <class Request, class Response>
void InitdHandler::InitdCallback(const Request* request, 
                                 Response* response, 
                                 bool failed, int error) {
    boost::scoped_ptr<const Request> ptr_request(request);
    boost::scoped_ptr<Response> ptr_response(response);

    if (failed || error != 0 || ptr_response->status() != kOk) {
        LOG(WARNING, "initd rpc error[%d]", error);
    }
    return;
}

} // ending namespace galaxy
} // ending namespace baidu
