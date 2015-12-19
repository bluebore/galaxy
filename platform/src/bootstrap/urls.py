# -*- coding:utf-8 -*-
from django.conf import urls
from django.contrib import admin
urlpatterns = urls.patterns('bootstrap.views',
     (r'^$', 'index'),
)


urlpatterns += urls.patterns('',
     (r'^minion/', urls.include('minion.urls')),
)

urlpatterns += urls.patterns('',
     (r'^trace/', urls.include('trace.urls')),
)

urlpatterns += urls.patterns('',
     (r'^dc/', urls.include('dc.urls')),
)

