#!/usr/bin/env sh
# NOTE ftp path
../output/bin/galaxy_client add --master_addr=localhost:8102 --job_name=1234  \
                           --cpu_soft_limit=1  \
                           --cpu_limit=1 --mem_gbytes=1 \
                           --replicate_num=4 \
                           --task_raw=task.sh.tar.gz \
                           --cmd_line="sh task.sh" \
                           --one_task_per_host
