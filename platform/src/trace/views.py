# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import time
import urllib
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

def sql_decorator(func):
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

def query_decorator(func):
    def query_wrapper(request, *args, **kwds):
        start_time = request.GET.get("start", None)
        end_time = request.GET.get("end", None)
        request.has_err = False
        if not end_time or not start_time:
            end_time =  datetime.datetime.now()
            start_time = end_time - datetime.timedelta(hours = 1)
            request.start_time = long(time.mktime(start_time.timetuple())) * 1000000
            request.end_time = long(time.mktime(end_time.timetuple())) * 1000000
        else:
            request.start_time = long(start_time)
            request.end_time = long(end_time)
        db = request.GET.get("db", None)
        if not db :
            request.has_err = True
            request.err = "db is required"
            return func(request, *args, **kwds)
        request.db = db
        table = request.GET.get("table", None)
        if not table:
            request.has_err = True
            request.err = "table is required"
            return func(request, *args, **kwds)
        request.table = table
        fields = request.GET.get("fields", None)
        if not fields:
            request.has_err = True
            request.err = "fields is required"
            return func(request, *args, **kwds)
        request.fields = fields.split(",")
        request.reverse = request.GET.get("reverse", None)
        limit = request.GET.get("limit", "100")
        request.limit = int(limit)
        return func(request, *args, **kwds)
    return query_wrapper

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
            stats.append(data_filter(util.pb2dict(agent_event), fields))
    return stats[0:limit]


PROCESSOR_MAP={
        "baidu.galaxy":{
            "JobEvent":job_event_processor,
            "JobStat":job_stat_processor,
            "PodEvent":pod_event_processor,
            "TaskEvent":task_event_processor,
            "ClusterStat":cluster_stat_processor,
            "AgentEvent":agent_event_processor
    }
}

@query_decorator
def query(request):
    builder = http.ResponseBuilder()
    if request.has_err:
        return builder.error(request.err).build_json()
    ftrace = fsdk.FtraceSDK(settings.TRACE_QUERY_ENGINE)
    id = request.GET.get("id", None)
    jobid = request.GET.get("jobid", None)
    podid = request.GET.get("podid", None)
    resultset = []
    status = False
    if id :
        resultset, status = ftrace.simple_query(request.db, 
                                                request.table, 
                                                id,
                                                request.start_time,
                                                request.end_time,
                                                request.limit)
    elif jobid:
        resultset, status = ftrace.index_query(request.db,
                                               request.table,
                                               "jobid",
                                               jobid,
                                               request.start_time,
                                               request.end_time,
                                               request.limit)
    elif podid:
        resultset, status = ftrace.index_query(request.db,
                                               request.table,
                                               "pod_id",
                                               podid,
                                               request.start_time,
                                               request.end_time,
                                               request.limit)
    if not status:
        return builder.error("fail to make a query").build_json()
    proc_func = PROCESSOR_MAP[request.db][request.table]
    datas= proc_func(resultset, request.fields, request.limit)
    return builder.ok(data = {"datas":datas}).build_json()

def index(request):
    return util.render_tpl(request, {}, "index.html")

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
  <table class="table">
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
    ftrace = fsdk.FtraceSDK(settings.TRACE_QUERY_ENGINE)
    resultset, ok = ftrace.make_req(pb_req)
    if not ok:
        return builder.error("fail to parse sql").build_json()
    proc_func = PROCESSOR_MAP[request.db][context["table"]]
    datas= proc_func(resultset, context["fields"], request.limit)
    return builder.ok(data = {"datas":datas, "tpl":gen_tpl(context["fields"])}).build_json()

def sql(request):
    return util.render_tpl(request, {},"sql.html")

def cluster(request):
    return util.render_tpl(request, {},"cluster.html")

def job_stat(request):
    jobid = request.GET.get("jobid", None)
    reverse = request.GET.get("reverse", None)
    builder = http.ResponseBuilder()
    if not jobid:
        return builder.error("jobid is required").build_json()
    end_time = datetime.datetime.now()
    start_time = end_time - datetime.timedelta(hours = 1)
    trace_dao = dao.TraceDao(settings.TRACE_QUERY_ENGINE)
    stats, status = trace_dao.get_job_stat(jobid,
                                     time.mktime(start_time.timetuple()),
                                     time.mktime(end_time.timetuple()))
    if not status:
        return builder.error("fail to get job stat").build_json()
    if reverse:
        stats = sorted(stats, key=lambda x:x["time"], reverse = True)
    return builder.ok(data = {"stats":stats}).build_json()

def get_pod_event_by_jobid(request):
    jobid = request.GET.get("jobid", None)
    reverse = request.GET.get("reverse", None)
    builder = http.ResponseBuilder()
    if not jobid:
        return builder.error("jobid is required").build_json()
    end_time = datetime.datetime.now()
    start_time = end_time - datetime.timedelta(hours = 24)
    trace_dao = dao.TraceDao(settings.TRACE_QUERY_ENGINE)
    events, status = trace_dao.get_pod_event_by_jobid(jobid,
                                     time.mktime(start_time.timetuple()),
                                     time.mktime(end_time.timetuple()),
                                     limit=50)
    if not status:
        return builder.error("fail to get pod event").build_json()
    filter_events = []
    for e in events:
        if e["level"] == "TINFO":
            continue

        e["ftime"] = datetime.datetime.fromtimestamp(e['time']/1000000).strftime("%Y-%m-%d %H:%M:%S")
        filter_events.append(e)
    filter_events = sorted(filter_events, key=lambda x:x["time"], reverse = True)
    return builder.ok(data = {"events": filter_events}).build_json()


def job_event(request):
    jobid = request.GET.get("jobid", None)
    builder = http.ResponseBuilder()
    if not jobid:
        return builder.error("jobid is required").build_json()
    end_time = datetime.datetime.now()
    start_time = end_time - datetime.timedelta(hours = 1)
    trace_dao = dao.TraceDao(settings.TRACE_QUERY_ENGINE)
    events, status = trace_dao.get_job_event(jobid,
                                     time.mktime(start_time.timetuple()),
                                     time.mktime(end_time.timetuple()))
    if not status:
        return builder.error("fail to get job evnets").build_json()
    events = sorted(events, key=lambda x:x["time"], reverse = True)
    return builder.ok(data = {"events":events}).build_json()

def get_pod(request):
    podid = request.GET.get("podid", None)
    if not podid:
        return util.render_tpl(request, {}, "404.html")
    end_time = datetime.datetime.now()
    start_time = end_time - datetime.timedelta(hours = 1)
    trace_dao = dao.TraceDao(settings.TRACE_QUERY_ENGINE)
    pod_events, status = trace_dao.get_pod_event(podid,
                                     time.mktime(start_time.timetuple()),
                                     time.mktime(end_time.timetuple()))
    if not status:
        return util.render_tpl(request, {"err":"fail to get trace"}, "500.html")

    task_events, status = trace_dao.get_task_event(podid,
                                     time.mktime(start_time.timetuple()),
                                     time.mktime(end_time.timetuple()))

    return util.render_tpl(request, {"podid":podid,
                                     "pod_events":pod_events,
                                     "task_events":task_events},
                                     "pod_trace.html")
def pod_event(request):
    podid = request.GET.get("podid", None)
    builder = http.ResponseBuilder()
    if not podid:
        return builder.error("podid is required").build_json()
    end_time = datetime.datetime.now()
    start_time = end_time - datetime.timedelta(hours = 1)
    trace_dao = dao.TraceDao(settings.TRACE_QUERY_ENGINE)
    events, status = trace_dao.get_pod_event(podid,
                                     time.mktime(start_time.timetuple()),
                                     time.mktime(end_time.timetuple()))
    if not status:
        return builder.error("fail to get pod event").build_json()
    events = sorted(events, key=lambda x:x["time"], reverse = True)
    return builder.ok(data = {"events": events}).build_json()

def task_event(request):
    podid = request.GET.get("podid", None)
    builder = http.ResponseBuilder()
    if not podid:
        return builder.error("podid is required").build_json()

    trace_dao = dao.TraceDao(settings.TRACE_QUERY_ENGINE)
    end_time = datetime.datetime.now()
    start_time = end_time - datetime.timedelta(hours = 1)
    task_events, status = trace_dao.get_task_event(podid,
                                     time.mktime(start_time.timetuple()),
                                     time.mktime(end_time.timetuple()),
                                     limit=20)

    if not status:
        return builder.error("fail to get task event").build_json()
    filter_events = []
    for e in task_events:
        if e["level"] == "TINFO":
            continue
        e["ftime"] = datetime.datetime.fromtimestamp(e['ttime']/1000000).strftime("%Y-%m-%d %H:%M:%S")
        filter_events.append(e)
    filter_events = sorted(filter_events, key=lambda x:x["ttime"], reverse = True)
    return builder.ok(data = {"events": filter_events}).build_json()

def pod_stat(request):
    podid = request.GET.get("podid", None)
    builder = http.ResponseBuilder()
    if not podid:
        return builder.error("podid is required").build_json()
    end_time = datetime.datetime.now()
    start_time = end_time - datetime.timedelta(hours = 1)
    trace_dao = dao.TraceDao(settings.TRACE_QUERY_ENGINE)
    stats, status = trace_dao.get_pod_stat(podid,
                                     time.mktime(start_time.timetuple()),
                                     time.mktime(end_time.timetuple()))
    if not status:
        return builder.error("fail to get pod stat").build_json()
    stats = sorted(stats, key=lambda x:x["time"], reverse = True)
    return builder.ok(data = {"stats":stats}).build_json()

def job_all(request):
    galaxy = sdk.GalaxySDK(settings.GALAXY_MASTER)
    jobs, status = galaxy.get_all_job()
    job_dicts = []
    for job in jobs:
        job_dict = pb2dict.protobuf_to_dict(job)
        job_dict['state'] = master_pb2.JobState.Name(job_dict['state'])   
        job_dicts.append(job_dict)
    return util.render_tpl(request, {"jobs":job_dicts}, "index.html")

def job_detail(request):

    return util.render_tpl(request, {"jobid":request.GET.get("jobid", None)},
                                     "job.html")

def pod_detail(request):
    return util.render_tpl(request, {"podid":request.GET.get("podid", None),
                                     "time":request.GET.get("time",None)},
                                     "pod_detail.html")

def get_real_time_status(request):
    galaxy = sdk.GalaxySDK(settings.GALAXY_MASTER)
    response = galaxy.get_real_time_status()
    builder = http.ResponseBuilder()
    status = pb2dict.protobuf_to_dict(response)
    return builder.ok(data = {"status":status}).build_json()
