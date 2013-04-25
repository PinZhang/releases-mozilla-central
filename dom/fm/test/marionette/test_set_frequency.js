/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 5000;

SpecialPowers.addPermission("fmradio", true, document);

let FMRadio = window.navigator.mozFMRadio;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio, "FMRadio");
  is(FMRadio.enabled, false, "FMRadio.enabled");
  setUp();
}

function setUp() {
  FMRadio.enable(90);
  FMRadio.onenabled = setFrequency;
}

function setFrequency() {
  log("Set Frequency");
  var request = FMRadio.setFrequency(100);
  ok(request, "setFrequency request should not be null.");

  request.onsuccess = setOutOfRangeFrequency;
  request.onerror = function() {
    ok(false, "setFrequency request should not fail.");
  };
}

function setOutOfRangeFrequency() {
  log("Set Frequency that out of the range");
  var request = FMRadio.setFrequency(FMRadio.frequencyUpperBound + 1);
  ok(request, "setFrequency should not be null.");

  request.onsuccess = function() {
    ok(false, "The request of setting an out-of-range frequency should fail.");
  };
  request.onerror = cleanUp;
}

function cleanUp() {
  FMRadio.disable();
  FMRadio.ondisabled = function() {
    FMRadio.ondisabled = null;
    ok(!FMRadio.enabled, "FMRadio.enabled is false");
    finish();
  };
}

verifyInitialState();

