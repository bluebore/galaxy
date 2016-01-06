// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "utils/trace.h"

#include <gflags/gflags.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "timer.h"
#include "logger.h"
#include <pthread.h>
#include <stdlib.h>

DECLARE_string(trace_conf);
DECLARE_bool(enable_trace);
DECLARE_string(agent_ip);


namespace baidu {
namespace galaxy {

static boost::uuids::random_generator gen;
void Trace::Init() {
}

std::string Trace::GenUuid() {
    return boost::lexical_cast<std::string>(gen()); 
}

void Trace::Log(::google::protobuf::Message& msg, const std::string& pk) {
    if (FLAGS_enable_trace) {
        const ::google::protobuf::Descriptor* descriptor = msg.GetDescriptor();
        int64_t now = ::baidu::common::timer::get_micros();
        mdt::LogProtoBuf(pk, &msg);
        int64_t consumed = (::baidu::common::timer::get_micros() - now)/1000;
        LOG(INFO, "save %s to trace consumed %d ms", descriptor->full_name().c_str(), consumed);
    }
}

void Trace::TraceClusterStat(ClusterStat& stat) {
    stat.set_id(GenUuid());
    Trace::Log(stat, "id");
}

void Trace::TraceAgentEvent(AgentEvent& e) {
    e.set_id(GenUuid());
    Trace::Log(e,"addr");
}

void Trace::TraceJobEvent(const TraceLevel& level,
                          const std::string& action,
                          const Job* job) {
    if (job == NULL) {
        return;
    }
    JobEvent e;
    e.set_id(GenUuid());
    e.set_level(level);
    e.set_action(action);
    e.set_jobid(job->id_);
    std::string desc;
    job->desc_.SerializeToString(&desc);
    e.set_desc(desc);
    e.set_version(job->latest_version);
    e.set_name(job->desc_.name());
    e.set_state(job->state_);
    e.set_update_state(job->update_state_);
    e.set_time(::baidu::common::timer::get_micros());
    Trace::Log(e, "id");
}

void Trace::TraceJobStat(const Job* job) {
    if (job == NULL) {
        return;
    }
    JobStat job_stat;
    job_stat.set_id(GenUuid());
    job_stat.set_jobid(job->id_);
    int32_t running = 0;
    int32_t pending = 0;
    int32_t death = 0;
    std::map<Version, PodDescriptor>::const_iterator pod_desc_it = job->pod_desc_.find(job->latest_version);
    if (pod_desc_it !=  job->pod_desc_.end()) {
        job_stat.set_cpu_quota(job->desc_.replica() * pod_desc_it->second.requirement().millicores());
        job_stat.set_mem_quota(job->desc_.replica() * pod_desc_it->second.requirement().memory());
    }
    int64_t cpu_used = 0;
    int64_t mem_used = 0;
    job_stat.set_time(::baidu::common::timer::get_micros());
    job_stat.set_dynamic_cpu_quota(0);
    job_stat.set_dynamic_mem_quota(0);
    std::map<PodId, PodStatus*>::const_iterator it = job->pods_.begin();
    for (; it != job->pods_.end(); ++it) {
        PodStatus* pod = it->second;
        cpu_used += pod->resource_used().millicores();
        mem_used += pod->resource_used().memory();
        if (pod->stage() == kStageRunning) {
            ++running;
        }else if (pod->stage() == kStagePending) {
            ++pending;
        }else {
            ++death;
        }
    }
    job_stat.set_replica(job->desc_.replica());
    job_stat.set_running(running);
    job_stat.set_pending(pending);
    job_stat.set_mem_used(mem_used);
    job_stat.set_cpu_used(cpu_used);
    job_stat.set_death(death);
    Trace::Log(job_stat, "id");
}

void Trace::TraceTaskEvent(const TraceLevel& level,
                           const TaskInfo* task,
                           const std::string& err,
                           const TaskState& status,
                           bool internal_error,
                           int32_t exit_code) {
    TaskEvent e;
    e.set_id(GenUuid());
    e.set_level(level);
    e.set_pod_id(task->pod_id);
    e.set_job_id(task->job_id);
    e.set_agent_addr(FLAGS_agent_ip);
    e.set_initd_addr(task->initd_endpoint);
    e.set_state(status);
    e.set_stage(task->stage);
    e.set_ctime(task->ctime);
    e.set_internal_error(internal_error);
    e.set_ttime(::baidu::common::timer::get_micros());
    e.set_task_chroot_path(task->task_chroot_path);
    e.set_error(err);
    e.set_exit_code(exit_code);
    e.set_cmd(task->desc.start_command());
    e.set_deploy(task->deploy_process.status());
    e.set_main(task->main_process.status());
    e.set_gc_dir(task->gc_dir);
    Trace::Log(e, "id");
}

void Trace::TracePodEvent(const TraceLevel& level,
                          const PodStatus* pod,
                          const std::string& from,
                          const std::string& reason) {
    if (pod == NULL) {
        return;
    }
    PodEvent e;
    e.set_level(level);
    e.set_time(::baidu::common::timer::get_micros());
    e.set_id(GenUuid());
    e.set_podid(pod->podid());
    e.set_jobid(pod->jobid());
    e.set_stage(pod->stage());
    e.set_state(pod->state());
    e.set_from(from);
    e.set_reason(reason);
    e.set_agent_addr(pod->endpoint());
    e.set_gc_dir(pod->pod_gc_path());
    e.set_version(pod->version());
    Trace::Log(e, "id");
}

void Trace::TracePodStat(const PodStatus* pod, int32_t cpu_quota, int64_t mem_quota) {
    if (pod == NULL) {
        return;
    }
    PodStat stat;
    stat.set_id(GenUuid());
    stat.set_podid(pod->podid());
    stat.set_jobid(pod->jobid());
    stat.set_cpu_quota(cpu_quota);
    stat.set_mem_quota(mem_quota);
    stat.set_read_bytes_ps(pod->resource_used().read_bytes_ps());
    stat.set_write_bytes_ps(pod->resource_used().write_bytes_ps());
    stat.set_syscr_ps(pod->resource_used().syscr_ps());
    stat.set_syscw_ps(pod->resource_used().syscw_ps());
    stat.set_cpu_used(pod->resource_used().millicores());
    stat.set_mem_used(pod->resource_used().memory());
    stat.set_time(::baidu::common::timer::get_micros());
    Trace::Log(stat, "id");
}


}
}
