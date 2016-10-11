#include <gflags/gflags.h>

DEFINE_string(resman_port, "1645", "resman listen port");
DEFINE_int64(sched_interval, 50, "scheduling interval (ms)");
DEFINE_int64(container_group_gc_check_interval, 30000, "container group gc check interval (ms)");
DEFINE_string(nexus_root, "/galaxy3", "root prefix on nexus");
DEFINE_string(nexus_addr, "", "nexus server list");
DEFINE_int32(agent_timeout, 30 , "timeout of agent, in seconds");
DEFINE_int32(agent_query_interval , 5, "query interval of agent, in seconds");
DEFINE_int32(container_group_max_replica, 100000, "max replica allowed for one group");
DEFINE_double(safe_mode_percent, 0.85, "when agent alive percent bigger than this, leave safe mode");
DEFINE_bool(check_container_version, false, "by default, AM will handle that");
DEFINE_int32(max_batch_pods, 12, "max batch pods per agent");
