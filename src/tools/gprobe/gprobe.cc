// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "protocol/galaxy.pb.h"
#include "protocol/agent.pb.h"
#include "util/error_code.h"
#include "util/util.h"
#include "rpc/rpc_client.h"
#include "boost/smart_ptr/shared_ptr.hpp"
#include "pod_metrix.h"
#include "agent/util/output_stream_file.h"
#include <boost/scoped_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem/operations.hpp>

#include <gflags/gflags.h>

#include <unistd.h>


#include <iostream>

DEFINE_string(p, "", "used to decalre port of agent");
DEFINE_string(i, "", "used to declare pod id");
DEFINE_string(w, "", "dir to write");
DEFINE_string(a, "", "dir to append");
DEFINE_string(h, "", "to print help");
DEFINE_string(e, "", "endpoint");
DEFINE_int32(t, -1, "");

baidu::galaxy::util::ErrorCode CheckParameter();
void PrintHelp(const char* argv0);

void ParseInfo(const baidu::galaxy::proto::QueryResponse& qres, 
            std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> >& metrix);

void WriteMetrixesToFiles(std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> >& metrix,
        const std::string& dir_path,
        bool append);
void PrintMetrixes(std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> >& metrix);
 
int main(int argc, char** argv) {
    if (argc <= 1) {
        PrintHelp(argv[0]);
        return -1;
    }

    google::ParseCommandLineFlags(&argc, &argv, true);
    baidu::galaxy::util::ErrorCode ec = CheckParameter();

    if (0 != ec.Code()) {
        std::cerr << "check parameter failed: " << ec.Message() << std::endl;
        return -1;
    }

    if (!FLAGS_a.empty() || !FLAGS_w.empty()) {
        std::string path = !FLAGS_a.empty() ? FLAGS_a : FLAGS_w;
        boost::system::error_code ec;
        if (!boost::filesystem::exists(path)
                    && !baidu::galaxy::file::create_directories(path, ec)) {
            std::cerr << "create directories failed:" << ec.message() << std::endl;
            return -1;
        }
    }

    boost::scoped_ptr<baidu::galaxy::RpcClient> rpc(new baidu::galaxy::RpcClient());
    std::string endpoint;
    if (FLAGS_e.empty()) {
        endpoint = "127.0.0.1:" + FLAGS_p;
    } else {
        endpoint = FLAGS_e;
    }

    baidu::galaxy::proto::Agent_Stub* agent_stub = NULL;

    if (!rpc->GetStub(endpoint, &agent_stub)) {
        std::cerr << "get stub failed, endpoint is" << endpoint << std::endl;
        return -1;
    }

    baidu::galaxy::proto::QueryRequest qr;
    qr.set_full_report(true);
    int64_t last_output_time = baidu::common::timer::get_micros();

    while (true) {
        last_output_time = baidu::common::timer::get_micros();
        baidu::galaxy::proto::QueryResponse qres;

        if (rpc->SendRequest(agent_stub, 
                        &baidu::galaxy::proto::Agent_Stub::Query,
                        &qr, 
                        &qres,
                        5,
                        1)) {
            //std::cerr << qres.DebugString() << std::endl;
            std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> > metrix;
            ParseInfo(qres, metrix);

            if (!FLAGS_w.empty()) {
                WriteMetrixesToFiles(metrix, FLAGS_w, false);
            } else if (!FLAGS_a.empty()) {
                WriteMetrixesToFiles(metrix, FLAGS_a, true);
            } else {
                PrintMetrixes(metrix);
            }
        }

        if (FLAGS_t <= 0) {
            break;
        } else {
            int64_t now = baidu::common::timer::get_micros();
            int64_t deta = now + FLAGS_t * 1000000L - last_output_time;
            if (deta > 0) {
                sleep(deta / 1000000L);
            }
        }
    }

    return 0;
}

baidu::galaxy::util::ErrorCode CheckParameter() {
    if (FLAGS_p.empty() && FLAGS_e.empty()) {
        return ERRORCODE(-1, "endpoint is not set");
    }

    if (!FLAGS_p.empty() && !FLAGS_e.empty()) {
        return ERRORCODE(-1, "");
    }

    return ERRORCODE_OK;
}

void PrintHelp(const char* argv0) {
    std::cout << "usage: " << argv0 << " -p port [ -i ] [ -w ] [ -a ] [ -e ]" << std::endl;
}

void ParseInfo(const baidu::galaxy::proto::QueryResponse& qres, std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> >& metrix) {
    const baidu::galaxy::proto::AgentInfo& ai = qres.agent_info();

    std::map<std::string, int> cinfs; // index for container info
    for (int k = 0; k < ai.container_info_size(); k++) {
        cinfs[ai.container_info(k).id()] = k;
    }

    for (int k = 0; k < ai.container_info_size(); k++) {
        const baidu::galaxy::proto::ContainerInfo& cinf = ai.container_info(k);
        const baidu::galaxy::proto::ContainerDescription& cdes = cinf.container_desc();

        boost::shared_ptr<baidu::galaxy::tools::PodMetrix> pm(new baidu::galaxy::tools::PodMetrix(cinf.id()));
        // limit
        int64_t cpu_limit = 0L;
        int64_t memory_limit = 0L;
        int64_t tcp_recv_limit = 0L;
        int64_t tcp_send_limit = 0L;

        for (int i = 0; i < cdes.cgroups_size(); i++) {
            const baidu::galaxy::proto::Cgroup cg = cdes.cgroups(i);
            cpu_limit += cg.cpu().milli_core();
            memory_limit += cg.memory().size();
            tcp_recv_limit += cg.tcp_throt().recv_bps_quota();
            tcp_send_limit += cg.tcp_throt().send_bps_quota();
        }

        pm->cpu_limit_in_millicore = cpu_limit;
        pm->memory_limit_in_byte = memory_limit;
        // used
        pm->rss_used_in_byte = cinf.memory_used();
        pm->cpu_used_in_millicore = cinf.cpu_used();
        // volum

        std::map<std::string, int64_t> volume_total;
        for (int i = 0; i < cdes.data_volums_size(); i++) {
            volume_total[cdes.data_volums(i).dest_path()] = cdes.data_volums(i).size();
        }
        volume_total[cdes.workspace_volum().dest_path()] = cdes.workspace_volum().size();

        for (int i = 0; i < cinf.volum_used_size(); i++) {
            const baidu::galaxy::proto::Volum& vs = cinf.volum_used(i);
            boost::shared_ptr<baidu::galaxy::tools::PodMetrix::Volum> v(new baidu::galaxy::tools::PodMetrix::Volum);
            v->path = vs.path();
            v->used_in_byte = vs.used_size();
            
            std::map<std::string, int64_t>::const_iterator iter = volume_total.find(v->path);
            assert(iter != volume_total.end());
            v->total_in_byte = iter->second;
            pm->volums.push_back(v);
        }

        // parse volum jobs
        // create volum index
        std::map<std::string, int64_t> depend_volum_total_size;

        for (int i = 0; i < cdes.volum_containers_size(); i++) {
             const std::string& vj = cdes.volum_containers(i);
             std::map<std::string, int>::const_iterator iter = cinfs.find(vj);
             assert(iter != cinfs.end());
             assert(iter->second < ai.container_info_size());
             const baidu::galaxy::proto::ContainerInfo& vci 
                 = ai.container_info(iter->second); //volum container   info            

             for (int j = 0; j < vci.container_desc().data_volums_size(); j++) {
                 depend_volum_total_size[vci.container_desc().data_volums(j).dest_path()] 
                     = vci.container_desc().data_volums(j).size(); 
             }
             depend_volum_total_size[vci.container_desc().workspace_volum().dest_path()]
                 = vci.container_desc().workspace_volum().size();
        }

        //
        for (int i = 0; i < cdes.volum_containers_size(); i++) {
            const std::string& vj = cdes.volum_containers(i);
            std::map<std::string, int>::const_iterator iter = cinfs.find(vj);

            if (iter != cinfs.end()) {
                assert(iter->second < ai.container_info_size());

                const baidu::galaxy::proto::ContainerInfo& vci = ai.container_info(iter->second); //volum container info

                for (int i = 0; i < vci.volum_used_size(); i++) {
                    const baidu::galaxy::proto::Volum& vs = vci.volum_used(i);
                    boost::shared_ptr<baidu::galaxy::tools::PodMetrix::Volum> v(new baidu::galaxy::tools::PodMetrix::Volum);
                    v->path = vs.path();
                    v->used_in_byte = vs.used_size();
                    std::map<std::string, int64_t>::const_iterator it = depend_volum_total_size.find(v->path);
                    assert(it != depend_volum_total_size.end());
                    v->total_in_byte = it->second;
                    pm->volums.push_back(v);
                }
            }
        }

        metrix[cinf.id()] = pm;
    }
}

void WriteMetrixesToFiles(std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> >& metrix,
        const std::string& dir_path,
        bool append) {
    std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> >::const_iterator iter = metrix.begin();

    while (iter != metrix.end()) {
        if (!FLAGS_i.empty() && FLAGS_i != iter->first) {
            iter++;
            continue;
        }

        boost::shared_ptr<baidu::galaxy::file::OutputStreamFile> osf;
        std::string mode = append ? "a+" : "w";
        std::string path = dir_path + "/" + iter->first;
        osf.reset(new baidu::galaxy::file::OutputStreamFile(path, mode));

        if (!osf->IsOpen()) {
            std::cerr << "open file failed: " << path << " " << osf->GetLastError().Message() << std::endl;
        } else {
            std::string str = iter->second->ToString();
            size_t size = str.size();
            osf->Write(str.c_str(), size);
        }

        iter++;
    }
}

void PrintMetrixes(std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> >& metrix) {
    std::map<std::string, boost::shared_ptr<baidu::galaxy::tools::PodMetrix> >::const_iterator iter = metrix.begin();

    while (iter != metrix.end()) {
        if (!FLAGS_i.empty() && FLAGS_i != iter->first) {
            iter++;
            continue;
        }

        fprintf(stdout, "pod_id is: %s\n", iter->first.c_str());
        fprintf(stdout, "%s", iter->second->ToString().c_str());
        fflush(stdout);
        iter++;
    }
}
