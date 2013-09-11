/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("fmradio", true, document);
SpecialPowers.addPermission("settings-read", true, document);
SpecialPowers.addPermission("settings-write", true, document);

let FMRadio = window.navigator.mozFMRadio;
let mozSettings = window.navigator.mozSettings;
let KEY = "ril.radio.disabled";

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio);
  is(FMRadio.enabled, false);
  ok(mozSettings);

  checkRilSettings();
}

function checkRilSettings() {
  log("Checking airplane mode settings");
  let req = mozSettings.createLock().get(KEY);
  req.onsuccess = function(event) {
    ok(!req.result[KEY], "Airplane mode is disabled.");
    enableFMRadio();
  };

  req.onerror = function() {
    ok(false, "Error occurs when reading settings value.");
    finish();
  };
}

function enableFMRadio() {
  log("Enable FM radio");
  let frequency = FMRadio.frequencyLowerBound + FMRadio.channelWidth;
  let req = FMRadio.enable(frequency);

  req.onsuccess = function() {
    enableAirplaneMode();
  };

  req.onerror = function() {
    ok(false, "Failed to enable FM radio.");
  };
}

function enableAirplaneMode() {
  log("Enable airplane mode");
  FMRadio.ondisabled = function() {
    FMRadio.ondisabled = null;
    enableFMRadioWithAirplanModeEnabled();
  };

  mozSettings.createLock().set({
    "ril.radio.disabled": true
  });
}

function enableFMRadioWithAirplanModeEnabled() {
  log("Enable FM radio with airplane mode enabled");
  let frequency = FMRadio.frequencyLowerBound + FMRadio.channelWidth;
  let req = FMRadio.enable(frequency);
  req.onerror = cleanUp();

  req.onsuccess = function() {
    ok(false, "FMRadio could be enabled when airplane mode is enabled.");
  };
}

function cleanUp() {
  let req = mozSettings.createLock().set({
    "ril.radio.disabled": false
  });

  req.onsuccess = function() {
    ok(!FMRadio.enabled, "FMRadio.enabled is false");
    finish();
  };

  req.onerror = function() {
    ok(false, "Error occurs when setting value");
  };
}

verifyInitialState();

