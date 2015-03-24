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
//work space ���� task info��Ԫ�������ɺ�һ��
//�����е�workspace
class Workspace{

public:
    /**
     *����һ����������Ŀ¼
     * */
    virtual int Create() = 0;
    /**
     *���workspace
     * */
    virtual int Clean() = 0;
    /**
     *����Ŀ¼�Ƿ��Ѿ����ڣ�����һ������ֹͣ��೤ʱ���������Ŀ¼������
     * */
    virtual bool IsExpire() = 0 ;
    /**
     *��ȡһ���Ѿ�׼���õ�·��
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
        //�������Ŀ¼������ļ�
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
