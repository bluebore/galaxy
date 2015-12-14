import datetime
from common import http
from lumia import sdk as lu_sdk
from galaxy import sdk as ga_sdk
from bootstrap import settings
def get_overview(request):
    response_builder = http.ResponseBuilder()
    lumia_addr = request.GET.get("lumia", settings.LUMIA_ADDR)
    galaxy_addr = request.GET.get("galaxy", settings.GALAXY_ADDR)
    lumia = lu_sdk.LumiaSDK(lumia_addr)
    minions = lumia.get_overview()
    minions_formated = []
    for minion in minions:
        order = 1
        if not minion.mount_ok or not minion.device_ok:
            order = 0
        minions_formated.append({
                             "ip":minion.ip, 
                             "order":order,
                             "hostname":minion.hostname,
                             "mount_ok":minion.mount_ok,
                             "device_ok":minion.device_ok,
                             "last_update":datetime.datetime.fromtimestamp(minion.datetime/1000000).strftime("%Y-%m-%d %H:%M:%S") })
    sort_func = lambda x : x["order"] 
    ordered_minions = sorted(minions_formated, key=sort_func, reverse=False)
    return response_builder.add_params({"status":0,"msg":"ok","minions":ordered_minions,"lumia_addr":lumia_addr})\
                           .add_req(request)\
                           .build_tmpl("minion.html")

def get_detail(request):
    response_builder = http.ResponseBuilder()
    lumia_addr = request.GET.get("lumia", settings.LUMIA_ADDR)
    ip = request.GET.get("ip", None)
    if not ip:
        return response_builder.add_params({})\
                           .add_req(request)\
                           .build_tmpl("404.html")
    lumia = lu_sdk.LumiaSDK(lumia_addr)
    pb_minions = lumia.show_minion([ip])
    minion = {}
    for m in pb_minions:
        minion = m
    if not minion:
        return response_builder.add_params({})\
                           .add_req(request)\
                           .build_tmpl("404.html")
    return response_builder.add_params({"status":0,"msg":"ok","minion":minion})\
                           .add_req(request)\
                           .build_tmpl("minion-detail.html")

def get_status(req):
    response_builder = http.ResponseBuilder()
    lumia_addr = req.GET.get("lumia", settings.LUMIA_ADDR)
    lumia = lu_sdk.LumiaSDK(lumia_addr)
    live_nodes, dead_nodes = lumia.get_status()
    return response_builder.add_params({"status":0,"msg":"ok","live_nodes":live_nodes,"dead_nodes":dead_nodes})\
                           .add_req(req)\
                           .build_tmpl("status.html")

def report(req):
    response_builder = http.ResponseBuilder()
    lumia_addr = req.GET.get("lumia", settings.LUMIA_ADDR)
    lumia = lu_sdk.LumiaSDK(lumia_addr)
    ip = req.GET.get("ip",None)
    if not ip:
        return response_builder.add_params({"status":-1, "msg":"ip is required"})\
                           .add_req(req)\
                           .build_tmpl("report.html")

    status = lumia.report(ip)
    return response_builder.add_params({"status":status, "msg":"ok"})\
                           .add_req(req)\
                           .build_tmpl("report.html")

    
