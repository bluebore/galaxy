/* workspace.h
 * Copyright (C) 2015 wangtaize <wangtaize@baidu.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef AGENT_WORKSPACE_H
#define AGENT_WORKSPACE_H
#include <string>
#include "proto/task.pb.h"
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
    /**
     *获取一个已经准备好的路径
     * */
    virtual std::string GetPath() = 0 ;
    virtual ~Workspace(){}
};

class DefaultWorkspace:public Workspace{

public:
    DefaultWorkspace(const TaskInfo &_task_info,
                     std::string _root_path)
                     :m_task_info(_task_info),
                     m_root_path(_root_path),
                     m_has_created(false){
        m_mutex = new common::Mutex();
    }
    int Create();
    int Clean();
    bool IsExpire();
    std::string GetPath();
    ~DefaultWorkspace(){
        //清除工作目录下面的文件
        Clean();
        if(m_mutex != NULL){
            delete m_mutex;
        }
    }
private:
    ::galaxy::TaskInfo m_task_info;
    std::string m_root_path;
    std::string m_task_root_path;
    bool m_has_created;
    common::Mutex * m_mutex;
};

}
#endif /* !AGENT_WORKSPACE_H */
