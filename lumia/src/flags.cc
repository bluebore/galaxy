// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <gflags/gflags.h>

DEFINE_string(lumia_ctrl_port, "8080", "lumia controller port");
DEFINE_string(lumia_ctrl_host, "0.0.0.0", "lumia controller host");
DEFINE_string(minion_dict, "dict/minion.pb.dict", "minion dict");
DEFINE_string(scripts_dir, "scripts", "scripts dir");

DEFINE_string(rms_api_http_host, "xxx", "rms api http host");
DEFINE_string(ccs_api_http_host, "xxx", "ccs api http host");

DEFINE_string(rms_api_check_job, "xxx", "rms check job status api");
DEFINE_string(rms_token, "b4631c7cb697d9ee7080126b18dd09abcfad79eb", "rms token");
DEFINE_string(rms_app_key, "112", "rms app key");
DEFINE_string(rms_auth_user, "wangtaize", "rms auth user");
DEFINE_int32(exec_job_check_interval, 2000, "exec job check interval");

DEFINE_string(nexus_servers, "", "server list of nexus, e.g abc.com:1234,def.com:5342");

DEFINE_string(lumia_root_path, "/baidu/lumia", "root path of lumia cluster on nexus, e.g /baidu/lumia");

DEFINE_string(lumia_lock, "/lock", "root path of lumia lock on nexus, e.g /lock");
DEFINE_string(lumia_main, "/main", "the path of lumia main ");
DEFINE_string(lumia_minion, "/minion", "the path of lumia minion ");
DEFINE_string(lumia_script, "/script", "the path of lumia script ");

DEFINE_string(lumia_agent_smartctl_bin_path, "./bin/smartctl", "the path of smartctl path");
DEFINE_string(lumia_agent_port, "8123", "the port of lumia agent");
DEFINE_string(lumia_agent_ip, "127.0.0.1", "the ip of lumia agent");
DEFINE_string(lumia_agent_cgroups, "/cgroups", "the sys cgroup root");
DEFINE_string(lumia_agent_cpu_root_group, "/cgroup/cpu/lumia", "the lumia cpu cgroup root");
DEFINE_string(lumia_agent_mem_root_group, "/cgroup/memory/lumia", "the lumia mem cgroup root");
DEFINE_string(lumia_agent_workspace, "/cgroup/memory/lumia", "the lumia mem cgroup root");
