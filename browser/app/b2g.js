// base url for the wifi geolocation network provider
pref("geo.wifi.uri", "https://maps.googleapis.com/maps/api/browserlocation/json");

// enable geo
pref("geo.enabled", true);

// Override some named colors to avoid inverse OS themes
pref("ui.-moz-dialog", "#efebe7");
pref("ui.-moz-dialogtext", "#101010");
pref("ui.-moz-field", "#fff");
pref("ui.-moz-fieldtext", "#1a1a1a");
pref("ui.-moz-buttonhoverface", "#f3f0ed");
pref("ui.-moz-buttonhovertext", "#101010");
pref("ui.-moz-combobox", "#fff");
pref("ui.-moz-comboboxtext", "#101010");
pref("ui.buttonface", "#ece7e2");
pref("ui.buttonhighlight", "#fff");
pref("ui.buttonshadow", "#aea194");
pref("ui.buttontext", "#101010");
pref("ui.captiontext", "#101010");
pref("ui.graytext", "#b1a598");
pref("ui.highlight", "#fad184");
pref("ui.highlighttext", "#1a1a1a");
pref("ui.infobackground", "#f5f5b5");
pref("ui.infotext", "#000");
pref("ui.menu", "#f7f5f3");
pref("ui.menutext", "#101010");
pref("ui.threeddarkshadow", "#000");
pref("ui.threedface", "#ece7e2");
pref("ui.threedhighlight", "#fff");
pref("ui.threedlightshadow", "#ece7e2");
pref("ui.threedshadow", "#aea194");
pref("ui.window", "#efebe7");
pref("ui.windowtext", "#101010");
pref("ui.windowframe", "#efebe7");

// Web Notifications
pref("notification.feature.enabled", true);

// IndexedDB
pref("indexedDB.feature.enabled", true);
pref("dom.indexedDB.warningQuota", 5);

//  0: don't show fullscreen keyboard
//  1: always show fullscreen keyboard
// -1: show fullscreen keyboard based on threshold pref
pref("widget.ime.android.landscape_fullscreen", -1);
pref("widget.ime.android.fullscreen_threshold", 250); // in hundreths of inches

// enable touch events interfaces
pref("dom.w3c_touch_events.enabled", 1);
pref("dom.w3c_touch_events.safetyX", 0); // escape borders in units of 1/240"
pref("dom.w3c_touch_events.safetyY", 120); // escape borders in units of 1/240"

// True if this is the first time we are showing about:firstrun
pref("browser.firstrun.show.uidiscovery", true);
pref("browser.firstrun.show.localepicker", true);

// initiated by a user
pref("content.ime.strict_policy", true);

// True if you always want dump() to work
//
// On Android, you also need to do the following for the output
// to show up in logcat:
//
// $ adb shell stop
// $ adb shell setprop log.redirect-stdio true
// $ adb shell start
pref("browser.dom.window.dump.enabled", false);

// Temporarily relax file:// origin checks so that we can use <img>s
// from other dirs as webgl textures and more.  Remove me when we have
// installable apps or wifi support.
pref("security.fileuri.strict_origin_policy", false);

// Default Content Security Policy to apply to privileged and certified apps
pref("security.apps.privileged.CSP.default", "default-src *; script-src 'self'; object-src 'none'; style-src 'self' 'unsafe-inline'");
pref("security.apps.certified.CSP.default", "options inline-script eval-script; default-src *; script-src 'self'; object-src 'none'; style-src 'self'");

// Temporarily force-enable GL compositing.  This is default-disabled
// deep within the bowels of the widgetry system.  Remove me when GL
// compositing isn't default disabled in widget/android.
pref("layers.acceleration.force-enabled", true);

// Temporary permission hack for WebSMS
pref("dom.sms.enabled", true);
pref("dom.sms.strict7BitEncoding", false); // Disabled by default.

// Temporary permission hack for WebContacts
pref("dom.mozContacts.enabled", true);

// WebAlarms
pref("dom.mozAlarms.enabled", true);

// WebSettings
pref("dom.mozSettings.enabled", true);
pref("dom.mozPermissionSettings.enabled", true);

// controls if we want camera support
pref("device.camera.enabled", true);
pref("media.realtime_decoder.enabled", true);

// TCPSocket
pref("dom.mozTCPSocket.enabled", true);

// "Preview" landing of bug 710563, which is bogged down in analysis
// of talos regression.  This is a needed change for higher-framerate
// CSS animations, and incidentally works around an apparent bug in
// our handling of requestAnimationFrame() listeners, which are
// supposed to enable this REPEATING_PRECISE_CAN_SKIP behavior.  The
// secondary bug isn't really worth investigating since it's obseleted
// by bug 710563.
pref("layout.frame_rate.precise", true);

// Temporary remote js console hack
pref("b2g.remote-js.enabled", true);
pref("b2g.remote-js.port", 9999);

// Handle hardware buttons in the b2g chrome package
pref("b2g.keys.menu.enabled", true);

// Screen timeout in seconds
pref("power.screen.timeout", 600);

pref("full-screen-api.enabled", true);

pref("media.volume.steps", 10);

// Enable device storage
pref("device.storage.enabled", true);

pref("media.plugins.enabled", false);
pref("media.omx.enabled", true);


