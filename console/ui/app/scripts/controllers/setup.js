'use strict';
// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wangtaize@baidu.com
// Date  : 2015-03-31
(function(angular){
angular.module('galaxy.ui.ctrl')
       .controller('SetupCtrl',function($scope,
                                          $modal,
                                          $http,
                                          $routeParams,
                                          $log,
                                          $cookies,
                                          notify,
                                          config){
           $scope.config = config;
           $scope.update = function(){
               $cookies.masterAddr = $scope.config.masterAddr;
               notify({message:"更新成功"});
           }
}
)
 
}(angular));
