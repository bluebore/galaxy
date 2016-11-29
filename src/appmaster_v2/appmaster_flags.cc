#include <gflags/gflags.h>
#include "agent/util/error_code.h"


DEFINE_string(nexus_root, "", "root prefix on nexus");
DEFINE_string(nexus_addr, "", "nexus server list");
DEFINE_string(appmaster_lock, "", "lock point");
DEFINE_string(appmaster_addr, "", "");
DEFINE_string(resman_addr, "", "");

DEFINE_string(job_meta_path, "", "appmaster jobs store path");
DEFINE_string(appmaster_port, "1647", "appmaster listen port");
DEFINE_string(appmaster_ip, "", "appmaster host ip");
DEFINE_string(appworker_cmdline, "", "appworker default cmdline");

DEFINE_int32(appmaster_check_deadpod_interval, 10, "[sec] the interval job tracker will check if pod is alive or not");
DEFINE_int32(appmaster_check_deploy_interval, 10, "[sec] the interval job tracker will check if updating step is achived");
DEFINE_int32(master_pod_dead_threshhold, 60, "[sec] master pod overtime threshold");

DEFINE_int32(master_fail_last_threshold, 3600, "master pod fail status lasts time threshold");
DEFINE_int32(safe_interval, 20, "master safe mode interval");
DEFINE_int32(deploying_pod_timeout, 60, "deploy timeout in sec");


#define CHECK_EMPTY(flag) do {\
    if (FLAGS_##flag.empty()) { \
        return ERRORCODE(-1, "--%s should be specified", #flag); \
    } \
} while(0)

baidu::galaxy::util::ErrorCode CheckCommandLine() {
    CHECK_EMPTY(nexus_root);
    CHECK_EMPTY(nexus_addr);
    CHECK_EMPTY(appmaster_lock);
    CHECK_EMPTY(appmaster_addr);
    CHECK_EMPTY(resman_addr);
    CHECK_EMPTY(job_meta_path);
    CHECK_EMPTY(appmaster_ip);
    CHECK_EMPTY(appmaster_port);
    return ERRORCODE_OK;
}
