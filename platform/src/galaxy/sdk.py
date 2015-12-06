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
from galaxy import galaxy_pb2
from common import util
LOG = logging.getLogger('console')
class BaseEntity(object):
    def __setattr__(self,name,value):
        self.__dict__[name] = value
    def __getattr__(self,name):
        return self.__dict__.get(name,None)

class GalaxySDK(object):
    """
    Lumia python sdk
    """
    def __init__(self, master_addr):
        self.channel = client.Channel(master_addr)

    def get_pods(self, jobid):
        """
        """
        controller = client.Controller()
        controller.SetTimeout(5)
        master = master_pb2.Master_Stub(self.channel)
        request = master_pb2.ShowPodRequest()
        request.jobid = jobid
        response = master.ShowPod(controller, request)
        if response.status != galaxy_pb2.kOk:
            LOG.error("fail get pods");
            return [], False
        pods = []
        for pod in response.pods:
            new_pod = util.pb2dict(pod) 
            new_pod["stage"] = galaxy_pb2.PodStage.Name(pod.stage)
            new_pod["state"] = galaxy_pb2.PodState.Name(pod.state)
            pods.append(new_pod)
        return pods, True

    def get_all_job(self):
        controller = client.Controller()
        controller.SetTimeout(5)
        master = master_pb2.Master_Stub(self.channel)
        request = master_pb2.ListJobsRequest()
        response = master.ListJobs(controller, request)
        return response.jobs, True

    def get_real_time_status(self):
        controller = client.Controller()
        controller.SetTimeout(5)
        master = master_pb2.Master_Stub(self.channel)
        request = master_pb2.GetMasterStatusRequest()
        response = master.GetStatus(controller, request)
        return response
