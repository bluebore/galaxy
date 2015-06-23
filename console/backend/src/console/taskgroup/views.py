# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com
# Date: 2015-03-30
import logging
from common import http
from common import decorator as D
from galaxy import wrapper
from bootstrap import settings
from console.taskgroup import helper
from console.service import decorator as s_decorator
from console.taskgroup import decorator as t_decorator
from django.db import transaction
from django.views.decorators.csrf import csrf_exempt
LOG = logging.getLogger("console")
# service group 0

SHOW_G_BYTES_LIMIT = 1024 * 1024 * 1024

def str_pretty(total_bytes):
    if total_bytes < SHOW_G_BYTES_LIMIT:
        return "%sM"%(total_bytes/(1024*1024))
    return "%sG"%(total_bytes/(1024*1024*1024))


@D.api_auth_required
@t_decorator.task_group_id_required
def update_task_group(request):
    pass



@D.api_auth_required
def get_task_status(request):
    builder = http.ResponseBuilder()
    id = request.GET.get('id',None)
    agent = request.GET.get('agent',None)
    master_addr = request.GET.get('master',None)
    if not master_addr:
        return builder.error('master is required').build_json()
    galaxy = wrapper.Galaxy(master_addr,settings.GALAXY_CLIENT_BIN)
    tasklist = []
    if id :
        status,tasklist = galaxy.list_task_by_job_id(id)
        if not status:
            return builder.error("fail to get task list")\
                          .build_json()
    if agent:
        status,tasklist = galaxy.list_task_by_host(agent)
        if not status:
            return builder.error("fail to get task list")\
                          .build_json()
    statics = {"RUNNING":0,"DEPLOYING":0,"ERROR":0}
    for task in tasklist:
        task['mem_used'] = str_pretty(task['mem_used'])
        task['mem_limit'] = str_pretty(task['mem_limit'])
        task['cpu_used'] ="%0.2f"%( task['cpu_used'])
        if task['status'] in statics:
            statics[task['status']] += 1
        if task['agent_addr']:
            task['path'] = "http://"+task['agent_addr'].split(":")[0]+":8181/data/galaxy/"+str(task['id'])
    return builder.ok(data={'needInit':False,'taskList':tasklist,'statics':statics}).build_json()

@D.api_auth_required
def get_job_sched_history(request):
    builder = http.ResponseBuilder()
    id = request.GET.get('id',None)
    master_addr = request.GET.get('master',None)
    if not master_addr:
        return builder.error('master is required').build_json()
    galaxy = wrapper.Galaxy(master_addr,settings.GALAXY_CLIENT_BIN)
    if not id :
        return builder.error("id is required").build_json()

    status,tasklist = galaxy.job_history(id)
    for task in tasklist:
        task['mem_used'] = str_pretty(task['mem_used'])
        task['mem_limit'] = str_pretty(task['mem_limit'])
        task['cpu_used'] ="%0.2f"% (task['cpu_used'])
        if task['agent_addr']:
            task['path'] = "http://"+task['agent_addr'].split(":")[0]+":8181/gc/"+ task['gc_path'].replace("/home/galaxy/agent/work_dir/gc/","")
    return builder.ok(data={'needInit':False,'taskList':tasklist}).build_json()
