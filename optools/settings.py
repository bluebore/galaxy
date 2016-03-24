# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import socket
host_name = socket.gethostname()
NEXUS_SERVERS="%s:8868,%s:8869,%s:8870,%s:8871,%s:8872"%(host_name, host_name, host_name, host_name, host_name)
USER_PREFIX="/baidu/galaxy/users"
QUOTA_PREFIX="/baidu/galaxy/quotas"
