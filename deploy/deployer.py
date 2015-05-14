#!/usr/bin/env python2.7
# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com
# Date: 2015-04-20
import argparse
import json
import sys
import os
import traceback
from paramiko import client 
from argparse import RawTextHelpFormatter
BACK_GROUND_BLACK = '\033[1;40m'
BACK_GROUND_RED = '\033[1;41m'
BACK_GROUND_GREEN = '\033[1;42m'
BACK_GROUND_YELLOW = '\033[1;43m'
BACK_GROUND_BLUE = '\033[1;44m'
BACK_GROUND_BLUE_UNDERLINE = '\033[4;44m'
BACK_GROUND_PURPLE = '\033[1;45m'
BACK_GROUND_CYAN = '\033[1;46m'
BACK_GROUND_WHITE = '\033[1;47m'
TEXT_COLOR_BLACK = '\033[1;30m'
TEXT_COLOR_RED = '\033[1;31m'
TEXT_COLOR_GREEN = '\033[1;32m'
TEXT_COLOR_YELLOW = '\033[1;33m'
TEXT_COLOR_BLUE = '\033[1;34m'
TEXT_COLOR_PURPLE = '\033[1;35m'
TEXT_COLOR_CYAN = '\033[1;36m'
TEXT_COLOR_WHITE = '\033[1;37m'
RESET = '\033[1;m'
USE_SHELL = sys.platform.startswith("win")

class RichText(object):
    @classmethod
    def render(cls, text, bg_color, color=TEXT_COLOR_WHITE):
        """render
        """
        if USE_SHELL:
            return text
        result = []
        result.append(bg_color)
        result.append(color)
        result.append(text)
        result.append(RESET)
        return ''.join(result)

    @classmethod
    def render_text_color(cls, text, color):
        """render text with color
        """
        if USE_SHELL:
            return text
        return ''.join([color, text, RESET])

    @classmethod
    def render_green_text(cls, text):
        """render green text
        """
        return cls.render_text_color(text, TEXT_COLOR_GREEN)

    @classmethod
    def render_red_text(cls, text):
        """render red text
        """
        return cls.render_text_color(text, TEXT_COLOR_RED)

    @classmethod
    def render_blue_text(cls, text):
        """render blue text
        """
        return cls.render_text_color(text, TEXT_COLOR_BLUE)

    @classmethod
    def render_yellow_text(cls, text):
        """render yellow text
        """
        return cls.render_text_color(text, TEXT_COLOR_YELLOW)


usage = """
   deployer <command> [<args>...]
"""
epilog = """
bug reports<>
"""
    
def build_parser(deployer):
    parser = argparse.ArgumentParser(
                       prog="deployer.py",
                       formatter_class=RawTextHelpFormatter,
                       epilog=epilog,
                       usage=usage)
    subparsers = parser.add_subparsers()
    stop_sub = subparsers.add_parser("stop",
                                 help="stop apps on all hosts")
    stop_sub.add_argument('-v',
                          default=False,
                          dest="show_output",
                          action="store_true",
                          help="show output and err")
    stop_sub.add_argument('-c',
                          default=None,
                          dest="config",
                          action="store",
                          help="specify config file ")

    stop_sub.add_argument('-u',
                          default=None,
                          dest="user",
                          action="store",
                          help="specify user on host")
    
    stop_sub.add_argument('-p',
                          default=None,
                          dest="password",
                          action="store",
                          help="specify password on host")
    stop_sub.set_defaults(func=deployer.stop)
 
    fetch_sub = subparsers.add_parser("fetch",help="fetch package to all hosts")
    fetch_sub.add_argument('-v',
                          default=False,
                          dest="show_output",
                          action="store_true",
                          help="show output and err")
    fetch_sub.add_argument('-c',
                          default=None,
                          dest="config",
                          action="store",
                          help="specify config file")

    fetch_sub.add_argument('-u',
                          default=None,
                          dest="user",
                          action="store",
                          help="specify user on host")
    
    fetch_sub.add_argument('-p',
                          default=None,
                          dest="password",
                          action="store",
                          help="specify password on host")
    
    fetch_sub.set_defaults(func=deployer.fetch)

    start_sub = subparsers.add_parser("start",help="start apps on all hosts")
    start_sub.add_argument('-v',
                          default=False,
                          dest="show_output",
                          action="store_true",
                          help="show output and err")
    start_sub.add_argument('-c',
                          default=None,
                          dest="config",
                          action="store",
                          help="specify config file")

    start_sub.add_argument('-u',
                          default=None,
                          dest="user",
                          action="store",
                          help="specify user on host")
    
    start_sub.add_argument('-p',
                          default=None,
                          dest="password",
                          action="store",
                          help="specify password on host")
   
    start_sub.set_defaults(func=deployer.start)
    init_sub = subparsers.add_parser("init",
                                 help="init all nodes")
    init_sub.add_argument('-v',
                          default=False,
                          dest="show_output",
                          action="store_true",
                          help="show output and err")
    init_sub.add_argument('-c',
                          default=None,
                          dest="config",
                          action="store",
                          help="specify config file")

    init_sub.add_argument('-u',
                          default=None,
                          dest="user",
                          action="store",
                          help="specify user on host")
    
    init_sub.add_argument('-p',
                          default=None,
                          dest="password",
                          action="store",
                          help="specify password on host")
    init_sub.set_defaults(func=deployer.init)
 
    clean_sub = subparsers.add_parser("clean",
                                 help="clean all nodes")
    clean_sub.add_argument('-v',
                          default=False,
                          dest="show_output",
                          action="store_true",
                          help="show output and err")
    clean_sub.add_argument('-c',
                          default=None,
                          dest="config",
                          action="store",
                          help="specify config file")

    clean_sub.add_argument('-u',
                          default=None,
                          dest="user",
                          action="store",
                          help="specify user on host")
    
    clean_sub.add_argument('-p',
                          default=None,
                          dest="password",
                          action="store",
                          help="specify password on host")
    clean_sub.set_defaults(func=deployer.clean)
     
    migrate_sub = subparsers.add_parser("migrate",
                             help="migrate all apps")
    migrate_sub.add_argument('-v',
                          default=False,
                          dest="show_output",
                          action="store_true",
                          help="show output and err")
    migrate_sub.add_argument('-c',
                          default=None,
                          dest="config",
                          action="store",
                          help="specify config file")

    migrate_sub.add_argument('-u',
                          default=None,
                          dest="user",
                          action="store",
                          help="specify user on host")
    
    migrate_sub.add_argument('-p',
                          default=None,
                          dest="password",
                          action="store",
                          help="specify password on host")
    migrate_sub.set_defaults(func=deployer.migrate)
 



    return parser


class Deployer(object):
    def __init__(self):
        self.show_output = False

    def _exec_cmd_on_host(self,host,cmds,real_time=False):
        """
        exec cmd on specify host
        """
        ret_dict = {}
        try:
            sshclient = self._build_client()
            sshclient.connect(host,username=self.user,password=self.password)
            for cmd in cmds:
                stdin,stdout,stderr = sshclient.exec_command(cmd)
                retcode = stdout.channel.recv_exit_status()
                ret_dict[cmd] = retcode 
                if not real_time:
                    continue
                for line in stdout:
                    print line
                for line in stderr:
                    print line
            sshclient.close()
        except Exception as e:
            traceback.print_exc(file=sys.stdout)
        return ret_dict

    def init(self,options):
        self.password = options.password
        self.user = options.user
        for node in options.module.NODE_LIST:
            ret_dict = self._exec_cmd_on_host(node,options.module.INIT_SYS_CMDS,options.show_output)
            for key in ret_dict:
                if ret_dict[key] == 0:
                    print "exec %s on %s %s"%(key,node,RichText.render_green_text("succssfully"))
                else: 
                    print "exec %s on %s %s"%(key,node,RichText.render_red_text("error"))

    def clean(self,options):
        self.password = options.password
        self.user = options.user
        for node in options.module.NODE_LIST:
            ret_dict = self._exec_cmd_on_host(node,options.module.CLEAN_SYS_CMDS,options.show_output)
            for key in ret_dict:
                if ret_dict[key] == 0:
                    print "exec %s on %s %s"%(key,node,RichText.render_green_text("succssfully"))
                else: 
                    print "exec %s on %s %s"%(key,node,RichText.render_red_text("error"))

    def fetch(self,options):
        self.password = options.password
        self.user = options.user
        for app in options.module.APPS:
            print "fetch app %s"%RichText.render_green_text(app['name']) 
            fetch_cmd = "mkdir -p %s && cd %s  && wget -O tmp.tar.gz %s && tar -zxvf tmp.tar.gz"%(app['workspace'], 
                                                  app['workspace'],
                                                                                  app['package'])
            print "exec %s"%fetch_cmd
            for host in app['hosts']:
                ret_dict = self._exec_cmd_on_host(host,[fetch_cmd],options.show_output)
                if ret_dict[fetch_cmd] ==0 :
                    print "fetch on %s %s"%(host,RichText.render_green_text("succssfully"))
                else: 
                    print "fetch on %s %s"%(host,RichText.render_red_text("error"))

    def start(self,options):
        self.password = options.password
        self.user = options.user 
        for app in options.module.APPS:
            print "start app %s"%RichText.render_green_text(app["name"])
            start_cmd = "cd %s && %s"%(app['workspace'],
                                       app['start_cmd'])
            print "exec %s"%start_cmd
            for host in app['hosts']:
                ret_dict = self._exec_cmd_on_host(host,[start_cmd],True) 
                if ret_dict[start_cmd] == 0 :           
                    print "start on %s %s"%(host,RichText.render_green_text("succssfully"))
                else: 
                    print "start on %s %s"%(host,RichText.render_red_text("error"))

    def stop(self,options):
        self.password = options.password
        self.user = options.user 
        for app in options.module.APPS:
            print "stop app %s"%app["name"]
            stop_cmd = "cd %s && %s"%(app["workspace"],app["stop_cmd"])
            print "exec %s"%stop_cmd
            for host in app["hosts"]:
                ret_dict = self._exec_cmd_on_host(host,[stop_cmd])          
                if ret_dict[stop_cmd] == 0 : 
                    print "stop on %s %s"%(host,RichText.render_green_text("succssfully"))
                else: 
                    print "stop on %s %s"%(host,RichText.render_red_text("error"))
    def migrate(self,options):
        self.fetch(options)
        self.stop(options)
        self.start(options)

    def _build_client(self):
        sshclient = client.SSHClient()
        sshclient.load_system_host_keys()
        sshclient.set_missing_host_key_policy(client.AutoAddPolicy())
        return sshclient


if __name__ == "__main__":
    deploy = Deployer()
    parser = build_parser(deploy)
    options = parser.parse_args()
    sys.path.append(os.getcwd())
    if not options.config:
        print "-c parameter is required"
        sys.exit(-1)
    md = options.config.replace(".py","")
    module = __import__(md)
    options.module = module
    options.func(options)
