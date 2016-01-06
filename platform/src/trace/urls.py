# -*- coding:utf-8 -*-
from django.conf import urls

urlpatterns = urls.patterns('trace.views',
     (r'^job$', 'job_all'),
     (r'^job/detail', 'job_detail'),
     (r'^squery$', 'squery'),
     (r'^sql$', 'sql'),
     (r'^cluster$', 'cluster'),
     (r'^status$', 'get_real_time_status'),
     (r'^dc$', 'get_total_status'),
)

