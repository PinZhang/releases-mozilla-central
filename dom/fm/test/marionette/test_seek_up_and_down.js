/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

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
  FMRadio.onenabled = seekUp;
}

function seekUp() {
  log("Seek up");
  var request = FMRadio.seekUp();
  ok(request, "Seekup request should not be null.");

  request.onsuccess = function() {
    seekDown();
  };

  request.onerror = function() {
    ok(false, "Seekup request should not fail.");
  };
}

function seekDown() {
  log("Seek down");
  var request = FMRadio.seekDown();
  ok(request, "Seekdown request should not be null.");

  request.onsuccess = function() {
    cleanUp();
  };

  request.onerror = function() {
    ok(false, "Seekdown request should not fail.");
  };

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

