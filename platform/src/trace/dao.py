# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import datetime
from ftrace import sdk
from galaxy import log_pb2
from galaxy import master_pb2
from galaxy import galaxy_pb2
from galaxy import agent_pb2
from common import pb2dict
from common import util
import logging
logger = logging.getLogger("console")

class TraceDao(object):

    def __init__(self, trace_se):
        self.ftrace = sdk.FtraceSDK(trace_se)

    def get_job_event(self, jobid, 
                      start_time, 
                      end_time,
                      limit=100):
        data_list, status = self.ftrace.simple_query("baidu.galaxy", 
                                                 "JobEvent",
                                                 jobid,
                                                 int(start_time * 1000000),
                                                 int(end_time * 1000000),
                                                 limit = limit)
        if not status:
            logger.error("fail to query job stat")
            return [], False
        events = []
        job_event = log_pb2.JobEvent()
        job_desc = master_pb2.JobDescriptor()
        for data in data_list:
            for d in data.data_list:
                job_event.ParseFromString(d)
                job_desc.ParseFromString(job_event.desc)
                data = pb2dict.protobuf_to_dict(job_event)
                data["desc"] = pb2dict.protobuf_to_dict(job_desc)
                data["state"] = master_pb2.JobState.Name(data["state"])
                data["level"] = log_pb2.TraceLevel.Name(job_event.level)
                data["update_state"] = master_pb2.JobUpdateState.Name(data["update_state"])
                data["time"] =  datetime.datetime.fromtimestamp(data["time"]/1000000).strftime("%Y-%m-%d %H:%M:%S") 
                events.append(data)
        logger.info("query job %s event from %s to %s , count %d"%(jobid,
                                   datetime.datetime.fromtimestamp(start_time).strftime("%Y-%m-%d %H:%M:%S"),
                                   datetime.datetime.fromtimestamp(end_time).strftime("%Y-%m-%d %H:%M:%S"),
                                   len(events)))
        return events, True

    def get_job_stat(self, jobid, start_time, end_time, limit = 100):
        data_list, status = self.ftrace.simple_query("baidu.galaxy", 
                                                 "JobStat",
                                                 jobid,
                                                 int(start_time * 1000000),
                                                 int(end_time * 1000000),
                                                 limit = limit)
        if not status:
            logger.error("fail to query job stat")
            return [], False
        stats = []
        job_stat = log_pb2.JobStat()
        for data in data_list:
            for d in data.data_list:
                job_stat.ParseFromString(d)
                job_stat.time = job_stat.time/1000
                stats.append(util.pb2dict(job_stat))
        logger.info("query job %s stat from %s to %s , count %d"%(jobid,
                                   datetime.datetime.fromtimestamp(start_time).strftime("%Y-%m-%d %H:%M:%S"),
                                   datetime.datetime.fromtimestamp(end_time).strftime("%Y-%m-%d %H:%M:%S"),
                                   len(stats)))
        return stats, True

    def get_pod_stat(self, podid, start_time, end_time, limit = 100):
        data_list, status = self.ftrace.simple_query("baidu.galaxy", 
                                                 "PodStat",
                                                 podid,
                                                 int(start_time * 1000000),
                                                 int(end_time * 1000000),
                                                 limit = limit)
        if not status:
            logger.error("fail to query job stat")
            return [], False
        stats = []
        pod_stat = log_pb2.PodStat()
        for data in data_list:
            for d in data.data_list:
                pod_stat.ParseFromString(d)
                pod_stat.time = pod_stat.time/1000
                stats.append(util.pb2dict(pod_stat))

        return stats, True


    def get_pod_event(self, podid, start_time, end_time, limit = 100):
        data_list, status = self.ftrace.simple_query("baidu.galaxy", 
                                                 "PodEvent",
                                                 podid,
                                                 int(start_time * 1000000),
                                                 int(end_time * 1000000),
                                                 limit = limit)
        if not status:
            logger.error("fail to query pod event")
            return [], False
        events = []
        pod_event = log_pb2.PodEvent()
        for data in data_list:
            for d in data.data_list:
                pod_event.ParseFromString(d)
                e = util.pb2dict(pod_event)
                e["stage"] = galaxy_pb2.PodStage.Name(pod_event.stage)
                e["level"] = log_pb2.TraceLevel.Name(pod_event.level)
                e["state"] = galaxy_pb2.PodState.Name(pod_event.state)
                events.append(e)
        return events, True

    def get_task_event(self, podid, start_time, end_time, limit=100):
        data_list, status = self.ftrace.index_query("baidu.galaxy", 
                                                    "TaskEvent",
                                                    "pod_id", 
                                                    podid,
                                                    int(start_time * 1000000),
                                                    int(end_time * 1000000),
                                                    limit = limit)
        events = []
        task_event = log_pb2.TaskEvent()
        for data in data_list:
            for d in data.data_list:
                task_event.ParseFromString(d)
                e = util.pb2dict(task_event)
                e["id"] = e["id"][-8:]
                e["initd_port"] = e["initd_addr"].split(":")[-1]
                e["stage"] = agent_pb2.TaskStage.Name(task_event.stage)
                e["level"] = log_pb2.TraceLevel.Name(task_event.level)
                e["state"] = galaxy_pb2.TaskState.Name(task_event.state)
                events.append(e)
        return events, True

    def get_pod_event_by_jobid(self, jobid, start_time, end_time, limit=1000):
        data_list, status = self.ftrace.index_query("baidu.galaxy", 
                                                    "PodEvent",
                                                    "jobid", 
                                                    jobid,
                                                    int(start_time * 1000000),
                                                    int(end_time * 1000000),
                                                    limit = limit)
        events = []
        pod_event = log_pb2.PodEvent()
        for data in data_list:
            for d in data.data_list:
                pod_event.ParseFromString(d)
                e = util.pb2dict(pod_event)
                e["stage"] = galaxy_pb2.PodStage.Name(pod_event.stage)
                e["level"] = log_pb2.TraceLevel.Name(pod_event.level)
                e["state"] = galaxy_pb2.PodState.Name(pod_event.state)
                events.append(e)
        return events, True


