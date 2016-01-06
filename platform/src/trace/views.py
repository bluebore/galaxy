# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import time
import urllib
import json
from common import util
from common import pb2dict
from bootstrap import settings
from trace import dao
from common import http
from galaxy import sdk
from ftrace import sdk as fsdk
from galaxy import galaxy_pb2
from galaxy import agent_pb2
from galaxy import log_pb2
from galaxy import initd_pb2
from galaxy import master_pb2
from sql import sql_parser
from ftrace import query_pb2
import logging
logger = logging.getLogger("console")

def data_center_decorator(func):
    def data_center_decorator_wrapper(request, *args, **kwds):
        data_center = request.GET.get("dc", None)
        request.has_err = False
        if not data_center:
            request.has_err = True
            return func(request, *args, **kwds)
        master = request.GET.get("master", None)
        if not master:
            request.has_err = True
            return func(request, *args, **kwds)
        trace = request.GET.get("trace", None)
        if not trace:
            request.has_err = True
            return func(request, *args, **kwds)
        request.trace = trace
        request.data_center = data_center
        request.master = master
        return func(request, *args, **kwds)
    return data_center_decorator_wrapper

def sql_decorator(func):
    @data_center_decorator
    def sql_wrapper(request, *args, **kwds):
        request.has_err = False
        db = request.GET.get("db", None)
        if not db:
            request.has_err = True
            request.err = "db is required"
            return func(request, *args, **kwds)
        request.db = db
        sql = request.GET.get("sql", None)
        if not sql:
            request.has_err = True
            request.err = "sql is required"
            return func(request, *args, **kwds)
        request.sql = urllib.unquote(sql)
        limit = request.GET.get("limit", "10000")
        request.limit = int(limit)
        return func(request, *args, **kwds)
    return sql_wrapper

def data_filter(data, fields = []):
    new_dict = {}
    if fields[0] == "*":
        return data
    for key in data:
        if key not in fields:
            continue
        new_dict[key] = data[key]
    return new_dict

def job_event_processor(resultset, fields=[], limit=100):
    if not fields:
        return []
    job_event = log_pb2.JobEvent()
    events = []
    for result in resultset:
        for d in result.data_list:
            job_event.ParseFromString(d)
            data = pb2dict.protobuf_to_dict(job_event)
            data["state"] = master_pb2.JobState.Name(data["state"])
            data["level"] = log_pb2.TraceLevel.Name(job_event.level)
            data["update_state"] = master_pb2.JobUpdateState.Name(data["update_state"])
            events.append(data_filter(data, fields))
    return events


def job_stat_processor(resultset, fields=[], limit=100):
    if not fields:
        return []
    stats = []
    job_stat = log_pb2.JobStat()
    for result in resultset:
        for d in result.data_list:
            job_stat.ParseFromString(d)
            data = util.pb2dict(job_stat)
            stats.append(data_filter(data, fields))
    stats = sorted(stats, key=lambda x:x["time"])
    return stats

def pod_event_processor(resultset, fields=[], limit=100):
    if not fields:
        return []
    events = []
    pod_event = log_pb2.PodEvent()
    for data in resultset:
        for d in data.data_list:
            pod_event.ParseFromString(d)
            e = util.pb2dict(pod_event)
            e["stage"] = galaxy_pb2.PodStage.Name(pod_event.stage)
            e["level"] = log_pb2.TraceLevel.Name(pod_event.level)
            if e["level"] not in ["TERROR", "TWARNING"]:
                continue
            e["state"] = galaxy_pb2.PodState.Name(pod_event.state)
            events.append(data_filter(e, fields))
    events = sorted(events, key=lambda x:x["time"])
    return events[0:limit]

def task_event_processor(resultset, fields=[], limit=100):
    events = []
    task_event = log_pb2.TaskEvent()
    for data in resultset:
        for d in data.data_list:
            task_event.ParseFromString(d)
            e = util.pb2dict(task_event)
            e["initd_port"] = e["initd_addr"].split(":")[-1]
            e["stage"] = agent_pb2.TaskStage.Name(task_event.stage)
            e["level"] = log_pb2.TraceLevel.Name(task_event.level)
            e["state"] = galaxy_pb2.TaskState.Name(task_event.state)
            e["main"] = initd_pb2.ProcessStatus.Name(task_event.main)
            e["ftime"] = datetime.datetime.fromtimestamp(e['ttime']/1000000).strftime("%Y-%m-%d %H:%M:%S")
            e["deploy"] = initd_pb2.ProcessStatus.Name(task_event.deploy)
            events.append(data_filter(e, fields))
    events = sorted(events, key=lambda x:x["ttime"], reverse = True)
    return events[0:limit]

def cluster_stat_processor(resultset, fields=[], limit=100):
    stats = []
    cluster_stat = log_pb2.ClusterStat()
    for data in resultset:
        for d in data.data_list:
            cluster_stat.ParseFromString(d)
            stats.append(data_filter(util.pb2dict(cluster_stat), fields))
    stats = sorted(stats, key=lambda x:x["time"])
    return stats[0:limit]

def agent_event_processor(resultset, fields=[], limit=100):
    stats = []
    agent_event = log_pb2.AgentEvent()
    for data in resultset:
        for d in data.data_list:
            agent_event.ParseFromString(d)
            e = util.pb2dict(agent_event)
            e['ftime'] = datetime.datetime.fromtimestamp(e['time']/1000000).strftime("%Y-%m-%d %H:%M:%S")
            stats.append(data_filter(e , fields))
    stats = sorted(stats, key=lambda x:x["time"])
    return stats[0:limit]

def pod_stat_processor(resultset, fields=[], limit=100):
    stats = []
    pod_stat = log_pb2.PodStat()
    for data in resultset:
        for d in data.data_list:
            pod_stat.ParseFromString(d)
            e = util.pb2dict(pod_stat)
            e['ftime'] = datetime.datetime.fromtimestamp(e['time']/1000000).strftime("%Y-%m-%d %H:%M:%S")
            stats.append(data_filter(e , fields))
    return stats[0:limit]

PROCESSOR_MAP={
        "baidu.galaxy":{
            "JobEvent":job_event_processor,
            "JobStat":job_stat_processor,
            "PodEvent":pod_event_processor,
            "TaskEvent":task_event_processor,
            "ClusterStat":cluster_stat_processor,
            "AgentEvent":agent_event_processor,
            "PodStat":pod_stat_processor
    }
}

@data_center_decorator
def index(request):
    return util.render_tpl(request, {"dc":request.data_center,"master":request.master,"trace":request.trace}, "index.html")

def sql_to_mdt(db, sql, limit):
    operator_dict = {
            "=":query_pb2.RpcEqualTo,
            "<":query_pb2.RpcLess,
            "<=":query_pb2.RpcLessEqual,
            ">":query_pb2.RpcGreater,
    }
    context, status = sql_parser.SimpleSqlParser().parse(sql)
    if not status:
        return None, None, False
    logger.info(context)
    request = query_pb2.RpcSearchRequest()
    request.db_name = db
    request.table_name = context["table"]
    request.limit = limit
    conds = []
    has_start_time = False
    has_end_time = False
    for cond in context["conditions"]:
        if cond[0] == "id":
            request.primary_key = cond[2]
        elif cond[0] == "time" and cond[1].startswith(">"):
            if isinstance(cond[2], unicode) or isinstance(cond[2], str):
                request.start_timestamp = long(time.mktime(time.strptime(cond[2], "%Y-%m-%d %H:%M"))) * 1000000
            else:
                request.start_timestamp = cond[2]
            has_start_time = True
        elif cond[0] == "time" and cond[1].startswith("<"):
            if isinstance(cond[2], unicode) or isinstance(cond[2], str):
                request.end_timestamp = long(time.mktime(time.strptime(cond[2], "%Y-%m-%d %H:%M"))) * 1000000
            else:
                request.end_timestamp = cond[2]
            has_end_time = True
        else:
            condition = request.condition.add()
            condition.cmp_key = cond[2]
            condition.cmp = operator_dict[cond[1]]
            condition.index_table_name = cond[0]
    end_time =  datetime.datetime.now()
    start_time = end_time - datetime.timedelta(hours = 24)
    if not has_start_time:
        request.start_timestamp = long(time.mktime(start_time.timetuple())) * 1000000
    if not has_end_time:
        request.end_timestamp = long(time.mktime(end_time.timetuple())) * 1000000
    return context, request, True

def gen_tpl(fields):
    tpl="""
  <table class="table table-striped">
  <thead>
    <tr>
    %(head)s
    </tr>
  </thead>
  <tbody>
    {{#datas}}
    <tr>
     %(body)s
    </tr>
    {{/datas}}
   </tbody>
</table>
    """
    head = ""
    body = ""
    for field in fields:
        head += "<th>%s</th>"%field
        body += "<td>{{%s}}</td>"%field
    tpl = tpl%{"head":head, "body":body}
    return tpl


@sql_decorator
def squery(request):
    builder = http.ResponseBuilder()
    if request.has_err:
        return builder.error(request.err).build_json()
    context, pb_req, ok = sql_to_mdt(request.db, request.sql, request.limit)
    if not ok:
        return builder.error("fail to parse sql").build_json()
    ftrace = fsdk.FtraceSDK(request.trace)
    resultset, ok = ftrace.make_req(pb_req)
    if not ok:
        return builder.error("fail to parse sql").build_json()
    proc_func = PROCESSOR_MAP[request.db][context["table"]]
    datas= proc_func(resultset, context["fields"], request.limit)
    return builder.ok(data = {"dc":request.data_center,"master":request.master,"trace":request.trace,"datas":datas, "tpl":gen_tpl(context["fields"])}).build_json()

@data_center_decorator
def sql(request):
    return util.render_tpl(request, {"dc":request.data_center,"master":request.master,"trace":request.trace},"sql.html")

@data_center_decorator
def cluster(request):
    return util.render_tpl(request, {"dc":request.data_center,"master":request.master, "trace":request.trace},"cluster.html")

@data_center_decorator
def job_all(request):
    galaxy = sdk.GalaxySDK(request.master)
    jobs, status = galaxy.get_all_job()
    job_dicts = []
    for job in jobs:
        job_dict = pb2dict.protobuf_to_dict(job)
        job_dict['state'] = master_pb2.JobState.Name(job_dict['state'])   
        job_dicts.append(job_dict)
    return util.render_tpl(request, {"jobs":job_dicts,
                                     "dc":request.data_center,
                                     "master":request.master,
                                     "trace":request.trace}, "index.html")

@data_center_decorator
def pod_all(request):
    galaxy = sdk.GalaxySDK(request.master)
@data_center_decorator
def job_detail(request):

    return util.render_tpl(request, {"dc":request.data_center,"master":request.master,"trace":request.trace, "jobid":request.GET.get("jobid", None)},
                                     "job.html")

def get_total_status(request):
    dcs = {}
    with open(settings.LITE_DB_PATH, "rb") as fd:
        dcs = json.load(fd)
    datas = []
    for key in dcs:
        galaxy = sdk.GalaxySDK(dcs[key]["master"])
        response = galaxy.get_real_time_status()
        builder = http.ResponseBuilder()
        status = pb2dict.protobuf_to_dict(response)
        datas.append(status);
    builder = http.ResponseBuilder()
    return builder.ok(data = {"status":datas}).build_json()

@data_center_decorator
def get_real_time_status(request):
    galaxy = sdk.GalaxySDK(request.master)
    response = galaxy.get_real_time_status()
    builder = http.ResponseBuilder()
    status = pb2dict.protobuf_to_dict(response)
    return builder.ok(data = {"status":status,"dc":request.data_center,"master":request.master}).build_json()
