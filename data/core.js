var sousVide = angular.module('sousVide', []);
sousVide.controller( 'mainController', function($scope, $http, $interval,$httpParamSerializerJQLike) {
	$scope.tRefresh=0;
	$interval(function(){
		$scope.tRefresh++;
	}, 1000);

	$scope.submit = function(data){
		$scope.tx = true;
		$http.post('/io', $httpParamSerializerJQLike(data)).then(function(res) {
			$scope.data = res.data;
			$scope.upTimeDisp = (new Date($scope.data.upTime)).toISOString().substr(11, 8);
			$scope.tRefresh = 0;
		}).finally(function(){
			$scope.tx = false;
		});
	};

	$scope.submit({});
	document.getElementById('st').focus();

	$interval(function(){
		if(!$scope.tx) {
			$scope.submit({});
		}
	}, 5000);
	
}); //sousvide