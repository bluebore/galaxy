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
          $scope.chartOptions = {
                 animate:1000
          }; 
          $scope.cpuPercent = 0;
          $scope.memPercent = 0;
          $scope.pageSize = 10;
          $scope.totalItems = 0;
          $scope.currentPage = 1;
          var get_status = function(){
               $http.get(config.rootPrefixPath + "cluster/status?master="+config.masterAddr)
                  .success(function(data){
                      if(data.status == 0){
                          $scope.allMachineList = data.data.machinelist;
                          $scope.totalItems = $scope.allMachineList.length;
                          var startIndex = ($scope.currentPage - 1) * $scope.pageSize;
                          var endIndex = startIndex + $scope.pageSize;
                          if(endIndex > $scope.totalItems){
                             endIndex = $scope.totalItems;
                          }
                          $scope.machineList = $scope.allMachineList.slice(startIndex, endIndex);
                          $scope.total_node_num = data.data.total_node_num;
                          $scope.total_cpu_num = data.data.total_cpu_num;
                          $scope.total_cpu_allocated = data.data.total_cpu_allocated;
                          $scope.total_mem_allocated = data.data.total_mem_allocated;
                          $scope.total_mem_num = data.data.total_mem_num;
                          $scope.total_task_num = data.data.total_task_num;
                          $scope.total_mem_used = data.data.total_mem_used;
                          $scope.total_cpu_used = data.data.total_cpu_used;
                          $scope.cpuPercent = data.data.cpu_usage_p;
                          $scope.memPercent = data.data.mem_usage_p;
                      }else{
                      
                      }
                  })
             .error(function(){})
           }
           get_status();
           $scope.pageChanged = function() {
                  $scope.totalItems = $scope.allMachineList.length;
                  var startIndex = ($scope.currentPage - 1) * $scope.pageSize;
                  var endIndex = startIndex + $scope.pageSize;
                  if(endIndex > $scope.totalItems){
                     endIndex = $scope.totalItems;
                  }
                  $scope.machineList = $scope.allMachineList.slice(startIndex, endIndex);

            };
            $scope.setPage = function (pageNo) {
                $scope.currentPage = pageNo;
            };
           $scope.searchText = "";
           $scope.filterMachine = function(){

               if ($scope.searchText == null || $scope.searchText == "") {
                   return;
               }
               var filteredMachineList = new Array;
               for (var index in $scope.allMachineList ){

                   if ($scope.allMachineList[index].addr.indexOf($scope.searchText) >= 0) {
                       filteredMachineList.push($scope.allMachineList[index]);
                   }
               }
               $scope.pageSize = 10;
               $scope.totalItems = filteredMachineList.length;
               $scope.currentPage = 1;
               var startIndex = ($scope.currentPage - 1) * $scope.pageSize;
               var endIndex = startIndex + $scope.pageSize;
               if(endIndex > $scope.totalItems){
                    endIndex = $scope.totalItems;
               }

               $scope.machineList = filteredMachineList.slice(startIndex, endIndex);
           }

}
);

 
}(angular));
