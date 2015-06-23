# -*- coding:utf-8 -*-
from common import http
from common import decorator
from django.forms.models import model_to_dict
@decorator.api_auth_required
def get_conf(request):
    builder = http.ResponseBuilder()
    home = [{"nodeName":"服务列表","href":"#service","nodestyle":["level1"],"children":[]},
    {"nodeName":"配额信息","href":"#quota","nodestyle":["level1"],"children":[]},
    {"nodeName":"集群信息","href":"#cluster","nodestyle":["level1"],"children":[
        {"nodeName":"集群状态","href":"#cluster/status","subpage":"status","nodestyle":["level2"],"children":[]},
        {"nodeName":"集群标签","href":"#cluster/tag","subpage":"tag","nodestyle":["level2"],"children":[]}
        ]},
    {"nodeName":"配置集群","href":"#setup","nodestyle":["level1"],"children":[]}]
    service = [{ "nodeName" : "Tera", "nodestyle":["level1"], "children" : [ { "nodestyle":["level2"],"subpage":"status","nodeName" : "服务状态", "children" : []}] }]
    return builder.ok(data = {'home':home,'service':service,'user':{'username':request.user.username,'id':request.user.id}}).build_json()


