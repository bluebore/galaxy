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
from lumia import lumia_pb2

LOG = logging.getLogger('console')
class BaseEntity(object):
    def __setattr__(self,name,value):
        self.__dict__[name] = value
    def __getattr__(self,name):
        return self.__dict__.get(name,None)

class LumiaSDK(object):
    """
    Lumia python sdk
    """
    def __init__(self, lumia_ctrl_addr):
        self.channel = client.Channel(lumia_ctrl_addr)

    def get_overview(self):
        """
        """
        lumia_ctrl = lumia_pb2.LumiaCtrl_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(2.5)
        request = lumia_pb2.GetOverviewRequest()
        try:
            response = lumia_ctrl.GetOverview(controller,request)
            return response.minions
        except:
            LOG.exception("fail to call list node")
        return []

    def show_minion(self, ips = []):
        if not ips:
            return []
        lumia_ctrl = lumia_pb2.LumiaCtrl_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(2.5)
        request = lumia_pb2.GetMinionRequest(ips = ips)
        try:
            response = lumia_ctrl.GetMinion(controller, request)
            return response.minions
        except:
            LOG.exception("fail to call list node")
        return []

    def get_status(self):
        lumia_ctrl = lumia_pb2.LumiaCtrl_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(2.5)
        request = lumia_pb2.GetStatusRequest()
        try:
            response = lumia_ctrl.GetStatus(controller, request)
            return response.live_nodes, response.dead_nodes
        except:
            LOG.exception("fail to call get status")
        return [], []

    def report(self, ip):
        lumia_ctrl = lumia_pb2.LumiaCtrl_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(2.5)
        request = lumia_pb2.ReportDeadMinionRequest()
        request.ip = ip
        try:
            response = lumia_ctrl.ReportDeadMinion(controller, request)
            return response.status
        except:
            LOG.exception("fail to call get status")
        return [], []


