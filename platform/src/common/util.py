# -*- coding:utf-8 -*-
from bootstrap import settings
from common import http

def render_tpl(req, context, tpl_path):
    render = http.ResponseBuilder()
    media_url = req.build_absolute_uri("/statics")
    root_url = req.build_absolute_uri("/")
    context["media_url"] = media_url
    context["root_url"] = root_url
    return render.set_content(context)\
                 .add_req(req)\
                 .build_tmpl(tpl_path)

def pb2dict(message):
    fields = message.ListFields()
    data = {}
    for (field, value) in fields:
        data[field.name] = value
    return data

