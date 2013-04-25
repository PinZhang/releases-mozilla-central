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
  FMRadio.onenabled = seek;
}

function seek() {
  log("Seek up");
  var request = FMRadio.seekUp();
  ok(request, "Seekup request should not be null.");

  var seekUpIsCancelled = false;
  request.onerror = function() {
    seekUpIsCancelled = true;
  };

  request.onsuccess = function() {
    ok(false, "Seekup request should fail.");
  };

  log("Seek up");
  var cancelSeekReq = FMRadio.cancelSeek();
  ok(cancelSeekReq, "Cancel request should not be null.");

  cancelSeekReq.onsuccess = function() {
    ok(seekUpIsCancelled, "Seekup request failed.");
    cleanUp();
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

