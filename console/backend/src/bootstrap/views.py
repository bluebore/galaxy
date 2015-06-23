
from common import http
from common import cas
from django.contrib import auth
from django import shortcuts

@cas.auto_login_required
def index(request):
    response_builder = http.ResponseBuilder()
    return response_builder.add_params({})\
                           .add_req(request)\
                           .build_tmpl("index.html")


def login(request):
    username = request.POST.get('username',None)
    response_builder = http.ResponseBuilder()
    if not username:
        return response_builder.add_params({"error":"username is required"})\
                               .add_req(request)\
                               .build_tmpl("login.html")
    password = request.POST.get('password',None)
    if not password:
        return response_builder.add_params({"error":"password is required"})\
                               .add_req(request)\
                               .build_tmpl("login.html")
    user = auth.authenticate(username=username, password=password)
    if user and user.is_active:
        login(request, user)
        shortcuts.redirect("/")
    else:
        return response_builder.add_params({"error":"user %s does not exist "%username})\
                               .add_req(request)\
                               .build_tmpl("login.html")





