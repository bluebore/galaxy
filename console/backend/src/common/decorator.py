# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com
# Date: 2015-03-30

from common import http
from bootstrap import settings
from bootstrap import models
def post_required(func):
    """
    http request post method decorator
    """
    def post_wrapper(request,*args,**kwds):
        res = http.ResponseBuilder()
        if request.method != 'POST':
            return res.error("post is required").build_json()
        return func(request,*args,**kwds)
    return post_wrapper

def api_auth_required(func):
    def api_auth_wrapper(request, *args, **kwds):
        res = http.ResponseBuilder()
        if request.user.is_authenticated():
            return func(request, *args, **kwds)
        return res.error("%s?service=%s"%(settings.UUAP_CAS_SERVER,settings.MY_HOST),
                          status = -2).build_json()
    return api_auth_wrapper

def job_permission_requred(func):
    def job_permission_required_wrapper(request, *args, **kwds):
        res = http.ResponseBuilder()
        job_id = request.GET.get('id', None)
        if not job_id:
            return res.error("id is required").build_json()
        master = request.GET.get('master', None)
        if not master:
            return res.error('master is required').build_json()
       
