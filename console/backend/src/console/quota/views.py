# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com

from bootstrap import settings
from bootstrap import models
from common import http
from common import decorator as D
from galaxy import wrapper
SHOW_G_BYTES_LIMIT = 1024 * 1024 * 1024
SHOW_T_BYTES_LIMIT = 1024 * 1024 * 1024 * 1024

def str_pretty(total_bytes):
    if total_bytes < SHOW_G_BYTES_LIMIT:
        return "%sM"%(total_bytes/(1024*1024))
    elif total_bytes < SHOW_T_BYTES_LIMIT:
        return "%sG"%(total_bytes/(SHOW_G_BYTES_LIMIT))
    else:
        return "%sT"%(total_bytes/(SHOW_T_BYTES_LIMIT))


def get_group_quota_stat(group, master_job_cache):
    db_jobs = models.GalaxyJob.objects.filter(group__id = group.id)
    all_jobs = None
    client = wrapper.Galaxy(group.galaxy_master,
                            settings.GALAXY_CLIENT_BIN)
    if group.galaxy_master in master_job_cache:
        all_jobs = master_job_cache[group.galaxy_master]
    else:
        status,jobs = client.list_jobs()
        if not status:
            return False,{}
        master_job_cache[group.galaxy_master] = jobs
        all_jobs = jobs
    db_job_dict = {}
    for db_job in db_jobs:
        db_job_dict[db_job.job_id] = db_job
    stat = {}
    stat['total_cpu_quota'] = group.quota.cpu_total_limit
    stat['total_mem_quota'] = group.quota.mem_total_limit
    stat['total_cpu_allocated'] = 0.0
    stat['total_cpu_real_used'] = 0.0
    stat['total_mem_allocated'] = 0
    stat['total_mem_real_used'] = 0
    stat['job_list'] = []
    stat['task_count'] = 0
    for job in all_jobs:
        if job.job_id not in db_job_dict:
            continue
        job.trace = job.trace.__dict__
        stat['job_list'].append(job.__dict__)
        status,tasklist = client.list_task_by_job_id(job.job_id)
        if not status:
            return False,{}
        for task in tasklist:
            stat['total_cpu_allocated'] += task['cpu_limit']
            stat['total_cpu_real_used'] += task['cpu_used']
            stat['total_mem_allocated'] += task['mem_limit']
            stat['total_mem_real_used'] += task['mem_used']
            stat['task_count'] += 1
    stat['pretty_total_mem_quota'] = str_pretty(stat['total_mem_quota'])
    stat['pretty_total_mem_allocated'] = str_pretty(stat['total_mem_allocated'])
    stat['pretty_total_mem_real_used'] = str_pretty(stat['total_mem_real_used'])
    stat['total_mem_left'] = stat['total_mem_quota'] - stat['total_mem_allocated']
    stat['pretty_total_mem_left'] = str_pretty(stat['total_mem_left'])
    stat['total_cpu_left'] = stat['total_cpu_quota'] - stat['total_cpu_allocated']
    stat['mem_used_percent'] = 0
    if stat['total_mem_allocated']:
        stat['mem_used_percent'] = "%0.1f"%(100 *stat['total_mem_real_used']/stat['total_mem_allocated'])
    stat['cpu_used_percent'] = 0
    if stat['total_cpu_allocated']:
        stat['cpu_used_percent'] = "%0.1f"%(100 * stat['total_cpu_real_used'] / stat['total_cpu_allocated'])
    stat['pretty_total_cpu_allocated'] = "%0.1f"%stat['total_cpu_allocated']
    stat['pretty_total_cpu_left'] = "%0.1f"%stat['total_cpu_left']
    return True,stat

@D.api_auth_required
def my_quota(req):
    builder = http.ResponseBuilder()
    groupmember_list = models.GroupMember.objects.filter(user_name = req.user.username)
    master_job_cache = {}
    result = []
    for gm in groupmember_list:
        _,stat = get_group_quota_stat(gm.group, master_job_cache)
        result.append({'group':{'name':gm.group.name,'master_addr':gm.group.galaxy_master},'stat':stat})
    return builder.ok(data=result).build_json()


@D.api_auth_required
def my_group(req):
    builder = http.ResponseBuilder()
    groupmember_list = models.GroupMember.objects.all()
    result = []
    for gm in groupmember_list:
        result.append({'name':gm.group.name,'id':gm.group.id})
    return builder.ok(data=result).build_json()

@D.api_auth_required
def group_stat(req):
    builder = http.ResponseBuilder()
    group_id = req.GET.get('id',None)
    if not group_id:
        return builder.error('id is required').build_json()
    try:
        groupmember_list = models.GroupMember.objects.filter(user_name = req.user.username , group__id = int(group_id))
        for gm in groupmember_list:
            _,stat = get_group_quota_stat(gm.group, {})
            return builder.ok(data={'group':{'name':gm.group.name},'stat':stat}).build_json()
        return builder.error('group %s does not exist'%group_id).build_json()
    except:
        LOG.exception('fail to get group member user %s, group_id %s'%(req.user.username,group_id))
        return builder.error('fail to get group member').build_json()

@D.api_auth_required
def get_status(req):
    builder = http.ResponseBuilder()
    master_addr = req.GET.get('master',None)
    if not master_addr:
        return builder.error('master is required').build_json()

    client = wrapper.Galaxy(master_addr,settings.GALAXY_CLIENT_BIN)
    machine_list = client.list_node()
    ret = []
    total_node_num = 0
    total_cpu_num = 0
    total_cpu_allocated = 0
    total_cpu_used = 0
    total_mem_used = 0
    total_mem_num = 0
    total_mem_allocated = 0
    total_task_num = 0
    total_node_num = len(machine_list)
    for machine in machine_list:
        total_cpu_num += machine.cpu_share
        total_mem_num += machine.mem_share
        total_task_num += machine.task_num
        total_cpu_allocated += machine.cpu_allocated
        total_mem_allocated += machine.mem_allocated
        total_mem_used += machine.mem_used
        total_cpu_used += machine.cpu_used 
        machine.mem_share = str_pretty(machine.mem_share)
        machine.mem_allocated = str_pretty(machine.mem_allocated)
        machine.cpu_used = '%0.0f'%machine.cpu_allocated
        ret.append(machine.__dict__)
    mem_usage_p = 0
    cpu_usage_p = 0
    if total_mem_num:
        mem_usage_p = "%0.1f"%(100*total_mem_used/total_mem_num)
    if total_cpu_num:
        cpu_usage_p = "%0.1f"%(100*total_cpu_used/total_cpu_num)
    return builder.ok(data={'machinelist':ret,
                                'total_node_num':total_node_num,
                                'total_mem_allocated':str_pretty(total_mem_allocated),
                                'total_cpu_allocated':"%0.0f"%total_cpu_allocated,
                                'total_cpu_num':total_cpu_num,
                                'total_mem_num':str_pretty(total_mem_num),
                                'total_task_num':total_task_num,
                                'total_mem_used':str_pretty(total_mem_used),
                                'total_cpu_used':"%0.0f"%total_cpu_used,
                                'mem_usage_p':mem_usage_p,
                                'cpu_usage_p':cpu_usage_p}).build_json()


