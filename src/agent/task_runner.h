// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#ifndef AGENT_TASK_RUNNER_H
#define AGENT_TASK_RUNNER_H
#include <map>
#include <boost/function.hpp>
#include "proto/task.pb.h"
#include "common/mutex.h"
#include "agent/workspace.h"
#include "agent/resource_collector.h"
namespace galaxy{

class TaskRunner{

public:

   virtual void AsyncDownload(boost::function<void()>) = 0;

   virtual int Prepare() = 0 ;
   /**
    * start task
    * */
   virtual int Start() = 0 ;

   virtual int StartMonitor() = 0;

   /**
    * restart
    * */
   virtual int ReStart() = 0 ;
   /**
    *stop task
    * */
   virtual int Stop() = 0 ;

   /**
    * check task
    * */
   virtual int IsRunning() = 0 ;

   virtual void PersistenceAble(const std::string& persistence_path) = 0;

   virtual void Status(TaskStatus* status) = 0;
   virtual void Killed() = 0;
   virtual ~TaskRunner(){}
   virtual int Clean() { return 0;}
   virtual bool LoadPersistenceInfo(
                const ::galaxy::TaskPersistence& info) = 0;
   virtual bool DumpPersistenceInfo(
                ::galaxy::TaskPersistence* info) = 0;
};

class AbstractTaskRunner:public TaskRunner{

public:
    AbstractTaskRunner(TaskInfo task_info,
                       DefaultWorkspace * workspace)
                       :m_task_info(task_info),
                       m_child_pid(-1),
                       m_group_pid(-1),
                       m_monitor_pid(-1),
                       m_monitor_gid(-1),
                       m_workspace(workspace),
                       m_has_retry_times(0),
                       m_task_state(DEPLOYING),
                       downloader_id_(-1) {}
    virtual int Prepare() = 0;
    virtual int Start() = 0;
    virtual int StartMonitor() = 0;
    int IsRunning();
    int IsProcessRunning(pid_t pid);
    virtual int Stop();
    int ReStart();
    void AsyncDownload(boost::function<void()> callback);
    // do something after stop
    virtual void StopPost() = 0;
    virtual void Status(TaskStatus* status) = 0;

    void PersistenceAble(const std::string& persistence_path) {
        persistence_path_dir_ = persistence_path; 
    }

    void Killed() {
        // Mark kill 
        SetStatus(KILLED); 
    }

    void SetWorkspace(DefaultWorkspace* workspace) {
        m_workspace = workspace;
    }
    virtual bool LoadPersistenceInfo(
                const ::galaxy::TaskPersistence& info);
    virtual bool DumpPersistenceInfo(
                ::galaxy::TaskPersistence* info);
protected:
    void SetStatus(int status);
    void StartAfterDownload(
            boost::function<void()> callback, 
            int ret);
    void PrepareStart(std::vector<int>& fd_vector,int* stdout_fd,int* stderr_fd);
    void PrepareStartMonitor(std::vector<int>& fd_vector,int* stdout_fd,int* stderr_fd);
    void StartTaskAfterFork(std::vector<int>& fd_vector,int stdout_fd,int stderr_fd);
    void StartMonitorAfterFork(std::vector<int>& fd_vector,int stdout_fd,int stderr_fd);

protected:
    TaskInfo m_task_info;
    //task parent pid
    pid_t  m_child_pid;
    pid_t  m_group_pid;
    pid_t  m_monitor_pid;
    pid_t  m_monitor_gid;
    DefaultWorkspace * m_workspace;
    int m_has_retry_times;
    int m_task_state;
    int downloader_id_;
    std::string persistence_path_dir_;
    std::map<std::string, std::string> envs_;
};

class CommandTaskRunner:public AbstractTaskRunner{

public:
    CommandTaskRunner(TaskInfo _task_info,
                      DefaultWorkspace * _workspace)
                      :AbstractTaskRunner(_task_info,_workspace),
                       collector_(NULL),
                       collector_id_(-1),
                       sequence_id_(0) {
    }

    virtual ~CommandTaskRunner();
    virtual int Prepare();
    int Start();
    int StartMonitor();
    virtual void Status(TaskStatus* status);
    virtual void StopPost();

    static bool RecoverRunner(
            const std::string& persistence_path);

    static bool RecoverMonitor(
            const std::string& persistence_path);
protected:

    ProcResourceCollector* collector_;
    long collector_id_;
    int64_t sequence_id_;
};



}//galaxy

#endif /* !AGENT_TASK_RUNNER_H */
