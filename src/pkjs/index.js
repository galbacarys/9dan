// goface pkjs entry: hooks Clay so the phone can render the config webview
// (bundled, works offline) and relays the chosen values to the watch via
// AppMessage. Also fetches outside temperature from Open-Meteo (free, no key)
// using the phone's GPS, per the canonical Pebble weather tutorial.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

// ---- weather (Open-Meteo, phone-side) ----
// Temperature units come from the Clay config page; stored in localStorage by Clay.
function weatherUnits() {
  try {
    var saved = JSON.parse(localStorage.getItem('clay-settings') || '{}');
    return saved.Units === 'F' ? 'F' : 'C';
  } catch (e) { return 'C'; }
}

function xhrRequest(url, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () { callback(this.responseText); };
  xhr.open('GET', url);
  xhr.send();
}

function locationSuccess(pos) {
  var url = 'https://api.open-meteo.com/v1/forecast?' +
      'latitude=' + pos.coords.latitude +
      '&longitude=' + pos.coords.longitude +
      '&current=temperature_2m';
  if (weatherUnits() === 'F') url += '&temperature_unit=fahrenheit';

  xhrRequest(url, function (responseText) {
    var json;
    try { json = JSON.parse(responseText); } catch (e) { return; }
    if (!json.current) return;
    var temperature = Math.round(json.current.temperature_2m);
    Pebble.sendAppMessage(
      { 'TEMPERATURE': temperature },
      function () { console.log('Weather sent: ' + temperature); },
      function () { console.log('Error sending weather'); }
    );
  });
}

function locationError(err) {
  console.log('Error requesting location!');
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    { timeout: 15000, maximumAge: 60000 }
  );
}

// ---- app events ----
Pebble.addEventListener('ready', function (e) {
  console.log('PebbleKit JS ready!');
  getWeather();               // fetch temp as soon as the face opens
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload['REQUEST_WEATHER']) getWeather();
});
