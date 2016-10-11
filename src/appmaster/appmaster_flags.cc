#include <gflags/gflags.h>

DEFINE_string(nexus_root, "/galaxy3", "root prefix on nexus");
DEFINE_string(nexus_addr, "", "nexus server list");
DEFINE_string(jobs_store_path, "/jobs", "appmaster jobs store path");
DEFINE_string(appmaster_port, "1647", "appmaster listen port");
DEFINE_string(appworker_cmdline, "", "appworker default cmdline");
DEFINE_int32(master_job_check_interval, 5, "master job checker interval");
DEFINE_int32(master_pod_dead_time, 10, "master pod overtime threshold");
DEFINE_int32(master_pod_check_interval, 5, "master pod checker interval");
DEFINE_int32(master_fail_last_threshold, 3600, "master pod fail status lasts time threshold");
DEFINE_int32(safe_interval, 20, "master safe mode interval");
