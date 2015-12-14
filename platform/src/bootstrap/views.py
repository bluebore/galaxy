from common import http
def index(request):
    response_builder = http.ResponseBuilder()
    return response_builder.add_params({})\
                           .add_req(request)\
                           .build_tmpl("index.html")
