// Copyright (c) 2014, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>

DEFINE_bool(v, false, "show version string");

// nexus
DEFINE_string(nexus_servers, "", "server list of nexus, e.g abc.com:1234,def.com:5342");
DEFINE_string(nexus_root_path, "/baidu/galaxy", "root path of galaxy cluster on nexus, e.g /ps/galaxy");

// sdk
DEFINE_string(master_host, "localhost", "Master service hostname");

// master
DEFINE_string(master_port, "7828", "Master service listen port");
DEFINE_int32(master_agent_timeout, 40000, "Agent timeout");
DEFINE_int32(master_agent_rpc_timeout, 10, "Agent RPC timeout (seconds)");
DEFINE_int32(master_query_period, 30000, "Query period");
DEFINE_string(master_lock_path, "/master_lock", "master lock name on nexus");
DEFINE_string(master_path, "/master", "master path on nexus");
DEFINE_string(jobs_store_path, "/jobs", "");
DEFINE_string(labels_store_path, "/labels", "");
DEFINE_int32(max_need_update_job_size, 10, "the max size of need update job size ");
DEFINE_int32(max_scale_down_size, 10, "the max size of scale down jobs that schedule fetches");
DEFINE_int32(max_scale_up_size, 10, "the max size of scale up jobs that schedule fetches");
DEFINE_int32(master_pending_job_wait_timeout, 1000, "the timeout that master pending scheduler request");
// scheduler
DEFINE_int32(scheduler_get_pending_job_timeout, 2000, "the timeout that scheduler get pending job from master");
DEFINE_int32(scheduler_get_pending_job_period, 2000, "the period that scheduler get pending job from master");
DEFINE_int32(scheduler_sync_resource_timeout, 1000, "the timeout that scheduler sync resource from master");
DEFINE_int32(scheduler_sync_resource_period, 2000, "the period that scheduler sync resource from master");
DEFINE_int32(scheduler_feasibility_factor, 2, "the feasibility factor which schedulder use to calc agent for pending job");

DEFINE_double(scheduler_cpu_used_factor, 10.0, "the cpu used factor for calc agent load score");
DEFINE_double(scheduler_mem_used_factor, 1.0, "the mem used factor for calc agent load score");
DEFINE_double(scheduler_prod_count_factor, 32.0, "the prod count factor for calc agent load score");
DEFINE_double(scheduler_cpu_overload_threashold, 0.9, "the max cpu used");
DEFINE_int32(scheduler_agent_overload_turns_threashold, 3, "agent overload times");

// agent
DEFINE_string(agent_port, "8080", "agent listen port");
DEFINE_int32(agent_background_threads_num, 2, "agent background threads");
DEFINE_int32(agent_heartbeat_interval, 1000, "agent haertbeat interval ms");
DEFINE_string(agent_ip, "127.0.0.1", "agent host ip");
DEFINE_int32(agent_millicores_share, 15000, "agent millicores");
DEFINE_int64(agent_mem_share, 68719476736, "agent memory");
DEFINE_string(agent_initd_bin, "./initd", "initd bin path");
DEFINE_string(agent_work_dir, "./work_dir", "the work dir for storaging job package and runtime dir");
DEFINE_string(agent_gc_dir, "./gc_dir", "the gc dir for pod env");
DEFINE_int64(agent_gc_timeout, 1000 * 60 * 60 * 5, "gc timeout");
DEFINE_bool(agent_namespace_isolation_switch, false, "agent namespace isolate");

DEFINE_int32(agent_monitor_pods_interval, 10, "agent monitor pods interval, unit seconds");
DEFINE_int32(agent_rpc_initd_timeout, 2, "agent monitor initd interval, unit seconds");
DEFINE_int32(agent_initd_port_begin, 9000, "agent initd port used begin");
DEFINE_int32(agent_initd_port_end, 9500, "agent initd port used end");
DEFINE_string(agent_persistence_path, "./data", "agent persistence path");
DEFINE_string(agent_global_cgroup_path, "galaxy", "agent cgroup global path");
DEFINE_int32(agent_detect_interval, 1000, "agent detect process running interval");
DEFINE_string(agent_default_user, "galaxy", "agent default run task user");

DEFINE_int32(cpu_scheduler_intervals, 1000, "agent cpu scheduler intervals");
DEFINE_int32(cpu_scheduler_collector_intervals, 200, "agent cpu scheduler resource collect intervals");
DEFINE_int32(cpu_scheduler_idle_high_limit, 2000, "agent cpu scheduler idle high limit cores");
DEFINE_int32(cpu_scheduler_idle_low_limit, 1000, "agent cpu scheduler idle low limit cores");
DEFINE_int32(cpu_scheduler_guarantee, 1500, "agent cpu scheduler guarantee");
DEFINE_int32(cpu_scheduler_dec, 500, "agent cpu scheduler dec");
DEFINE_bool(cpu_scheduler_switch, false, "agent cpu scheduler switch");
DEFINE_int32(cpu_scheduler_start_frozen_time, 20, "start forzen time, unit second");


// gce
DEFINE_string(gce_cgroup_root, "/cgroups/", "Cgroup root mount path");
DEFINE_string(gce_support_subsystems, "", "Cgroup default support subsystems");
DEFINE_int64(gce_initd_zombie_check_interval, 100, "Initd Zombie Check Interval");
DEFINE_string(gce_initd_dump_file, "initd_checkpoint_file", "Initd Checkpoint File Name");
DEFINE_string(gce_initd_port, "8765", "gce initd listen port");
DEFINE_string(gce_bind_config, "", "gce mount bind config");
