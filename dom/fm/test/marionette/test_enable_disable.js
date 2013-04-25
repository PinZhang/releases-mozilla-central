/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 5000;

SpecialPowers.addPermission("fmradio", true, document);

let FMRadio = window.navigator.mozFMRadio;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio, "FMRadio");
  is(FMRadio.enabled, false, "FMRadio.enabled");
  verifyAttributesWhenDisabled();
  enableFMRadio();
}

function verifyAttributesWhenDisabled() {
  log("Verifying attributes when disabled.");
  is(FMRadio.frequency, null, "FMRadio.frequency == null");
  ok(FMRadio.frequencyLowerBound, "FMRadio.frequencyLowerBound");
  ok(FMRadio.frequencyUpperBound, "FMRadio.frequencyUpperBound");
  ok(FMRadio.frequencyUpperBound > FMRadio.frequencyLowerBound,
    "FMRadio.frequencyUpperBound > FMRadio.frequencyLowerBound");
  ok(FMRadio.channelWidth, "FMRadio.channelWidth")
}

function enableFMRadio() {
  log("Verifying behaviors when enabled.");
  var request = FMRadio.enable(90);
  ok(request, "FMRadio.enable(90) returns request");

  request.onsuccess = function() {
    ok(FMRadio.enabled, "FMRadio.enabled");
    ok(!isNaN(FMRadio.frequency), "FMRadio.frequency is a number");
    ok(FMRadio.frequency > FMRadio.frequencyLowerBound,
      "FMRadio.frequency > FMRadio.frequencyLowerBound");
  };

  request.onerror = function() {
    ok(null, "Failed to enable");
  };

  var enabled = false;
  FMRadio.onenabled = function() {
    FMRadio.onenabled = null;
    enabled = FMRadio.enabled;
  };

  FMRadio.onfrequencychange = function() {
    log("Check if 'onfrequencychange' event is fired after the 'enabled' event");
    FMRadio.onfrequencychange = null;
    ok(enabled, "FMRadio is enabled when handling `onfrequencychange`");
    disableFMRadio();
  };
}

function disableFMRadio() {
  log("Verify behaviors when disabled");

  var results = {
    onDisabledEventFired: false,
    seekErrorFired: false
  };

  var checkResults = function() {
    for (var r in results) {
      if (results[r] == false) {
        return;
      }
    }

    finish();
  };

  var seekRequest = FMRadio.seekUp();

  seekRequest.onerror = function() {
    results.seekErrorFired = true;
    checkResults();
  };

  seekRequest.onsuccess = function() {
    ok(false, "Seek request should fail after FMRadio.disable is called.");
  };

  FMRadio.disable();
  FMRadio.ondisabled = function() {
    FMRadio.ondisabled = null;
    results.onDisabledEventFired = true;
    ok(!FMRadio.enabled, "FMRadio.enabled is false");
    checkResults();
  };
}

verifyInitialState();

