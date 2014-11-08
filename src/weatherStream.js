var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function locationSuccess(pos) {
  // Construct URL
  var url = "http://api.openweathermap.org/data/2.5/weather?lat=" +
      pos.coords.latitude + "&lon=" + pos.coords.longitude;

  // Send request to OpenWeatherMap
  xhrRequest(url, 'GET', 
    function(responseText) {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);

      // Temperature in Kelvin requires adjustment
      var temperature = Math.round(json.main.temp - 273.15);
      //console.log("WX Temperature is " + temperature);

      // Conditions
      //var conditions = json.weather[0].main;      
      //console.log("WX Conditions are " + conditions);
      
      // Temperature Min
      //var temperatureMin = Math.round(json.main.temp_min - 273.15);
      //console.log("WX Min Temperature is " + temperatureMin);
      
      // Temperature Min
      //var temperatureMax = Math.round(json.main.temp_max - 273.15);
      //console.log("WX Max Temperature is " + temperatureMax);
      
      // Wind Speed
      var windSpeed = Math.round(json.wind.speed);
      //console.log("WX Wind Speed is " + windSpeed);
      
      // Wind Direction
      var windDirection = Math.round(json.wind.deg);
      //console.log("WX Wind Direction is " + windDirection);
      
      // Humidity
      //var humidity = Math.round(json.main.humidity);
      //console.log("WX Humidity is " + humidity);
      
      // Description
      var description = json.weather[0].description;      
      //console.log("WX Description is " + description);
      
      // Assemble dictionary using our keys
      var dictionary = {
        "KEY_TEMPERATURE": temperature,
        "KEY_WIND_SPEED": windSpeed,
        "KEY_WIND_DIRECTION": windDirection,
        "KEY_DESCRIPTION": description
      };
//        "KEY_TEMP_MIN": temperatureMin,
//        "KEY_TEMP_MAX": temperatureMax,
//        "KEY_CONDITIONS": conditions,
//        "KEY_HUMIDITY": humidity,

      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
        function(e) {
          //console.log("Weather info sent to Pebble successfully WX!");
        },
        function(e) {
          //console.log("Error sending weather info to Pebble WX!");
        }
      );
    }      
  );
  
  // Construct URL
  var forecasturl = "http://api.openweathermap.org/data/2.5/forecast/daily?lat=" +
      pos.coords.latitude + "&lon=" + pos.coords.longitude;

  // Send request to OpenWeatherMap
  xhrRequest(forecasturl, 'GET', 
    function(responseForecastText) {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseForecastText);

      var day1Time = json.list[0].dt;
      //console.log("Day 1 Time is " + day1Time);
      
      // Conditions
      var day1Conditions = json.list[0].weather[0].main;      
      //console.log("Day 1 Forecast is " + day1Conditions);
      
      // Temperature in Kelvin requires adjustment
      var day1TemperatureMin = Math.round(json.list[0].temp.min - 273.15);
      //console.log("Day 1 Min Temperature is " + day1TemperatureMin);

      // Temperature in Kelvin requires adjustment
      var day1TemperatureMax = Math.round(json.list[0].temp.max - 273.15);
      //console.log("Day 1 Max Temperature is " + day1TemperatureMax);

      var day2Time = json.list[1].dt;
      //console.log("Day 2 Time is " + day2Time);
      
      // Conditions
      var day2Conditions = json.list[1].weather[0].main;      
      //console.log("Day 2 Forecast is " + day2Conditions);
           
      // Temperature in Kelvin requires adjustment
      var day2TemperatureMin = Math.round(json.list[1].temp.min - 273.15);
      //console.log("Day 2 Min Temperature is " + day2TemperatureMin);

      // Temperature in Kelvin requires adjustment
      var day2TemperatureMax = Math.round(json.list[1].temp.max - 273.15);
      //console.log("Day 2 Max Temperature is " + day2TemperatureMax);

      var day3Time = json.list[2].dt;
      //console.log("Day 3 Time is " + day3Time);
      
      // Conditions
      var day3Conditions = json.list[2].weather[0].main;      
      //console.log("Day 3 Forecast is " + day3Conditions);
           
      // Temperature in Kelvin requires adjustment
      var day3TemperatureMin = Math.round(json.list[2].temp.min - 273.15);
      //console.log("Day 3 Min Temperature is " + day3TemperatureMin);

      // Temperature in Kelvin requires adjustment
      var day3TemperatureMax = Math.round(json.list[2].temp.max - 273.15);
      //console.log("Day 3 Max Temperature is " + day3TemperatureMax);

      // Assemble dictionary using our keys
      var dictionary = {
        "KEY_DAY1_TIME": day1Time,
        "KEY_DAY1_CONDITIONS": day1Conditions,
        "KEY_DAY1_TEMP_MIN": day1TemperatureMin,
        "KEY_DAY1_TEMP_MAX": day1TemperatureMax,
        "KEY_DAY2_TIME": day2Time,
        "KEY_DAY2_CONDITIONS": day2Conditions,
        "KEY_DAY2_TEMP_MIN": day2TemperatureMin,
        "KEY_DAY2_TEMP_MAX": day2TemperatureMax,
        "KEY_DAY3_TIME": day3Time,
        "KEY_DAY3_CONDITIONS": day3Conditions,
        "KEY_DAY3_TEMP_MIN": day3TemperatureMin,
        "KEY_DAY3_TEMP_MAX": day3TemperatureMax
      };

      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
        function(e) {
          //console.log("Weather info sent to Pebble successfully WX!");
        },
        function(e) {
          //console.log("Error sending weather info to Pebble WX!");
        }
      );
    }      
  );
  
}

function locationError(err) {
  console.log("Error requesting location WX!");
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 60000}
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    //console.log("PebbleKit WX JS ready!");

    // Get the initial weather
    getWeather();
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    //console.log("AppMessage WX received!");
    getWeather();
  }                     
);
