#include "master/http_client.h"
#include <boost/shared_ptr.hpp>
#include <iostream>

static volatile bool s_quit = false;
void Callback(boost::shared_ptr< ::baidu::lumia::HttpResponse > resp_ptr) {
    s_quit = true;
    std::cout << resp_ptr->body;
}

int main(int argc, char* args[]) {
    ::baidu::lumia::HttpClient* client = new ::baidu::lumia::HttpClient();
    boost::shared_ptr< ::baidu::lumia::HttpPostRequest> req_ptr(new ::baidu::lumia::HttpPostRequest());
    req_ptr->url="http://galaxyos.baidu.com/dc";
    req_ptr->data.push_back(std::make_pair("data","data"));
    boost::shared_ptr< ::baidu::lumia::HttpResponse> resp_ptr(new ::baidu::lumia::HttpResponse());
    client->AsyncPost(req_ptr, resp_ptr, Callback);
    while (!s_quit) {
        sleep(2);
    } 
    return 0;
}
