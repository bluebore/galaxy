# -*- coding:utf-8 -*-
from django.conf import urls
from django.contrib import admin

urlpatterns = urls.patterns('minion.views',
     (r'^overview', 'get_overview'),
     (r'^detail', 'get_detail'),
     (r'^status', 'get_status'),
     (r'^report', 'report'),
     (r'^$', 'index'),
)

