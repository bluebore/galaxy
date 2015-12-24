#include "master/http_client.h"
#include <boost/shared_ptr.hpp>
#include <iostream>

int main(int argc, char* args[]) {
    ::baidu::lumia::HttpClient* client = new ::baidu::lumia::HttpClient();
    ::baidu::lumia::HttpGetRequest req;
    req.url="http://galaxyos.baidu.com/dc";
    ::baidu::lumia::HttpResponse resp;
    client->SyncGet(req, &resp);
    std::cout << resp.status << std::endl;
    std::cout << resp.body;
    return 0;
}
