# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com
# Date: 2015-04-06
import datetime
import logging
from sofa.pbrpc import client
from galaxy import master_pb2

STATE_MAP={0:'DEPLOYING',2:'RUNNING',3:'KILLED',4:'RESTART',5:'ERROR',6:'COMPLETE'}
SCHEDULE_STATE_MAP={0:'Scheduling',1:'NoResource',2:'NoFitAgent'}
LOG = logging.getLogger('console')
class BaseEntity(object):
    def __setattr__(self,name,value):
        self.__dict__[name] = value
    def __getattr__(self,name):
        return self.__dict__.get(name,None)

class GalaxySDK(object):
    """
    galaxy python sdk
    """
    def __init__(self, master_addr):
        self.channel = client.Channel(master_addr)

    def list_all_node(self):
        """
        list all node of galaxy master
        return:
              if error ,None will be return
        """
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(1.5)
        request = master_pb2.ListNodeRequest()
        try:
            response = master.ListNode(controller,request)
            if not response:
                LOG.error('fail to call list node')
                return []
            ret = []
            for node in response.nodes:
                base = BaseEntity()
                base.id = node.node_id
                base.node_id = node.node_id
                base.addr = node.addr
                base.task_num = node.task_num
                base.cpu_share = node.cpu_share
                base.mem_share = node.mem_share
                base.cpu_allocated = node.cpu_allocated
                base.mem_allocated = node.mem_allocated
                base.mem_used = node.mem_used
                base.cpu_used = node.cpu_used
                ret.append(base)
            return ret
        except:
            LOG.exception("fail to call list node")
        return []

    def make_job(self,name,pkg_type,
                      pkg_src,boot_cmd,
                      replicate_num = 1,
                      mem_limit = 1024,
                      cpu_limit = 2,
                      cpu_soft_limit = 2,
                      deploy_step_size = -1,
                      one_task_per_host = False,
                      restrict_tags = [],
                      conf = None):
        """
        send a new job command to galaxy master
        return:

        """
        assert name
        assert pkg_type
        assert pkg_src
        assert boot_cmd
        req = self._build_new_job_req(name,pkg_type,str(pkg_src),
                                      boot_cmd,
                                      replicate_num = replicate_num,
                                      mem_limit = mem_limit,
                                      cpu_limit = cpu_limit,
                                      cpu_soft_limit = cpu_soft_limit,
                                      deploy_step_size = deploy_step_size,
                                      one_task_per_host = one_task_per_host,
                                      restrict_tags = restrict_tags,
                                      conf = conf)
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(1.5)
        try:
            response = master.NewJob(controller,req)
            if not response:
                LOG.error("fail to create job")
                return False,None
            if response.status == 0:
                return True,response.job_id
            return False,response.job_id
        except:
            LOG.exception("fail to create  job")
        return False,None

    def tag_agent(self, tag, agent_set):
        entity = master_pb2.TagEntity(tag = tag,
                                      agents = agent_set)
        request = master_pb2.TagAgentRequest(tag_entity = entity)
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(1.5)
        try:
            response = master.TagAgent(controller, request)
            if response.status == 0 :
                return True
            return False
        except:
            LOG.exception("fail to tag agent")
            return False

    def list_tag(self):
        request = master_pb2.ListTagRequest()
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(1.5)
        try:
            response = master.ListTag(controller, request)
            ret = []
            for tag in response.tags:
                base = BaseEntity()
                base.tag = tag.tag
                base.agents = [agent for agent in tag.agents]
                ret.append(base.__dict__)
            return ret
        except Exception as e:
            LOG.exception("fail to list tag %s"%str(e))
            return []

    def list_all_job(self):

        request = master_pb2.ListJobRequest()
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(1.5)
        try:
            response =  master.ListJob(controller,request)
            if not response:
                return False,[]
            ret  = []
            for job in response.jobs:
                base = BaseEntity()
                base.job_id = job.job_id
                base.job_name = job.job_name
                base.running_task_num = job.running_task_num
                base.replica_num = job.replica_num
                base.job_raw = job.job_raw
                base.cmd_line = job.cmd_line
                base.cpu_share = job.cpu_share
                base.mem_share = job.mem_share
                base.deploy_step_size = job.deploy_step_size
                base.killed = job.killed
                base.cpu_limit = job.cpu_limit
                base.one_task_per_host = job.one_task_per_host
                base.version = job.version
                base.is_updating = job.is_updating
                base.update_step_size = job.update_step_size
                trace = BaseEntity()
                trace.killed_count = job.trace.killed_count
                trace.overflow_killed_count = job.trace.overflow_killed_count
                trace.start_count = job.trace.start_count
                trace.deploy_failed_count = job.trace.deploy_failed_count
                trace.reschedule_count = job.trace.reschedule_count
                trace.deploy_start_time = job.trace.deploy_start_time
                trace.deploy_end_time = job.trace.deploy_end_time
                trace.state = SCHEDULE_STATE_MAP[job.trace.state]
                base.trace = trace
                ret.append(base)
            return True,ret
        except:
            LOG.exception('fail to list jobs')
        return False,[]

    def update_job(self, id, 
                        replicate_num, 
                        package,
                        deploy_step_size,
                        update_step_size,
                        is_updating):
        req = master_pb2.UpdateJobRequest()
        req.job_id = id
        req.replica_num = replicate_num
        req.job_raw = package 
        req.is_updating = is_updating
        req.deploy_step_size = deploy_step_size
        req.update_step_size = update_step_size
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(1.5)
        try:
            response = master.UpdateJob(controller,req)
            if not response or response.status != 0 :
                return False
            return True
        except:
            LOG.exception('fail to update job')
        return False

    def list_task_by_host(self,host):
        req = master_pb2.ListTaskRequest()
        req.agent_addr = host
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(1.5)
        try:
            response = master.ListTask(controller,req)
            if not response:
                LOG.error('fail to list task %s'%job_id)
                return False,[]
            ret = []
            for task in response.tasks:
                base = BaseEntity()
                base.id = task.info.task_id
                base.status = STATE_MAP[task.status]
                base.name = task.info.task_name
                base.agent_addr = task.agent_addr
                base.job_id = task.job_id
                base.offset = task.offset
                base.mem_limit = task.info.required_mem
                base.cpu_limit = task.info.required_cpu
                base.mem_used = task.memory_usage
                base.cpu_used = task.cpu_usage
                base.start_time = task.start_time
                ret.append(base)
            return True,ret
        except:
            LOG.exception('fail to list task')
        return False,[]

    def list_task_by_job_id(self,job_id):
        req = master_pb2.ListTaskRequest()
        req.job_id = job_id
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(3.5)
        try:
            response = master.ListTask(controller,req)
            if not response:
                LOG.error('fail to list task %s'%job_id)
                return False,[]
            ret = []
            for task in response.tasks:
                base = BaseEntity()
                base.id = task.info.task_id
                base.status = STATE_MAP[task.status]
                base.name = task.info.task_name
                base.agent_addr = task.agent_addr
                base.job_id = task.job_id
                base.offset = task.offset
                base.mem_limit = task.info.required_mem
                base.cpu_limit = task.info.required_cpu
                base.mem_used = task.memory_usage
                base.cpu_used = task.cpu_usage
                base.start_time = task.start_time
                ret.append(base)
            return True,ret
        except:
            LOG.exception('fail to list task')
        return False,[]

    def get_scheduled_history(self,job_id):
        req = master_pb2.ListTaskRequest()
        req.job_id = job_id
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(3.5)
        try:
            response = master.ListTask(controller,req)
            if not response:
                LOG.error('fail to list task %s'%job_id)
                return False,[]
            ret = []
            for task in response.scheduled_tasks:
                base = BaseEntity()
                base.id = task.info.task_id
                base.status = STATE_MAP[task.status]
                base.name = task.info.task_name
                base.agent_addr = task.agent_addr
                base.job_id = task.job_id
                base.offset = task.offset
                base.mem_limit = task.info.required_mem
                base.cpu_limit = task.info.required_cpu
                base.mem_used = task.memory_usage
                base.cpu_used = task.cpu_usage
                base.start_time = task.start_time
                base.gc_path = task.root_path
                base.end_time =  datetime.datetime.fromtimestamp(task.end_time).strftime("%m-%d %H:%M:%S") 
                ret.append(base)
            return True,ret
        except:
            LOG.exception('fail to list task history')
        return False,[]

    def kill_job(self,job_id):
        req = master_pb2.KillJobRequest()
        req.job_id = job_id
        master = master_pb2.Master_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(1.5)
        try:
            master.KillJob(controller,req)
        except:
            LOG.exception('fail to kill job')

    def _build_new_job_req(self,name,pkg_type,
                               pkg_src,boot_cmd,
                               replicate_num = 1,
                               mem_limit= 1024,
                               cpu_limit= 2,
                               cpu_soft_limit = 2,
                               deploy_step_size=-1,
                               one_task_per_host=False,
                               restrict_tags = [],
                               conf = None):

        req = master_pb2.NewJobRequest(restrict_tags = set(restrict_tags))
        if  deploy_step_size > 0:
            req.deploy_step_size = deploy_step_size
        req.job_name = name
        req.job_raw = pkg_src
        req.cmd_line = boot_cmd
        req.cpu_share = cpu_soft_limit
        req.mem_share = mem_limit
        req.replica_num = replicate_num
        req.cpu_limit = cpu_limit
        req.one_task_per_host = one_task_per_host
        if conf:
            req.monitor_conf = conf.SerializeToString()
        return req

