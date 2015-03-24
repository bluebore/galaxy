/*
 * test_task.cc
 * Copyright (C) 2015 wangtaize <wangtaize@baidu.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "agent/task_runner.h"
#include "proto/master.pb.h"
#include <stdio.h>
int main(){
    galaxy::TaskInfo task_info;
    task_info.set_task_name(9999999);
    task_info.set_cmd_line("python -m SimpleHTTPServer 8156");
    galaxy::agent::CommandTaskRunner runner(&task_info);
    runner.Start();
    if(runner.IsRunning() == 0){
        printf("task is running");
    }
    return 0;
}





