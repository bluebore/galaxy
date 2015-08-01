#include "gflags/gflags.h"
#include "sofa/pbrpc/pbrpc.h"
#include "proto/initd.pb.h"
#include "rpc/rpc_client.h"

int main(int argc, char** argv) {

    baidu::galaxy::Initd_Stub* initd;
    baidu::galaxy::RpcClient* rpc_client = 
        new baidu::galaxy::RpcClient();
    rpc_client->GetStub("localhost:8765", &initd);


    baidu::galaxy::ExecuteRequest exec_request;
    exec_request.set_key("dapigu");
    exec_request.set_commands("touch dapigu");
    exec_request.set_path(".");

    baidu::galaxy::ExecuteResponse exec_response;

    rpc_client->SendRequest(initd, 
            &baidu::galaxy::Initd_Stub::Execute, 
            &exec_request, 
            &exec_response, 5, 1);

    return 0;
}
