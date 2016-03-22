#include "trace_server.h"

#include <gflags/gflags.h>
#include <sofa/pbrpc/pbrpc.h>
#include <iostream>

DECLARE_string(agent_port);

int main(int argc, char** argv) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    
    baidu::galaxy::trace::TraceServer* service = new baidu::galaxy::trace::TraceServer();
    if (0 != service->Setup()) {
        //LOG(FATAL) << "setup trace server failed";
        std::cout << "setup trace server failed" << std::endl;
        exit(-1);
    }
    //LOG(INFO) << "set up trace server successfully";
    
    rpc_server.RegisterService(service);
    std::string service_host = std::string("0.0.0.0:8888");// + FLAGS_agent_port;
    if (!rpc_server.Start(service_host)) {
        std::cout << "start trace server failed" << std::endl;
        //LOG(FATAL) << "setup trace server failed:" << service_host;
    }
    std::cout << "start trace server sunccessfully" << std::endl;
    //LOG(INFO) << "start rpc service successfully: " << service_host;
    rpc_server.Run();
    service->Teradown();
    delete service;
    return 0;
}
