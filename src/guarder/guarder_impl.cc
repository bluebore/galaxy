// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yuanyi03@baidu.com

#include "guarder/guarder_impl.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>

#include <vector>

#include <boost/bind.hpp>
#include "gflags/gflags.h"
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"
#include "agent/utils.h"
#include "common/util.h"
#include "common/logging.h"

DECLARE_string(container);
DECLARE_string(cgroup_root);
DECLARE_string(cgroup_subsystem);

namespace galaxy {

GuarderImpl::GuarderImpl()
    : process_status_(), 
      cgroup_subsystems_(),
      cgroup_root_(),
      lock_(),
      background_thread_(NULL) {
    background_thread_ = new ThreadPool();
    background_thread_->DelayTask(5000, boost::bind(&GuarderImpl::ThreadWait, this));
}

GuarderImpl::~GuarderImpl() {
    if (background_thread_ != NULL) {
        delete background_thread_; 
        background_thread_ = NULL;
    }

    std::map<std::string, ProcessStatus>::iterator it = 
        process_status_.begin();
    for (; it != process_status_.end(); ++it) {
        int32_t kill_pid = it->second.gpid();
        ::killpg(kill_pid, SIGKILL);    
        int status = 0;
        ::wait(&status);
    }
}

bool GuarderImpl::Init() {
    if (FLAGS_container == "cgroup") {
        boost::split(cgroup_subsystems_, 
                FLAGS_cgroup_subsystem, 
                boost::is_any_of(","), 
                boost::token_compress_on);         
        cgroup_root_ = FLAGS_cgroup_root;
    }    
    return true;
}

void GuarderImpl::CollectFds(std::vector<int>* fd_vector) {
    pid_t current_pid = ::getpid();
    common::util::GetProcessFdList(current_pid, *fd_vector);
    return;
}

bool GuarderImpl::AttachCgroup(const std::string& cgroup_path, pid_t pid) {
    if (cgroup_subsystems_.size() == 0) {
        return true; 
    }
    if (cgroup_path == "") {
        return false;
    }
    std::string tasks_file_prefix = cgroup_root_ + "/";
    std::string tasks_file_suffix = cgroup_path + "/tasks"; 
    for (size_t i = 0; i < cgroup_subsystems_.size(); i++) {
        std::string task_file_path = tasks_file_prefix + 
            cgroup_subsystems_[i] + "/" + tasks_file_suffix;   
        if (0 != 
                common::util::WriteIntToFile(task_file_path, pid)) {
            return false; 
        }
    }
    return true;
}

void GuarderImpl::RunProcess(
        ::google::protobuf::RpcController* /*controller*/,
        const ::galaxy::RunProcessRequest* request,
        ::galaxy::RunProcessResponse* response,
        ::google::protobuf::Closure* done) {

    if (!request->has_process_id()
            || !request->has_start_cmd()
            || !request->has_pwd()
            || !request->has_user()) {
        response->set_status(
                kGuarderExecuteStateInputError);
        done->Run();
        return;
    }
    LOG(INFO, "run process %s at %s by %s",
            request->start_cmd().c_str(),
            request->pwd().c_str(),
            request->user().c_str());

    // prepare data for subprocess
    int stdout_fd = -1, stderr_fd = -1;
    std::vector<int> fd_vector;
    //pid_t parent_pid = ::getpid();
    std::string user = request->user();
    std::string pwd = request->pwd();
    std::string start_cmd = request->start_cmd();
    std::string cgroup_path = "";
    std::vector<std::string> envs;
    for (int i = 0; i < request->envs_size(); i++) {
        envs.push_back(request->envs(i));
    }
    if (request->has_cgroup_path()) {
        cgroup_path = request->cgroup_path(); 
    }
    CollectFds(&fd_vector);

    passwd* pw = ::getpwnam(user.c_str());
    if (pw == NULL) {
        LOG(WARNING, "getpwnam %s failed for process %s err[%d: %s]",
                user.c_str(), request->process_id().c_str(), 
                errno, strerror(errno)); 
        response->set_status(kGuarderExecuteStateInternalError);
        done->Run();
        return;
    }
    uid_t userid = ::getuid();
    if (pw->pw_uid != userid && userid == 0) {
        if (!file::Chown(pwd, pw->pw_uid, pw->pw_gid))  {
            LOG(WARNING, "chown %s failed", pwd.c_str()); 
            response->set_status(kGuarderExecuteStateInternalError);
            done->Run();
            return;
        }
    }

    if (!PrepareStdFds(request->pwd(), 
                &stdout_fd, &stderr_fd)) {
        if (stdout_fd != -1) {
            close(stdout_fd); 
        } 
        if (stderr_fd != -1) {
            close(stderr_fd); 
        }
        response->set_status(
                kGuarderExecuteStateInternalError);
        done->Run();
        return;
    }
     
    pid_t child_pid = ::fork();
    if (child_pid == -1) {
        response->set_status(
                kGuarderExecuteStateInternalError);         
        LOG(WARNING, "fork err for process %s err[%d: %s]",
                request->process_id().c_str(), errno, 
                strerror(errno));
    } else if (child_pid == 0) {
        // setgpid
        pid_t my_pid = getpid();
        int ret = ::setpgid(my_pid, my_pid);
        if (ret != 0) {
            assert(0); 
        }

        // attach group, garbage collect by agent
        if (!AttachCgroup(cgroup_path, my_pid)) {
            //LOG(WARNING, "attch failed %s", request->start_cmd().c_str());
            assert(0); 
        }

        ::chdir(pwd.c_str());

        passwd *pw = ::getpwnam(user.c_str());
        if (pw == NULL) {
            //LOG(WARNING, "getpwnam failed %s", request->start_cmd().c_str());
            assert(0); 
        } 
        uid_t userid = ::getuid();
        // root userid = 0
        if (userid == 0 
                && pw->pw_uid != userid) {
            ::setuid(pw->pw_uid); 
        }

        while (dup2(stdout_fd, STDOUT_FILENO) == -1 
                && errno == EINTR) {}
        while (dup2(stderr_fd, STDERR_FILENO) == -1
                && errno == EINTR) {}
        for (size_t i = 0; i < fd_vector.size(); i++) {
            if (fd_vector[i] == STDOUT_FILENO
                    || fd_vector[i] == STDERR_FILENO
                    || fd_vector[i] == STDIN_FILENO) {
                continue; 
            } 
            close(fd_vector[i]);
        }
        char *argv[] = {
            const_cast<char*>("sh"), 
            const_cast<char*>("-c"),
            const_cast<char*>(start_cmd.c_str()),
            NULL};

        char* env[envs.size() + 1];
        for (size_t i = 0; i < envs.size(); i++) {
            env[i] = const_cast<char*>(envs[i].c_str());     
        }
        env[envs.size()] = NULL;
        ::execve("/bin/sh", argv, env);  
        //LOG(WARNING, "execve failed %s err[%d: %s]", 
        //        request->start_cmd().c_str(),
        //        errno, strerror(errno));
        assert(0);
    }

    ProcessStatus status;
    status.set_process_id(request->process_id());
    status.set_pid(child_pid); 
    status.set_gpid(child_pid);
    status.set_state(kProcessStateRunning);
    status.set_ret_code(0);
    LOG(INFO, "process with key %s start pid %ld", 
            status.process_id().c_str(),
            child_pid);
    {
        MutexLock scoped_locked(&lock_);     
        process_status_[status.process_id()] = status;  
    }
    response->set_process_id(status.process_id());     
    response->set_status(kGuarderExecuteStateOK);
    response->set_pid(child_pid);
    response->set_gpid(child_pid);

    done->Run();
    return;
}

void GuarderImpl::StatusProcess(
        ::google::protobuf::RpcController* /*controller*/,
        const ::galaxy::StatusProcessRequest* request,
        ::galaxy::StatusProcessResponse* response,
        ::google::protobuf::Closure* done) {
    MutexLock scoped_locked(&lock_);
    if (request->has_process_id()) {
        std::map<
            std::string, 
            ProcessStatus>::iterator it = 
                process_status_.find(request->process_id());
        if (it != process_status_.end()) {
            ProcessStatus* status = response->add_status();    
            status->CopyFrom(it->second);
        }
    } else {
        std::map<
            std::string, 
            ProcessStatus>::iterator it = 
                process_status_.begin();
        for (; it != process_status_.end(); ++it) {
            ProcessStatus* status = response->add_status();    
            status->CopyFrom(it->second);
        }
    }
    done->Run();
    return;
}

void GuarderImpl::ThreadWait() {
    {
        MutexLock scoped_locked(&lock_);
        int status;
        pid_t pid = ::waitpid(-1, &status, WNOHANG);     
        if (pid > 0 && WIFEXITED(status)) {
            LOG(WARNING, "process %d exits ret %d",
                    pid, WEXITSTATUS(status));         
            std::map<std::string, ProcessStatus>::iterator it 
                = process_status_.begin();
            for (; it != process_status_.end(); ++it) {
                if (it->second.pid() == pid) {
                    it->second.set_state(kProcessStateError);
                    it->second.set_ret_code(WEXITSTATUS(status));
                    break;
                } 
            }
        }
    }
    background_thread_->DelayTask(5000, boost::bind(&GuarderImpl::ThreadWait, this));
}

void GuarderImpl::KillProcess(
        ::google::protobuf::RpcController* /*controller*/,
        const ::galaxy::KillProcessRequest* request,
        ::galaxy::KillProcessResponse* response,
        ::google::protobuf::Closure* done) {
    if (!request->has_process_id()
            || !request->has_signal()) {
        response->set_status(
                kGuarderExecuteStateInputError);
        done->Run();
        return;
    }
    MutexLock scoped_locked(&lock_);
    std::map<std::string, 
        ProcessStatus>::iterator it 
            = process_status_.find(request->process_id());
    if (it == process_status_.end()) {
        response->set_status(kGuarderExecuteStateInputError);
        done->Run();
        return;
    }

    ProcessStatus status = it->second;
    int ret = 0;    
    if (request->has_is_kill_group() &&
            request->is_kill_group()) {
        ret = ::killpg(status.gpid(), request->signal()); 
    } else {
        ret = ::kill(status.pid(), request->signal()); 
    }

    response->set_status(ret);
    response->set_kill_errno(errno);
    done->Run();
    return;
}

void GuarderImpl::RemoveProcessState(
        ::google::protobuf::RpcController* /*controller*/,
        const ::galaxy::RemoveProcessStateRequest* request,
        ::galaxy::RemoveProcessStateResponse* response,
        ::google::protobuf::Closure* done) {
    if (!request->has_process_id()) {
        response->set_status(
                kGuarderExecuteStateInputError); 
        done->Run();
        return;
    }
    MutexLock scoped_locked(&lock_);
    std::map<std::string, 
            ProcessStatus>::iterator it =
                process_status_.find(request->process_id());
    if (it != process_status_.end()) {
        process_status_.erase(it); 
    } 
    response->set_status(kGuarderExecuteStateOK);
    done->Run();
    return;
}

void GuarderImpl::CheckProcess(
        ::google::protobuf::RpcController* /*controller*/,
        const ::galaxy::CheckProcessRequest* request,
        ::galaxy::CheckProcessResponse* response,
        ::google::protobuf::Closure* done) { 
    if (!request->has_process_id()) {
        response->set_status(
                kGuarderExecuteStateInputError); 
        done->Run();
        return;
    }

    MutexLock scoped_locked(&lock_);
    std::map<std::string,
        ProcessStatus>::iterator it =
            process_status_.find(request->process_id());
    if (it == process_status_.end()) {
        LOG(WARNING, "process id %s not int guarder mem", 
                request->process_id().c_str());
        response->set_status(
                kGuarderExecuteStateInternalError); 
        done->Run();
        return;
    }
    response->set_status(kGuarderExecuteStateOK);
    ProcessStatus& status = it->second;
    if (status.state() != kProcessStateRunning) {
        response->set_process_state(status.state());     
        response->set_process_exit_code(status.ret_code());
        done->Run();
        return;
    }
    int ret = ::kill(status.pid(), 0);
    if (ret != 0) {
        LOG(WARNING, "%s kill %d failed, err[%d: %s]",
                status.process_id().c_str(),
                status.pid(),
                errno, strerror(errno));
        if (errno == ESRCH) {
            response->set_process_state(kProcessStateError); 
            status.set_state(kProcessStateError);
        }
    } else {
        int status_val;
        pid_t pid = ::waitpid(status.pid(), 
                &status_val, WNOHANG); 
        if (pid == 0) {
            response->set_process_state(kProcessStateRunning); 
            status.set_state(kProcessStateRunning);
        } else if (pid == -1) {
            LOG(WARNING, "%s wait %d failed, err[%d: %s]",
                    status.process_id().c_str(),
                    status.pid(),
                    errno, strerror(errno));     
            response->set_process_state(kProcessStateError);
            status.set_state(kProcessStateError);
        } else {
            if (WIFEXITED(status_val)) {
                response->set_process_exit_code(
                        WEXITSTATUS(status_val));
                status.set_ret_code(WEXITSTATUS(status_val));
            } 
            response->set_process_state(kProcessStateError);
            status.set_state(kProcessStateError);
        }
    }
    done->Run();
    return;
} 

bool GuarderImpl::PrepareStdFds(const std::string& path, int* stdout_fd, int* stderr_fd) {
    const int STD_FILE_OPEN_FLAG = O_CREAT | O_APPEND | O_WRONLY;
    const int STD_FILE_OPEN_MODE = S_IRWXU | S_IRWXG | S_IROTH;
    if (stdout_fd == NULL 
            || stderr_fd == NULL) {
        return false; 
    }
    // construct file name
    char strftime_buffer[15];    
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    struct tm t;
    localtime_r(&(now_tv.tv_sec), &t);
    snprintf(strftime_buffer, 14, "%04d%02d%02d_%02d%02d%02d",
            t.tm_year,
            t.tm_mon + 1,
            t.tm_mday,
            t.tm_hour,
            t.tm_min,
            t.tm_sec);
    strftime_buffer[14] = '\0';
    std::string stdout_path = path + "/stdout_";
    stdout_path.append(strftime_buffer);
    std::string stderr_path = path + "/stderr_";
    stderr_path.append(strftime_buffer);

    *stdout_fd = ::open(stdout_path.c_str(), 
            STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    if (*stdout_fd == -1) {
        LOG(WARNING, "open stdout file failed err[%d: %s]",
                errno, strerror(errno)); 
        return false;
    }

    *stderr_fd = ::open(stderr_path.c_str(), 
            STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    if (*stderr_fd == -1) {
        LOG(WARNING, "open stderr file failed err[%d: %s]",
                errno, strerror(errno));
        ::close(*stdout_fd);
        return false;
    }
    return true;
}

bool GuarderImpl::DumpPersistenceInfo(
        ProcessStatusPersistence* info) {
    if (info == NULL) {
        return false;
    } 
    std::map<std::string, ProcessStatus>::iterator it = 
        process_status_.begin();
    for (; it != process_status_.end(); ++it) {
        ProcessStatus* temp_status = info->add_status();         
        temp_status->CopyFrom(it->second);
    }
    background_thread_->Stop(false);
    return true;
}

bool GuarderImpl::LoadPersistenceInfo(
        const ProcessStatusPersistence* info) {
    if (info == NULL) {
        return false; 
    }

    process_status_.clear();
    for (int i = 0; i < info->status_size(); i++) {
        ProcessStatus temp_status; 
        temp_status.CopyFrom(info->status(i));
        if (!temp_status.has_process_id()) {
            LOG(WARNING, "status has no process id");
            return false; 
        }
        process_status_[temp_status.process_id()] = 
            temp_status;
    }
    return true;
} 

}   // ending namespace galaxy


/* vim: set ts=4 sw=4 sts=4 tw=100 */
