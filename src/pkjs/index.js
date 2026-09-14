// goface pkjs entry: hooks Clay so the phone can render the config webview
// (bundled, works offline) and relay the chosen values to the watch via AppMessage.
var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);
