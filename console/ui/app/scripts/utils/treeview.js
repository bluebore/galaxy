'use strict';
(function(angular){
var NavTree = function($scope,$location){
    this.$scope = $scope;
    this.$location = $location;
    this.serviceNavSetting = $scope.config.servicePageConfig;
    this.homeNavSetting = $scope.config.homeConfig;
    this.processRouter = {'service':this._processServicePage,
                          'home':this._processHomePage,
                          'cluster':this._processClusterPage};

}
//更新树节点状态
//hash改变时需要调用一下这个函数
NavTree.prototype.update = function() {
    var pathArray = this._parsePath(this.$location.path());
    if (this.$location.path().indexOf("/cluster") == 0) {
          this.processRouter['cluster'](this, pathArray);
          return;
    }
    if(pathArray.length <= 1){
          this.processRouter['home'](this,pathArray);
    }else {
          var secondPage = pathArray[0];
          if(secondPage in this.processRouter){
              this.processRouter[secondPage](this,pathArray);
          }else{
              console.log("no processor");
          }
    }
};
//解析hash路径
NavTree.prototype._parsePath = function(hash){
        if(!hash){
          return [];
        }
        var pathArray = [];
        angular.forEach(hash.split('/'),function(value,key){
          var trimedValue = value.trim();
          if(trimedValue){
            this.push(trimedValue);
          }
        },pathArray);
        return pathArray;
}



NavTree.prototype._processHomePage = function(self,pathArray){
      var activeParentIndex  = 0;
      if(pathArray.length == 1 ){
        if(pathArray[0] =="cluster"){
          activeParentIndex = 2;
        }else if(pathArray[0]== 'setup'){        
          activeParentIndex = 3;
        }else if(pathArray[0]== 'quota'){
            activeParentIndex = 1;
        }

      }
      for (var index in self.homeNavSetting) {
          self._cleanSelected(self, self.homeNavSetting[index]);
      }

      self._markHomeActive(activeParentIndex);
      self.$scope.treeModel = self.homeNavSetting;
}
NavTree.prototype._cleanHomeSelected = function(){
  for(var index in this.homeNavSetting){
        this.homeNavSetting[index].nodestyle = [this.homeNavSetting[index].nodestyle[0]];
  }
}
NavTree.prototype._markHomeActive = function(index){
  var nodestyle = this.homeNavSetting[index].nodestyle[0];

  this.homeNavSetting[index].nodestyle = [nodestyle,'selected'];
}

NavTree.prototype._processClusterPage = function(self, pathArray){
    var activeChildIndex = 0;
    if (pathArray.length == 2) {
        var subpage = pathArray[1];
        for (var index in self.homeNavSetting[2].children) {
            if (self.homeNavSetting[2].children[index].subpage == subpage) {
                activeChildIndex = index;
            }
        }
    }
    for (var index in self.homeNavSetting) {
        self._cleanSelected(self, self.homeNavSetting[index]);
    }
    self._markSelected(self, 2, activeChildIndex);
    self.$scope.treeModel = self.homeNavSetting;
}

NavTree.prototype._cleanSelected = function(self, node){
    node.nodestyle = [node.nodestyle[0]];
    if (node.children.length >0) {
        for (var index in node.children) {
            self._cleanSelected(self, node.children[index]);
        }
    }
}

NavTree.prototype._markSelected = function(self, activeParentIndex, activeChild) {
    var parentNode = self.homeNavSetting[activeParentIndex];
    if (parentNode.children.length <= 0  || activeChild < 0){
        parentNode.nodestyle.push("selected");
        return;
    }
    var childNode = parentNode.children[activeChild];
    childNode.nodestyle.push("selected"); 
}

//处理service路径逻辑
NavTree.prototype._processServicePage=function(self,pathArray){
      //没有默认展现页面
      //类似/service/tera
      var activeParentIndex = 0;
      var activeChildIndex = 0;
      //有默认展现页面
      //类似/service/tera/settings
      var servicename = pathArray[1];
      if(pathArray.length >=3){
          var activePageNamde = pathArray[2];
          for(var index in self.serviceNavSetting){
            for(var iindex in self.serviceNavSetting[index].children){
              var subpage = self.serviceNavSetting[index].children[iindex].subpage;
              if (subpage == activePageNamde){
                activeParentIndex = index;
                activeChildIndex = iindex;  
              }
            }
          }
        }
      self.$scope.servicename = servicename;
      self._cleanServiceNode(servicename);
      self._markServiceActive(activeParentIndex,activeChildIndex);
      self.$scope.treeModel = self.serviceNavSetting;

}
      //清除选择状态
NavTree.prototype._cleanServiceNode = function(servicename){
        for(var index in this.serviceNavSetting){
          for(var iindex in this.serviceNavSetting[index].children){
            this.serviceNavSetting[index].children[iindex].nodestyle=[this.serviceNavSetting[index].children[iindex].nodestyle[0]];
            this.serviceNavSetting[index].children[iindex].service = servicename;
            var subpage = this.serviceNavSetting[index].children[iindex].subpage;
            this.serviceNavSetting[index].children[iindex].href="/#/service/"+servicename+'/'+subpage;
          }
        }
        this.serviceNavSetting[0].nodeName = servicename;
}
//标记选择状态
NavTree.prototype._markServiceActive=function(activeParentIndex,activeChildIndex){
        var nodestyle = this.serviceNavSetting[activeParentIndex].children[activeChildIndex].nodestyle[0];
        this.serviceNavSetting[activeParentIndex].children[activeChildIndex].nodestyle=[nodestyle,'selected'];
}

angular.module( 'galaxy.ui.treeview', [] );
//构造nav tree
angular.module( 'galaxy.ui.treeview').directive( 'navTree',
		[function() {
			return {
				controller: 'navTreeCtrl',
				restrict: 'A',
				templateUrl:'views/nav.html'
	};
}]);
angular.module( 'galaxy.ui.treeview').controller('navTreeCtrl',['$scope','$rootScope','$http','$location','config',function($scope,$rootScope,$http,$location,config){
     $scope.config = {homeConfig:config.home,servicePageConfig:config.service};
     var navTree = new NavTree($scope,$location);
     navTree.update();
     var listener = $rootScope.$on("$routeChangeSuccess", function () {
         navTree.update();
     });
     $scope.$on('$destroy',listener);
}]);

}(angular));
