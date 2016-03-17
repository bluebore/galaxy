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
DEFINE_string(data_center, "yq01", "the data center of galaxy");

// master
DEFINE_string(master_port, "7828", "Master service listen port");
DEFINE_int32(master_agent_timeout, 40000, "Agent timeout");
DEFINE_int32(master_agent_rpc_timeout, 10, "Agent RPC timeout (seconds)");
DEFINE_int32(master_query_period, 30000, "Query period");
DEFINE_int32(master_job_trace_interval, 60000, "the period of trace job state");
DEFINE_int32(master_cluster_trace_interval, 60000, "the period of trace job state");
DEFINE_int32(master_preempt_interval, 2000, "the period of preempt");
DEFINE_string(master_lock_path, "/master_lock", "master lock name on nexus");
DEFINE_string(master_path, "/master", "master path on nexus");
DEFINE_string(safemode_store_path, "/safemode", "master safemode path on nexus");
DEFINE_string(safemode_store_key, "safemode_switch", "master safemode key on nexus");
DEFINE_string(jobs_store_path, "/jobs", "");
DEFINE_string(agents_store_path, "/agents", "");
DEFINE_string(labels_store_path, "/labels", "");
DEFINE_int32(max_need_update_job_size, 10, "the max size of need update job size ");
DEFINE_int32(max_scale_down_size, 10, "the max size of scale down jobs that schedule fetches");
DEFINE_int32(max_scale_up_size, 10, "the max size of scale up jobs that schedule fetches");
DEFINE_int32(master_pending_job_wait_timeout, 1000, "the timeout that master pending scheduler request");
// scheduler* (CPU_CFS_PERIOD / 1000)
DEFINE_int32(scheduler_get_pending_job_timeout, 2000, "the timeout that scheduler get pending job from master");
DEFINE_int32(scheduler_get_pending_job_period, 2000, "the period that scheduler get pending job from master");
DEFINE_int32(scheduler_sync_resource_timeout, 1000, "the timeout that scheduler sync resource from master");
DEFINE_int32(scheduler_sync_resource_period, 2000, "the period that scheduler sync resource from master");
DEFINE_int32(scheduler_sync_job_period, 2000, "the period that scheduler sync job from master");
DEFINE_int32(scheduler_sync_job_timeout, 2000, "the time that scheduler sync job from master");
DEFINE_int32(scheduler_feasibility_factor, 2, "the feasibility factor which schedulder use to calc agent for pending job");

DEFINE_double(scheduler_cpu_used_factor, 10.0, "the cpu used factor for calc agent load score");
DEFINE_double(scheduler_mem_used_factor, 1.0, "the mem used factor for calc agent load score");
DEFINE_double(scheduler_prod_count_factor, 32.0, "the prod count factor for calc agent load score");
DEFINE_double(scheduler_cpu_overload_threashold, 0.9, "the max cpu used");
DEFINE_int32(scheduler_agent_overload_turns_threashold, 3, "agent overload times");

// agent
DEFINE_string(agent_port, "8080", "agent listen port");
DEFINE_int32(agent_http_port, 8222, "agent http server port");
DEFINE_int32(agent_background_threads_num, 2, "agent background threads");
DEFINE_int32(agent_heartbeat_interval, 1000, "agent haertbeat interval ms");
DEFINE_string(agent_ip, "127.0.0.1", "agent host ip");
DEFINE_int32(agent_millicores_share, 15000, "agent millicores");
DEFINE_int32(agent_download_package_timeout, 20, "agent download package timeout in seconds");
DEFINE_int32(agent_download_package_retry_times, 10, "agent download package retry times");
DEFINE_int64(agent_mem_share, 68719476736, "agent memory");
DEFINE_string(agent_initd_bin, "./initd", "initd bin path");
DEFINE_string(agent_work_dir, "./work_dir", "the work dir for storaging job package and runtime dir");
DEFINE_string(agent_gc_dir, "./gc_dir", "the gc dir for pod env");
DEFINE_string(agent_coredump_dir, "./core_dir", "the core dump dir for pod env");
DEFINE_int64(agent_gc_timeout, 1000 * 60 * 60 * 5, "gc timeout");
DEFINE_bool(agent_namespace_isolation_switch, false, "agent namespace isolate");

DEFINE_int32(agent_monitor_pods_interval, 10, "agent monitor pods interval, unit seconds");
DEFINE_int32(agent_trace_pod_interval, 30000, "agent trace pod stat interval");
DEFINE_int32(agent_rpc_initd_timeout, 2, "agent monitor initd interval, unit seconds");
DEFINE_int32(agent_initd_port_begin, 9000, "agent initd port used begin");
DEFINE_int32(agent_initd_port_end, 9500, "agent initd port used end");
DEFINE_string(agent_persistence_path, "./data", "agent persistence path");
// hard limit
DEFINE_string(agent_global_hardlimit_path, "galaxy", "agent cpu hard limit root");
// soft limit
DEFINE_string(agent_global_softlimit_path, "softlimit", "agent cpu soft limit root");
DEFINE_string(agent_global_cgroup_path, "galaxy", "agent cgroup global path");
DEFINE_int32(agent_detect_interval, 1000, "agent detect process running interval");
DEFINE_int32(agent_io_collect_interval, 1000, "agent collect io interval");
DEFINE_int32(agent_memory_check_interval, 200, "agent check task memory interval");
DEFINE_bool(agent_use_galaxy_oom_killer, false, "use galaxy oom killer default false, use cgroup oom killer");
DEFINE_string(agent_default_user, "galaxy", "agent default run task user");
DEFINE_int32(send_bps_quota, 200000000, "galaxy net send limit");
DEFINE_int32(recv_bps_quota, 200000000, "galaxy new recv limit");

// gce
DEFINE_string(gce_cgroup_root, "/cgroups/", "Cgroup root mount path");
DEFINE_string(gce_support_subsystems, "", "Cgroup default support subsystems");
DEFINE_int64(gce_initd_zombie_check_interval, 100, "Initd Zombie Check Interval");
DEFINE_string(gce_initd_dump_file, "initd_checkpoint_file", "Initd Checkpoint File Name");
DEFINE_string(gce_initd_port, "8765", "gce initd listen port");
DEFINE_string(gce_bind_config, "", "gce mount bind config");
DEFINE_int32(cli_server_port, 8775, "cli server listen port");

DEFINE_string(trace_conf, "", "the conf of trace");
DEFINE_bool(enable_trace, false, "open trace");
DEFINE_bool(enable_resource_minitor, false, "open resource monitor");
DEFINE_int32(stat_check_period, 1000, "agent check health period(ms)");
DEFINE_int32(agent_recover_threshold, 3600, "threshold for agent recover from offline(stat times)");
DEFINE_double(max_cpu_usage, 0.8, "max cpu usage");
DEFINE_double(max_mem_usage, 0.8, "max mem usage");
DEFINE_double(max_disk_r_kbps, 0.0, "max disk read kbytes per second");
DEFINE_double(max_disk_w_kbps, 0.0, "max disk write kbytes per second");
DEFINE_double(max_disk_r_rate, 0.0, "max disk read times per second");
DEFINE_double(max_disk_w_rate, 0.0, "max disk read times per second");
DEFINE_double(max_disk_util, 0.0, "max disk io util");
DEFINE_double(max_net_in_bps, 0.0, "max net in bits per second");
DEFINE_double(max_net_out_bps, 0.0, "max net out bits per second");
DEFINE_double(max_net_in_pps, 0.0, "max net in packets per second");
DEFINE_double(max_net_out_pps, 0.0, "max net out packets per second");
DEFINE_double(max_intr_rate, 0.0, "max interupt times per second");
DEFINE_double(max_soft_intr_rate, 0.0, "max soft interupt times per second");
DEFINE_int32(max_ex_time, 0, "max exception time");
