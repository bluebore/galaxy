'use strict';
(function(angular){

var JobFormBuilder = function($scope){
    this.$scope = $scope;
    this.$scope.jobForm = {
        resource:{},
        jobMeta:{},
        monitorRule:{
            watchMsgList:[],
            triggerMsgList:[],
            actionMsgList:[],
            ruleMsgList:[]
        }
    };

};

JobFormBuilder.prototype.setJobMeta = function (jobName,
                                                instanceCount,
                                                oneTaskPerHost,
                                                groupId,
                                                bootCmd,
                                                deployStepSize) {
    this.$scope.jobForm.jobMeta = {
        jobName:jobName,
        instanceCount:instanceCount,
        oneTaskPerHost:oneTaskPerHost,
        groupId:groupId,
        bootCmd:bootCmd,
        deployStepSize:deployStepSize
    };
};


// 配置job resource 需求
JobFormBuilder.prototype.setResource = function (cpuLimit,
                                                 cpuShare,
                                                 memoryLimi) {
    this.$scope.jobForm.resource = {
        cpuLimit:cpuLimit,
        cpuShare:cpuShare,
        memoryLimit:memoryLimit
    };
};


// TODO validate regex
JobFormBuilder.prototype.addWatchMsg = function (name, regex) {
    this.$scope.jobForm.monitorRule.watchMsgList.push({
        name:name,
        regex:regex
    });
};

JobFormBuilder.prototype.addTriggerMsg = function (name,
                                                   threshold,
                                                   relate,
                                                   range) {
    this.$scope.jobForm.monitorRule.triggerMsgList.push({
        name:name,
        threshold:threshold,
        relate:relate,
        range:range
    })

};

JobFormBuilder.prototype.addActionMsg = function (sendList, title, content) {
    this.$scope.jobForm.monitorRule.actionMsgList.push({
        sendList:sendList,
        title:title,
        content:content
    });
};

JobFormBuilder.prototype.addRuleMsg = function (watchName, triggerName, actionName) {
    this.$scope.jobForm.monitorRule.ruleMsgList.push({
        watchName:watchName,
        triggerName:triggerName,
        actionName:actionName
    });

};
JobFormBuilder.prototype.deleteMsg = function (type ,index) {
    // return a new array
    var deleteItem = function(array, index) {
        var newArray = new Array;
        for (var i = 0; i < array.length; i ++){
            if (i == index) {
                continue;
            }
            newArray.push(array[i]);
        }
        return newArray;
    }
    var oldList = null;
    switch (type) {
        case 'watch':
            oldList = this.$scope.jobForm.monitorRule.watchMsgList;
            this.$scope.jobForm.monitorRule.watchMsgList = deleteItem(oldList, index);
            break;
        case 'trigger':
            oldList = this.$scope.jobForm.monitorRule.triggerMsgList;
            this.$scope.jobForm.monitorRule.triggerMsgList = deleteItem(oldList, index);
            break;
        case 'action':
            oldList = this.$scope.jobForm.monitorRule.actionMsgList;
            this.$scope.jobForm.monitorRule.actionMsgList = deleteItem(oldList, index);
            break;
        case 'rule':
            oldList = this.$scope.jobForm.monitorRule.ruleMsgList;
            this.$scope.jobForm.monitorRule.ruleMsgList = deleteItem(oldList, index);
        default:
            break;
    }
};

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
        controller: 'NewJobModalCtrl',
        keyboard:false,
        animation:false,
        backdrop:'static'
      });
    };
     $scope.updateService = function (service) {
      var modalInstance = $modal.open({
        templateUrl: 'views/updateService.html',
        controller: 'UpdateServiceModalIntanceCtrl',
        animation:false,
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
               for (var i in data.data) {
                    data.data[i].tpl = data.data[i].job_name + data.data[i].job_id + ".html";
               }
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
        animation:false,
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
        animation:false,
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
        animation:false,
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
        animation:false,
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
    $scope.popoverTmpl = "jobstate.html";
      
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

// 创建job controller
angular.module('galaxy.ui.ctrl').controller('NewJobModalCtrl',function($scope,
                                                                      $modalInstance,
                                                                      $http,
                                                                      $route,
                                                                      notify,
                                                                      config,
                                                                      $cookies){
    $scope.watchList = [];
    $scope.triggerList = [];
    $scope.actionList = [];
    $scope.ruleList = [];
    $scope.defaultPkgType = [{name:'FTP',id:0},{name:'HTTP',id:1},{name:'P2P',id:2},{name:'BINARY',id:3}];
    $scope.deployTpl = {
        groupId:"",
        name:"",
        startCmd:"",
        tag:"",
        pkgType:0,
        pkgSrc:"",
        deployStepSize:10,
        replicate:0,
        memoryLimit:3,
        cpuShare:0.5,
        cpuLimit:0.5,
        oneTaskPerHost:false,
        logInput:"",
        setMonitor:false
    };
    if ($cookies.lastServiceForm != undefined && 
         $cookies.lastServiceForm != null){
         try{
             $scope.deployTpl = JSON.parse($cookies.lastServiceForm);
         }finally{
         }
    }

    $scope.disableSubmit=false;
    $scope.alerts = [];
    // 获取集群可用tag列表
    $http.get(config.rootPrefixPath + "tag/list?master="+config.masterAddr)
           .success(function(data){
               $scope.tagList = data.data;
    });
    // 获取可用组
    $http.get(config.rootPrefixPath + "quota/mygroups")
           .success(function(data){
               $scope.groupList = data.data;
    });
    $scope.resetEditor = function() {
        $scope.showWatch = $scope.showAction = $scope.showTrigger = false ;
        $scope.showRule = false;
    }
    $scope.resetEditor();
    $scope.showEditor = function(type) {
        $scope.resetEditor();
        switch (type) {
            case 'watch':
                $scope.showWatch = true;
                $scope.watchTpl = {name:"",regex:""};
                break;
            case 'action':
                $scope.showAction = true;
                $scope.triggerTpl = {name:"",threshold:0,rangeFrom:0,rangeTo:0};
                break;
            case 'trigger':
                $scope.showTrigger = true;
                break;
            case 'rule':
                $scope.showRule = true;
                break;
            default:
                break;
        }
    }

    $scope.deleteRule = function(type, index) {
        var deleteItem = function(array, index) {
            var newArray = new Array;
            for (var i = 0; i < array.length; i ++){
                if (i == index) {
                    continue;
                }
                newArray.push(array[i]);
            }
            return newArray;
        }
        switch (type) {
            case 'watch':
                $scope.watchList = deleteItem($scope.watchList, index);
                break;
            case 'trigger':
                $scope.triggerList = deleteItem($scope.triggerList, index);
                break;
            case 'action':
                $scope.actionList = deleteItem($scope.actionList, index);
                break;
            case 'rule':
                $scope.ruleList = deleteItem($scope.ruleList, index);
                break;
            default:
                break;
        }
    }
    $scope.watchTpl = {name:"",regex:""};
    $scope.addWatchMsg = function(){
        $scope.watchList.push({
            "name":$scope.watchTpl.name,
            "regex":$scope.watchTpl.regex
        });
        $scope.resetEditor();
    }
    $scope.triggerTpl = {name:"",threshold:0,rangeFrom:0,rangeTo:0};
    $scope.addTriggerMsg = function(){
        $scope.triggerList.push({
            name:$scope.triggerTpl.name,
            threshold:$scope.triggerTpl.threshold,
            rangeFrom:$scope.triggerTpl.rangeFrom,
            rangeTo:$scope.triggerTpl.rangeTo
        });
        $scope.resetEditor();
    }
    $scope.actionTpl = {sendList:"", title:"", content:""};
    $scope.addAction = function(){
        $scope.actionList.push({
            sendList:$scope.actionTpl.sendList,
            title:$scope.actionTpl.title,
            content:$scope.actionTpl.content
        });
        $scope.resetEditor();
    }
    $scope.ruleTpl = {
        watch:{},
        trigger:{},
        action:{}
    };
    $scope.addRule = function() {
        $scope.ruleList.push({
            watch:$scope.ruleTpl.watch,
            trigger:$scope.ruleTpl.trigger,
            action:$scope.ruleTpl.action
        });
        $scope.resetEditor();
    };
    $scope.selectGroup = null;
    $scope.groupUpdate = function(selectGroup){
      if (selectGroup != null){
          $http.get(config.rootPrefixPath + "quota/groupstat?id="+ selectGroup.id)
               .success(function(data){
                $scope.groupStat = data.data.stat;
                $scope.deployTpl.groupId = selectGroup.id;
            });
      }
    }
    $scope.ok = function () {
        $scope.alerts = [];
        $scope.deployTpl.rules = $scope.ruleList;
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
        if ($scope.deployTpl.name == "") {
            $scope.alerts.push({msg: '请填写名称'});
            return;
        }

        $scope.disableBtn=true;
        $http({
            method:"POST",
            url:config.rootPrefixPath + 'service/create?master='+config.masterAddr, 
            data:{data:$cookies.lastServiceForm},
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
    };

     $scope.cancel = function () {
        $modalInstance.dismiss('cancel');
     };
     $scope.closeAlert = function(index) {
         $scope.alerts.splice(index, 1);
     };



});
/*angular.module('galaxy.ui.ctrl').controller('CreateServiceModalInstanceCtrl',
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
  
  
});*/

}(angular));
