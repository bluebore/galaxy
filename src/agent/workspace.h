// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com

#ifndef AGENT_WORKSPACE_H
#define AGENT_WORKSPACE_H
#include <string>
#include "proto/task.pb.h"
#include "proto/agent.pb.h"
#include "common/mutex.h"

namespace galaxy{

class Fetcher{

public:
    virtual int Fetch() = 0;
};
//work space 根据 task info中元数据生成好一个
//可运行的workspace
class Workspace{

public:
    /**
     *创建一个任务运行目录
     * */
    virtual int Create() = 0;
    /**
     *清除workspace
     * */
    virtual int Clean() = 0;
    /**
     *工作目录是否已经过期，会有一个任务停止后多长时间清除工作目录的需求
     * */
    virtual bool IsExpire() = 0 ;

    // change dir to a new dir
    virtual int MoveTo(const std::string& new_dir) = 0;
    /**
     *获取一个已经准备好的路径
     * */
    virtual std::string GetPath() = 0 ;
    virtual TaskInfo GetTaskInfo() = 0;
    virtual ~Workspace(){}
    virtual bool LoadPersistenceInfo(const WorkspacePersistence& info) = 0;
    virtual bool DumpPersistenceInfo(WorkspacePersistence* info) = 0;
};

class DefaultWorkspace:public Workspace{

public:
    DefaultWorkspace(const TaskInfo &_task_info,
                     const std::string& _root_path)
                     :m_task_info(_task_info),
                     m_root_path(_root_path),
                     m_has_created(false){
    }
    int Create();
    int Clean();
    bool IsExpire();
    std::string GetPath();
    std::string GetRoot();
    virtual int MoveTo(const std::string& new_dir);
    ~DefaultWorkspace(){
    }
    TaskInfo GetTaskInfo();

    bool LoadPersistenceInfo(const WorkspacePersistence& info);
    bool DumpPersistenceInfo(WorkspacePersistence* info);
private:
    void DeployInThread();
    int MakePath(const char *path, mode_t mode);

    TaskInfo m_task_info;
    std::string m_root_path;
    std::string m_task_root_path;
    bool m_has_created;
};

}
#endif /* !AGENT_WORKSPACE_H */
