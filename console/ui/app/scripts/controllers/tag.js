'use strict';
// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com
// Date  : 2015-03-31

(function(angular){


angular.module('galaxy.ui.ctrl').controller('TagPageCtrl',function($scope,
                                                                $http,
                                                                $log,
                                                                $interval,
                                                                $location,
                                                                $modal,
                                                                config){
    if (config.masterAddr == null) {
        $location.path('/setup');
    }
    $scope.showAddTagModal = function(){
         var modalInstace = $modal.open({
                templateUrl:'views/addTag.html',
                controller:'AddTagModelCtrl',
                keyboard:false,
                backdrop:'static',
                animation:false,
                size:'bg',
                resolve:{
                tagList:function(){
                    return $scope.tagList;
                  }
                }

        });
    }
    $http.get(config.rootPrefixPath + "tag/list?master="+config.masterAddr)
         .success(function(data){
             $scope.tagList = data.data;
             });
    $scope.selectTag = function(tag){
        $scope.selectedAgent = tag.agents;
    } 

});

angular.module('galaxy.ui.ctrl').controller('AddTagModelCtrl',function($scope,$modalInstance,$http,$route,config,notify,tagList) {
    $scope.tagform = {"tag":"","agentList":""};
    $scope.cancel = function(){
       $modalInstance.dismiss('cancel');
    }
    $scope.ok = function(){
       $http({
            method:"POST",
            url:config.rootPrefixPath + "tag/add?master="+config.masterAddr,
            data:$scope.tagform,
            headers:{'Content-Type': 'application/x-www-form-urlencoded'},
            transformRequest: function(obj) {
                var str = [];
                for(var p in obj){
                   str.push(encodeURIComponent(p) + "=" + encodeURIComponent(obj[p]));
                }
                return str.join("&");
           }
 
           }).success(function(data){
               if(data.status ==0 ){
                   notify({ message:'创建服务成功'} );
                   $scope.cancel();
                   $route.reload();
               }
            }) ;
    }
    $scope.$watch('tagform.tag', function(newVal, oldVal){
           for (var index in tagList) {
               if (tagList[index].tag == newVal ) {
                   $scope.tagform.agentList = tagList[index].agents.join("\n"); 
               }
           } 
    }, true);

});


}(angular));
