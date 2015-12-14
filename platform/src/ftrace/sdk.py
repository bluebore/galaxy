# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import logging
from sofa.pbrpc import client
from ftrace import query_pb2
LOG = logging.getLogger('console')

class FtraceSDK(object):
    def __init__(self, addr):
        self.channel = client.Channel(addr)

    def make_req(self, req):
        ftrace = query_pb2.SearchEngineService_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(10)
        LOG.info(req)
        response = ftrace.Search(controller, req)
        return response.result_list, True

    def simple_query(self, db, 
                          table,
                          id,
                          time_from, 
                          time_to,
                          limit = 100):
        if not db or not table or not id:
            return [], False
        LOG.info("db %s table %s id %s from %d to %d limit %d"%(db, table, id ,time_from, time_to ,limit))
        ftrace = query_pb2.SearchEngineService_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(5)
        request = query_pb2.RpcSearchRequest()
        request.db_name = db
        request.table_name = table
        request.primary_key = id
        request.start_timestamp = time_from
        request.end_timestamp = time_to
        request.limit = limit
        LOG.info(request)
        response = ftrace.Search(controller, request)
        return response.result_list, True

    def index_query(self, db, table, 
                          index_name, 
                          index_value, 
                          time_from,
                          time_to,
                          limit = 100):

        LOG.info("db %s table %s index_name %s index_value %s from %d to %d limit %d"%(db, table, index_name,
            index_value, time_from, time_to ,limit))
        if not db or not table or not index_name:
            return [], False
        ftrace = query_pb2.SearchEngineService_Stub(self.channel)
        controller = client.Controller()
        controller.SetTimeout(5)
        condition = query_pb2.RpcIndexCondition(cmp_key = index_value,
                                               cmp = query_pb2.RpcEqualTo,
                                               index_table_name = index_name)
        request = query_pb2.RpcSearchRequest(condition = [condition])
        request.db_name = db
        request.table_name = table
        request.start_timestamp = time_from
        request.end_timestamp = time_to
        request.limit = limit
        response = ftrace.Search(controller, request)
        return response.result_list, True
