# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com
# Date: 2015-04-01

from bootstrap import settings
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
        machine.cpu_used = '%0.1f'%machine.cpu_allocated
        machine.cpu_allocated = '%0.1f'%machine.cpu_allocated
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


