var MONTHS       = ["Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"];
var RESET_TIME_RAIN_GAUGE = 1000 * 60 * 5; //ultimos 5 minutos
var mMap;
var mHeatmap;
var mMapBounds;
var mMarkers = {};
var mSensors = {};

window.onload = function(){
    initMap();
    listenSensors();
    initGraphs();

    $("#all_sensors").click(function(){
        mMap.fitBounds(mMapBounds);
    });

    $("#heatmapToogle").click(function(){
        if(mHeatmap.getMap()){
            mHeatmap.setMap(null);
            showLabels(true);
        }
        else{
            mHeatmap.setMap(mMap);
            showLabels(false);
        }

    })

    $(document).on("click","#sensors li",function(){
        key = $(this).data("key");

        mMap.setCenter(mMarkers[key].position);
        mMap.setZoom(18);

    });

    setInterval(function(){
        resetRainGauges();
    },RESET_TIME_RAIN_GAUGE);
}

function initMap(){
    console.log("init Map");
    var myLatLng = {lat: -33.447487, lng: -70.673676};//para centralizar los marker

    var mapOptions = {
      zoom: 12,
      center: myLatLng,
      disableDefaultUI: true,
      zoomControl:true,
      styles:[
                {
                  featureType: 'all',
                  stylers: [
                    { saturation: -120 }
                  ]
                },{
                  featureType: 'road.arterial',
                  elementType: 'geometry',
                  stylers: [
                    { hue: '#00ffee' },
                    { saturation: 50 }
                  ]
                },{
                  featureType: 'poi.business',
                  elementType: 'labels',
                  stylers: [
                    { visibility: 'off' }
                  ]
                }
              ]

    };

    mMap       = new google.maps.Map(document.getElementById('map'),mapOptions);

    mMapBounds = new google.maps.LatLngBounds();

    mHeatmap = new google.maps.visualization.HeatmapLayer({data: [],
                                                            map: null,
                                                            radius: 150
                                                            });
}

function listenSensors(){
    console.log("listening sensors");

    firebase.database().ref('sensors').once('value', function(snapshot) {
        console.log("Sensors",snapshot.val())
        snapshot.forEach(function(sensor) {
            addSensor(sensor.key,sensor.val());

            listenSensor(sensor.key);
        });
        mMap.fitBounds(mMapBounds);
    });


    //Init Heatmap
    firebase.database().ref('measures/').limitToLast(100).once('value', function(snapshot) {
        console.log("LastMeasures",snapshot.val());

        snapshot.forEach(function(measure) {
            sensor = mSensors[measure.key];
            if(sensor){
                data   = measure.val();

                if(data["level"]){
                    console.log("LEVEL")
                }

                if(data["rain_gauge"]){
                    weight = 0;
                    for(key in data["rain_gauge"]){
                        value = data["rain_gauge"][key];
                        weight += value.value;
                    }

                    console.log(sensor.name,weight)

                    var latLng = new google.maps.LatLng(sensor.lat, sensor.lng);
                    mHeatmap.getData().push({location:latLng,weight:weight});
                }
            }

        });
      });
}

function listenSensor(key){

    firebase.database().ref('measures/'+key+'/level').limitToLast(1).on('value', function(snapshot) {
        console.log("level",snapshot.val())

        snapshot.forEach(function(sensor) {
            updateSensor(key,sensor.val(),"level");
        });

    });


    firebase.database().ref('measures/'+key+'/rain_gauge').limitToLast(1).on('value', function(snapshot) {
          console.log("level",snapshot.val())

        if(snapshot.val()!=null){
            val=0;
            if(mSensors[key] && mSensors[key]['values'] && mSensors[key]['values']['rain_gauge']){
                 val = parseInt(mSensors[key]['values']['rain_gauge']);
            }
            timestamp = 0;
            snapshot.forEach(function(sensor) {
                data = sensor.val();
                val += data.value;
                timestamp = data.timestamp;
            });

            updateSensor(key,{value:val,timestamp:timestamp},"rain_gauge");
            mHeatmap.getData().push({location:new google.maps.LatLng(mSensors[key].lat, mSensors[key].lng),weight:val});
        }

    });
}


function addSensor(key,data){
    if(data.name && data.lat && data.lng){
        var location = new google.maps.LatLng(parseFloat(data.lat),parseFloat(data.lng));

        var marker = new MarkerWithLabel({
            position: location,
            map: mMap,
            title: data.name,
            icon: "images/pin-green.png",

            labelContent: data.name,
            //labelAnchor: new google.maps.Point(22, 0),
            labelClass: "labels",
            labelStyle: {opacity: 0.75}
        });

        mMarkers[key] = marker;
        mSensors[key] = data;

        google.maps.event.addListener(marker, "click", function (e) {
            mMap.setCenter(marker.position);
            mMap.setZoom(18);

        });
        mMapBounds.extend(location);


        $("#sensors").append("<li class='clickeable' data-key='"+key+"'>"+data.name+"</li>")
    }
}

function updateSensor(key,data,type){
    //console.log(key,data)
    marker = mMarkers[key];
    sensor = mSensors[key];

    //console.log(marker,sensor)

    if(marker!=null && sensor!=null){
        if(!sensor['values']){
            sensor['values'] = {};
        }

        sensor['values'][type] = data.value;

        if(!sensor['values']['level']){
            sensor['values']['level'] = 0;
        }
        if(!sensor['values']['rain_gauge']){
            sensor['values']['rain_gauge'] = 0;
        }

        image = checkLimits(sensor);
        labelContent = "<b>"+marker.title+"</b>"+
                       "<br>Agua caída: <span class='value'>"+sensor['values']['rain_gauge']+" mm</span>"+
                       "<br>Acumulada: <span class='value'>"+sensor['values']['level']+" mm</span>"+
                        "<br><div class='date'>"+formatTime(data.timestamp)+"</div>";

        marker.setValues({ icon: image });
        marker.label.marker_.set("labelContent",labelContent);
        marker.label.setContent();
    }
}

function checkLimits(sensor){
    alert = "green";
    if(sensor.limits){
        if(sensor.values.level >= sensor.limits.hard_limit_level ||
           sensor.values.rain_gauge >= sensor.limits.hard_limit_rain_gauge){
            alert = "red";
        }
        else if(sensor.values.level >= sensor.limits.soft_limit_level ||
           sensor.values.rain_gauge >= sensor.limits.soft_limit_rain_gauge){
            alert = "yellow";
        }
    }

    return "images/pin-"+alert+".png";

}

function resetRainGauges(){
    for (var key in mSensors) {
        if (mSensors.hasOwnProperty(key)) {
            mSensors[key]['values']['rain_gauge']=0;
            updateSensor(key,{value:0,timestamp:new Date().getTime()},"rain_gauge");
        }
    }

}


//Utils
var showLabels = function(show){
    for (var key in mSensors) {
        if (mMarkers.hasOwnProperty(key)) {
            mMarkers[key].label.marker_.set("labelVisible",show);
            mMarkers[key].label.setVisible();
        }
    }

}

var formatTime = function(timestamp) {
    var date = (timestamp) ? new Date(timestamp) : new Date(),
        hours = date.getHours(),
        minutes = '' + date.getMinutes(),
        day = date.getDate(),
        month = MONTHS[date.getMonth()],
        year = date.getFullYear();

    minutes = (minutes.length < 2) ? '0' + minutes : minutes;
    return '' + hours + ':' + minutes + " <b>"+day+" "+month+" "+year+"</b>";
}
