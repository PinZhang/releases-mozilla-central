/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 5000;

SpecialPowers.addPermission("fmradio", true, document);

let FMRadio = window.navigator.mozFMRadio;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio, "FMRadio");
  is(FMRadio.enabled, false, "FMRadio.enabled");
  enableThenDisable();
}

function enableThenDisable() {
  log("Enable FM Radio and disable it immediately.");
  var request = FMRadio.enable(90);
  ok(request, "request should not be null.");

  request.onsuccess = function() {
    ok(false, "Enable request should fail.");
  };

  var failedToEnable = false;
  request.onerror = function() {
    failedToEnable = true;
  };

  var disableReq = FMRadio.disable();
  ok(disableReq, "Disable request should not be null.");

  disableReq.onsuccess = function() {
    ok(failedToEnable, "Enable request failed.");
    ok(!FMRadio.enabled, "FMRadio.enabled is false.");
    finish();
  };

  disableReq.onerror = function() {
    ok(false, "Disable request should not fail.");
  };
}

verifyInitialState();

