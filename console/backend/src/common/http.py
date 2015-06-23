# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com

import cgi
import logging
import tempfile
import urllib
import urllib2
import json
from django.http import response
from django import shortcuts
from django.template import context
class ResponseBuilder(object):
    def __init__(self, request=None):
        self.content_type = 'text/html'
        self.status = 200
        self.content = None
        self.request = request

    def set_status(self, status):
        self.status = status
        return self

    def set_content(self, content):
        self.content = content
        return self

    def set_content_type(self, content_type):
        self.content_type = content_type
        return self


    def add_context_attr(self, key, value):
        if not self.content:
            self.content = {}
            self.content[key] = value
        elif  type(self.content) == dict:
            self.content[key] = value
        else:
            raise Exception('the type of content should  be dict ,but it is %s'
                                % (type(self.content)))
        return self

    def add_params(self, params):
        """添加模板context
        args:
            params {dict}
        """
        if not self.content:
            self.content = {}
        self.content.update(params)
        return self

    def ok(self, data=None):
        self.add_context_attr("status", 0)\
            .add_context_attr('msg', "ok")\
            .add_context_attr('data', data)
        return self

    def error(self, msg, status = -1):
        self.add_context_attr("status", status)\
            .add_context_attr('msg', msg)
        return self

    def build_bson(self, bson_dumps):
        """
         this will ingore the content_type,
        and the type content should be dict
        """

        if not self.content:
             raise Exception('content should not be null')
        if type(self.content) != dict:
            raise Exception('the type of content should  be dict,but it is %s'
                                % (type(self.content)))
        self.set_content(bson_dumps(self.content))\
            .set_content_type('application/json')
        return self.build()

    def build_json(self):
        """
        this will ingore the content_type,
        and the type content should be dict
        """
        if not self.content:
            raise Exception('content should not be null')
        if type(self.content) != dict:
            raise Exception('the type of content should  be dict,but it is %s'
                                % (type(self.content)))
        self.set_content(json.dumps(self.content))\
            .set_content_type('application/json')
        return self.build()

    def add_req(self, request):
        """生成模块函数需要这个对象
        args:
            request {django.http.request.HttpRequest}
        return:
            result {self}
        """
        self.request = request
        return self

    def build_html(self):
        """生成网页内容
        """
        self.set_content_type('text/html')
        return self.build()

    def build_tmpl(self, template):
        """指定模板生成网页代码
         args:
            template {string} 模版名称
        return:
            response {django.http.reponse.HttpResponse}
        """
        # TODO 改善content这个变量名称
        if not self.content:
            self.content = {}
        return shortcuts.render_to_response(template, self.content,
               context_instance=context.RequestContext(self.request))

    def build(self):
        if not self.content:
            raise Exception('content should not be null')
        return response.HttpResponse(content=self.content,
                                     content_type=self.content_type,
                                     status=self.status)


class HttpClient(object):
    """
    http client 类，负责get请求和post请求
    """
    def __init__(self, buffer_size=1024, logger=None):
        self.logger = logger or logging.getLogger(__name__)
        self.buffer_size = buffer_size  # bytes
        self._build_opener()

    def do_get(self, url, headers=None, timeout=10, content_to_file=True):
        """
        以get方式请求一个网页url,返回网页相关描述信息,支持将网页内容保
        存在文件中，防止网页内容占用过多内存
        args:
            url , 请求的网页URL
            headers,请求头信息
            timeout,设置请求超时时间,单位为秒
            content_to_file,网页内容是否保存为文件
        return:
            dict ,返回类型为字典
            当content_to_file==True时，返回数据格式为
            {
             "content_to_file":True,
             "content_path":"/tmp/XsfASDAD",
             "content":None,
             "redirect":None,
             "error":None,
             "require_auth":False,
             "headers":{},
             "encoding":None
            }
            否则为
            {
             "content_to_file":False,
             "content_path":None,
             "content":"<html>hello world</html>",
             "redirect":None,
             "error":None,
             "require_auth":False
             "headers":{},
             "encoding":None
            }
        """
        assert url
        req = self._build_req(url, headers=headers)
        return self._do_service(req, timeout=timeout,
                              content_to_file=content_to_file)

    def do_post(self, url , req_data , headers=None, timeout=10, content_to_file=True):
        """
        以post方式请求一个网页url,支持附加表单信息，不支持上传文件，表单参数为
        touple_list,例如[('id',9527),]
        args:
            url,请求url
            req_data,表单数据,形如touple_list,例如[('id',9527),]
            headers，请求头信息
            timeout, 设置请求超时，单位为秒
            content_to_file,网页内容是否保存为文件
        return:
            dict,返回类型为字典
            当content_to_file==True时，返回数据格式为
            {
             "content_to_file":True,
             "content_path":"/tmp/XsfASDAD",
             "content":None,
             "redirect":None,
             "error":None,
             "require_auth":False,
             "headers":{},
             "encoding":None
            }
            否则为
            {
             "content_to_file":False,
             "content_path":None,
             "content":"<html>hello world</html>",
             "redirect":None,
             "error":None,
             "require_auth":False,
             "headers":{},
             "encoding":None
            }
        """
        assert url
        assert req_data
        req = self._build_req(url, req_data=req_data, headers=headers)
        return self._do_service(req, timeout=timeout,
                                content_to_file=content_to_file)

    def _build_req(self, url, req_data=None, headers=None):
        self.logger.debug("req url:%s,req_data:%s,headers:%s" % (
            url, req_data, headers))
        inner_headers = {}
        if headers:
            inner_headers = headers
        # post
        if req_data:
            req = urllib2.Request(url, data=urllib.urlencode(req_data),
                                  headers=inner_headers)
        else:
            req = urllib2.Request(url, headers=inner_headers)
        return req

    def _do_service(self, req, timeout=10, content_to_file=True):
        resp_meta = {
                      "content_to_file":content_to_file,
                      "content_path":None,
                      "content":None,
                      "error":None,
                      "redirect":None,
                      "require_auth":False,
                      "headers":None,
                      "encoding":None
                     }
        try:
            response = self.opener.open(req)
            resp_meta["headers"] = response.info()
            content_type = resp_meta["headers"].getheader('content-type', None)
            if content_type:
                _, param = cgi.parse_header(content_type)
                resp_meta["encoding"] = param.get("charset", None)
            if content_to_file:
                temp_path = self._save_to_file(response)
                if not temp_path:
                    resp_meta["error"] = "fail to save content to file"
                else:
                    resp_meta["content_path"] = temp_path
            else:
                resp_meta["content"] = response.read()

        except urllib2.HTTPError as e:
            resp_meta["headers"] = e.info()
            self.logger.exception("req http error")
            if 300 <= e.code < 400:
                resp_meta['redirect'] = e.info().getheader("Location", None)
            elif e.code == 403:
                resp_meta["require_auth"] = True
            resp_meta["error"] = e.read()
            e.close()
        except urllib2.URLError as e:
            self.logger.exception("req url error")
            resp_meta["error"] = "url error"
        return resp_meta

    def _build_opener(self):
        # 创建一个opener用于配置请求
        opener = urllib2.OpenerDirector()
        # disable 本地代理
        opener.add_handler(urllib2.ProxyHandler())
        # 对URL不符合要求抛出URLError
        opener.add_handler(urllib2.UnknownHandler())
        # 发送http请求
        opener.add_handler(urllib2.HTTPHandler())
        # 建错误reponse抛出为HttpEerror
        opener.add_handler(urllib2.HTTPDefaultErrorHandler())
        # 发送https请求
        opener.add_handler(urllib2.HTTPSHandler())
        # 对于 http response status code 不属于[200,300)
        # 转换为错误response
        opener.add_handler(urllib2.HTTPErrorProcessor())
        self.opener = opener

    def _save_to_file(self, response_fd):
        try:
            tmp_path = tempfile.mktemp()
            with open(tmp_path, "wb+", self.buffer_size) as fd:
                while True:
                    resp_buffer = response_fd.fp.read(self.buffer_size)
                    if not resp_buffer:
                        break
                    fd.write(resp_buffer)
            return tmp_path
        except IOError:
            self.logger.exception("fail to create temp file")
            return None

