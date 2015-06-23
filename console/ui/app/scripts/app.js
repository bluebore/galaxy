'use strict';


(function(angular){

 /**
 * @ngdoc overview
 * @name uidemoApp
 * @description
 * # uidemoApp
 *
 * Main module of the application.
 */
var galaxy = angular.module('galaxy.ui', [
    'ngAnimate',
    'ngCookies',
    'ngResource',
    'ngRoute',
    'ngSanitize',
    'easypiechart',
    'ui.bootstrap',
    'galaxy.ui.ctrl',
    'galaxy.ui.breadcrumb',
    'galaxy.ui.promot',
    'cgNotify',
    'galaxy.ui.loader',
    'galaxy.ui.treeview',
    'ui.select'
  ])
  .config(function ($routeProvider) {
    $routeProvider
      .when('/', {
        templateUrl: 'views/main.html',
        controller: 'HomeCtrl'
      })
      .when('/service', {
        templateUrl: 'views/main.html',
        controller: 'HomeCtrl'
      })
      .when('/cluster', {
        templateUrl: 'views/cluster.html',
        controller: 'ClusterCtrl'
      })
      .when('/cluster/status', {
        templateUrl: 'views/cluster.html',
        controller: 'ClusterCtrl'
      })
      .when('/quota', {
        templateUrl: 'views/quota.html',
        controller: 'QuotaCtrl'
      })
      .when('/cluster/tag',{
        templateUrl: 'views/tag.html',
        controller: 'TagPageCtrl'
       })
      .when('/setup', {
        templateUrl: 'views/setup.html',
        controller: 'SetupCtrl'
      })

      .otherwise({
        redirectTo: '/'
      });
  })
  .controller('AppCtrl',['$scope','$rootScope','$http','$location','notify','config',function($scope,$rootScope,$http,$location,notify,config){
      $scope.user = config.user;
      notify.config({duration:1000,templateUrl:'views/notify/notice.html'});
  }]);
  function getParameterByName(name) {
    name = name.replace(/[\[]/, "\\[").replace(/[\]]/, "\\]");
    var regex = new RegExp("[\\?&]" + name + "=([^&#]*)"),
        results = regex.exec(location.search);
    return results === null ? null : decodeURIComponent(results[1].replace(/\+/g, " "));
  }

  function fetchConfig(){
    var initInjector = angular.injector(["ng","ngCookies"]);
    var $http = initInjector.get("$http");
    var $cookies = initInjector.get('$cookies');
    var masterAddr = getParameterByName('master');
    if(masterAddr ==null && $cookies.masterAddr != undefined ){
        masterAddr = $cookies.masterAddr;
    }
    var rootPrefixPath = "/";
    return $http.get(rootPrefixPath+"conf/get").then(function(response) {
            if (response.data.status == -2) {
                   window.location=response.data.msg;
            }else{
                 galaxy.constant("config", {config:response.data.data.config,
                                       masterAddr:masterAddr,
                                       home:response.data.data.home,
                                       service:response.data.data.service,
                                       user:response.data.data.user,
                                       rootPrefixPath:rootPrefixPath});


            }
         }, function(errorResponse) {

     });
  }
  fetchConfig().then(function(){
     angular.element(document).ready(function() {
            angular.bootstrap(angular.element('body'), ["galaxy.ui"]);
      });
  });





}(angular));
