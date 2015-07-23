# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com
# Date: 2015-03-30
import logging
from common import shell
from galaxy import sdk
LOG = logging.getLogger("console")

class Galaxy(object):
    def __init__(self, master_addr,bin_path):
        self.master_addr = master_addr
        self.shell_helper = shell.ShellHelper()
        self.bin_path = bin_path
    def create_task(self, name, 
                          url,
                          cmd_line,
                          replicate_count,
                          mem_limit,
                          cpu_soft_limit,
                          cpu_limit,
                          deploy_step_size=-1, 
                          one_task_per_host=False, 
                          restrict_tags = [],
                          conf = None):

        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        status,job_id = galaxy_sdk.make_job(name,'ftp',url,cmd_line,
                                           replicate_num = replicate_count,
                                           mem_limit = mem_limit,
                                           cpu_limit = cpu_limit,
                                           cpu_soft_limit = cpu_soft_limit,
                                           deploy_step_size=deploy_step_size,
                                           one_task_per_host=one_task_per_host,
                                           restrict_tags = restrict_tags,
                                           conf = conf)
        return status,job_id

    def list_task_by_job_id(self,job_id):
        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        status ,task_list = galaxy_sdk.list_task_by_job_id(int(job_id))
        if not status:
            return False,[]

        ret_task_list = []
        for task in task_list:
           ret_task_list.append(task.__dict__)
        return True, ret_task_list

    def list_task_by_host(self,agent):
        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        status ,task_list = galaxy_sdk.list_task_by_host(str(agent))
        if not status:
            return False,[]

        ret_task_list = []
        for task in task_list:
           ret_task_list.append(task.__dict__)
        return True, ret_task_list

    def job_history(self, job_id):
        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        status ,task_list = galaxy_sdk.get_scheduled_history(int(job_id))
        if not status:
            return False,[]
        ret_task_list = []
        for task in task_list:
           ret_task_list.append(task.__dict__)
        return True, ret_task_list

    def list_node(self):
        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        node_list = galaxy_sdk.list_all_node()
        return node_list

    def kill_job(self,job_id):

        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        galaxy_sdk.kill_job(job_id)

    def update_job(self,job_id, replicate_num,
                       package,
                       deploy_step_size,
                       update_step_size,
                       is_updating):

        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        return galaxy_sdk.update_job(job_id,replicate_num,
                                     str(package),
                                     deploy_step_size,
                                     update_step_size,
                                     is_updating)

    def list_jobs(self):
        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        status,job_list = galaxy_sdk.list_all_job()
        return status ,job_list

    def tag_agent(self, tag, agent_set):
        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        status = galaxy_sdk.tag_agent(tag, agent_set)
        return status

    def list_tag(self):
        galaxy_sdk = sdk.GalaxySDK(self.master_addr)
        return galaxy_sdk.list_tag()
