'use strict';
// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com
// Date  : 2015-03-31
(function(angular){
angular.module('galaxy.ui.ctrl')
       .controller('ClusterCtrl',function($scope,
                                          $modal,
                                          $http,
                                          $routeParams,
                                          $log,
                                          $interval,
                                          notify,
                                          config,
                                          $location){
           if(config.masterAddr == null ){
              $location.path('/setup');
              return;
           }
          $scope.listTaskByAgent = function(agent){
                 var modalInstance = $modal.open({
                 templateUrl: 'views/task.html',
                 controller: 'TaskForAgentCtrl',
                 keyboard:false,
                 size:'lg',
                 backdrop:'static',
                 resolve:{
                    agent:function(){
                      return agent;
                 }
             }
           });
          }  
          $scope.cpuUsage = 0;
          $scope.memUsage = 0;
          $scope.machineList = [];
          var get_status = function(){
               $http.get("/console/cluster/status?master="+config.masterAddr)
                  .success(function(data){
                      if(data.status == 0){
                          $scope.machineList = data.data.machinelist;
                          $scope.total_node_num = data.data.total_node_num;
                          $scope.total_cpu_num = data.data.total_cpu_num;
                          $scope.total_cpu_allocated = data.data.total_cpu_allocated;
                          $scope.total_mem_allocated = data.data.total_mem_allocated;
                          $scope.total_mem_num = data.data.total_mem_num;
                          $scope.total_task_num = data.data.total_task_num;
                          $scope.total_mem_used = data.data.total_mem_used;
                          $scope.total_cpu_used = data.data.total_cpu_used;
                          $scope.cpuUsage = data.data.cpu_usage_p;
                          $scope.memUsage = data.data.mem_usage_p;
                      }else{
                      
                      }
                  })
             .error(function(){})
           }
           get_status();
           $interval(get_status,5000);
    
}
)
 
}(angular));
