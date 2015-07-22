'use strict';
// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com
// Date  : 2015-03-31

(function(angular){


angular.module('galaxy.ui.ctrl').controller('SetPasswordCtrl',function($scope,
                                                                $http,
                                                                $log,
                                                                $interval,
                                                                $location,
                                                                $modal,
                                                                config){
    $scope.showAddTagModal = function(){
         var modalInstace = $modal.open({
                templateUrl:'views/addTag.html',
                controller:'AddTagModelCtrl',
                keyboard:false,
                animation:false,
                backdrop:'static',
                size:'bg',
                resolve:{
                agent:function(){
                    return $scope.agent;
                  }
                }

        });
    }
   

});

angular.module('galaxy.ui.ctrl').controller('SetPasswordModelCtrl',function($scope,$modalInstance,$http,$route,config,notify, agent) {
    $scope.form = {"username":config.user.username,
                      "password":"",
                      "agent":agent};
    $scope.cancel = function(){
       $modalInstance.dismiss('cancel');
    }
    $scope.ok = function(){
       if ($scope.form.passowrd == "") {
           notify({ message:'密码不能为空'} );
           return;
       }

       $http({
            method:"POST",
            url:config.rootPrefixPath + "cluster/setPassword",
            data:$scope.form,
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
                   notify({ message:'设置密码成功'} );
                   $scope.cancel();
               }
            }) ;
    }
   

});


}(angular));
