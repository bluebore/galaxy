# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com
# Date: 2015-03-30
from django.conf import urls

#views
urlpatterns = urls.patterns("console.service.views",
        (r'^list','list_service'),
        (r'^create','create_service'),
        (r'^kill','kill_service'),
        (r'^update','update_service'),
)

