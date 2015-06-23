'use strict';
(function(angular){


/**
 * @ngdoc function
 * @name uidemoApp.controller:MainCtrl
 * @description
 * # MainCtrl
 * Controller of the uidemoApp
 */
angular.module('galaxy.ui.ctrl',[])
  .controller('HomeCtrl', function ($scope,$modal,$http,$route,notify,$log,config,$location) {
     if(config.masterAddr == null ){
         $location.path('/setup');
         return
     }
     $scope.open = function (size) {
      var modalInstance = $modal.open({
        templateUrl: 'views/createService.html',
        controller: 'CreateServiceModalInstanceCtrl',
        keyboard:false,
        backdrop:'static',
        size: size
      });
    };
     $scope.updateService = function (service) {
      var modalInstance = $modal.open({
        templateUrl: 'views/updateService.html',
        controller: 'UpdateServiceModalIntanceCtrl',
        keyboard:false,
        backdrop:'static',
        resolve:{
            service:function(){
                return service;
            }
        }
      });
    };

$http.get(config.rootPrefixPath + "service/list?user=9527&master="+config.masterAddr)
         .success(function(data){
          if(data.status == 0 ){
               $scope.serviceList = data.data;  
          }else{
            notify({ message:'获取服务列表失败',classes:"alert-danger"} );
            $log.error(data.msg);
          }
         }).error(function(data){
            notify({ message:'获取服务列表失败',classes:"alert-danger"} );
            $log.error(data);
     });
    $scope.listTaskByService = function(service){
        var modalInstance = $modal.open({
        templateUrl: 'views/task.html',
        controller: 'TaskCtrl',
        keyboard:false,
        size:'lg',
        backdrop:'static',
        resolve:{
            service:function(){
                return service;
            }
        }
      });

    }
    $scope.listTaskHistory = function(service){
        var modalInstance = $modal.open({
        templateUrl: 'views/history.html',
        controller: 'TaskHistoryCtrl',
        keyboard:false,
        size:'lg',
        backdrop:'static',
        resolve:{
            service:function(){
                return service;
            }
        }
      });

    }
    $scope.listTaskByAgent = function(agent){
        var modalInstance = $modal.open({
        templateUrl: 'views/task.html',
        controller: 'TaskForAgentCtrl',
        keyboard:false,
        backdrop:'static',
        resolve:{
            agent:function(){
                return agent;
            }
        }
      });


    }
    $scope.killService = function(id){
      var promot = $modal.open({
        templateUrl: 'views/promot.html',
        controller: 'PromotCtrl',
        keyboard:false,
        backdrop:'static',
        size: 'sm',
        resolve:{
          message:function(){
            return "确定kill服务？";
          }
        }
      });
      promot.result.then(function(result){
        if(result){
          $http.get(config.rootPrefixPath + "service/kill?id="+id+"&master="+config.masterAddr)
               .success(function(data){
                  if(data.status == 0){
                    notify({ message:'kill服务成功'} );
                    $route.reload();  
                  }else{
                    notify({ message:'kill服务失败',classes:"alert-danger"} );
                    $log.error(data.msg);
                  }
          
                })
               .error(function(data){
                     notify({ message:'kill服务失败',classes:"alert-danger"} );
                     $log.error(data);
               });
        }
      },function(){});
    }
      
  });
angular.module('galaxy.ui.ctrl').controller('UpdateServiceModalIntanceCtrl',function($scope,$modalInstance,$http,$route,config,service,notify){
        $scope.service = service;
        $scope.update = function(){
             $http.get(config.rootPrefixPath + 'service/update?id='+$scope.service.job_id+"&replicate="+$scope.service.replica_num+"&master="+config.masterAddr)
                  .success(function(data){
                        if(data.status == 0){ 
                          notify({ message:'更新服务成功'} );
                         $modalInstance.dismiss('cancel');
                        }else{ 
                          notify({ message:'更新服务失败',classes:"alert-danger"} );
                        }
                      })
                  .error(function(data){});
        }
        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };

});
angular.module('galaxy.ui.ctrl').controller('CreateServiceModalInstanceCtrl', 
                                            function ($scope, $modalInstance,$http,$route,notify,config ,$cookies) {

  $scope.disableBtn=false;
  $scope.alerts = [];
  $scope.defaultPkgType = [{name:'FTP',id:0},{name:'HTTP',id:1},{name:'P2P',id:2},{name:'BINARY',id:3}];
  $scope.deployTpl = {groupId:"",name:"",startCmd:"",tag:"",pkgType:0,pkgSrc:"",deployStepSize:5,replicate:0,memoryLimit:3,cpuShare:0.5,oneTaskPerHost:false};
  if ($cookies.lastServiceForm != undefined && 
      $cookies.lastServiceForm != null){
      try{
         $scope.deployTpl = JSON.parse($cookies.lastServiceForm);
      }finally{
      }
  }
  $http.get(config.rootPrefixPath + "tag/list?master="+config.masterAddr)
         .success(function(data){
             $scope.tagList = data.data;
  });
  $http.get(config.rootPrefixPath + "quota/mygroups")
         .success(function(data){
             $scope.groupList = data.data;
  });
  $scope.selectGroup = null;
  $scope.groupUpdate = function(){
      if ($scope.selectGroup != null){
          $http.get(config.rootPrefixPath + "quota/groupstat?id="+ $scope.selectGroup.id)
               .success(function(data){
                $scope.groupStat = data.data.stat;
                $scope.deployTpl.groupId = $scope.selectGroup.id;
            });
      }
  }
  $scope.showAdvanceOption = false;
  $scope.ok = function () {
    $scope.alerts = [];
    $cookies.lastServiceForm = JSON.stringify($scope.deployTpl);
    if ($scope.groupStat == null ){
        $scope.alerts.push({msg: '请选择quota组'});
        return ;
    }
    var totalCpuRequire = $scope.deployTpl.replicate * $scope.deployTpl.cpuShare;
    var totalMemRequire = $scope.deployTpl.replicate * $scope.deployTpl.memoryLimit * 1024 * 1024 * 1024;

    if(totalCpuRequire > $scope.groupStat.total_cpu_left){
        $scope.alerts.push({msg: 'cpu超出总配额'});
        return;
    }
    if(totalMemRequire > $scope.groupStat.total_mem_left){ 
        $scope.alerts.push({msg: '内存超出总配额'});
        return;
    }

    $scope.disableBtn=true;
    $http(
      {
        method:"POST",
        url:config.rootPrefixPath + 'service/create?master='+config.masterAddr, 
        data:$scope.deployTpl,
        headers:{'Content-Type': 'application/x-www-form-urlencoded'},
        transformRequest: function(obj) {
          var str = [];
          for(var p in obj){
             str.push(encodeURIComponent(p) + "=" + encodeURIComponent(obj[p]));
          }
          return str.join("&");
        }
      }).success(function(data, status, headers, config) {
        $scope.disableBtn=false;
       if(data.status==0){
         notify({ message:'创建服务成功'} );
         $route.reload();
         $modalInstance.close();
       }else{
           $scope.alerts.push({ type: 'danger', msg: '错误：'+data.msg });
       }
     }).error(function(data, status, headers, config) {
        $scope.disableBtn=false;

     });
    //$modalInstance.close($scope.selected.item);
  };
  $scope.cancel = function () {
    $modalInstance.dismiss('cancel');
  };
  $scope.closeAlert = function(index) {
    $scope.alerts.splice(index, 1);
  };
  
  
});

}(angular));
