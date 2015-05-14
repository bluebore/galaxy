'use strict';
// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com
// Date  : 2015-03-31

(function(angular){


angular.module('galaxy.ui.ctrl').controller('TaskCtrl',function($scope,
                                                                $modalInstance,
                                                                $http,
                                                                $log,
                                                                $interval,
                                                                service,
                                                                config){
    var stop = null;
   $scope.getTask = function(){
      $http.get("/console/taskgroup/status?id="+service.job_id+"&master="+config.masterAddr)
           .success(function(data){
               if(data.status == 0 ){
                  $scope.tasklist = data.data.taskList;
               }
          })
           .error(function(err){
             $log.error('fail to get service '+ $routeParams.serviceName+' status');
           });
    }

   stop = $interval($scope.getTask,500);
   $scope.close =function(){
      if(stop != null){
          $interval.cancel(stop);
      }

       $modalInstance.dismiss('cancel'); 
   }
   
});

angular.module('galaxy.ui.ctrl').controller('TaskForAgentCtrl',function($scope,
                                                                $modalInstance,
                                                                $http,
                                                                $log,
                                                                $interval,
                                                                agent,
                                                                config){
   var stop = null;
   $scope.getTask = function(){
      $http.get("/console/taskgroup/status?agent="+agent.addr+"&master="+config.masterAddr)
           .success(function(data){
               if(data.status == 0 ){
                  $scope.tasklist = data.data.taskList;
               }
          })
           .error(function(err){
             $log.error('fail to get service '+ $routeParams.serviceName+' status');
           });
    }
   stop = $interval($scope.getTask,500);
   $scope.close =function(){
      if(stop != null){
          $interval.cancel(stop);
      }
      $modalInstance.dismiss('cancel'); 
   }
   
});




}(angular));
