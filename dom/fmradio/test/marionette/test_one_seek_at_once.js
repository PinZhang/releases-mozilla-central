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
  var request = FMRadio.seekUp();
  ok(request, "Seekup request should not be null.");

  request.onerror = function() {
    cleanUp();
  };

  request.onsuccess = function() {
    ok(false, "Seekup request should fail after FMRadio.cancelSeek is called.");
  };

  var seekAgainReq = FMRadio.seekUp();
  ok(seekAgainReq, "Seekup request should not be null.");

  seekAgainReq.onerror = function() {
    log("Cancel the first seek to finish the test");
    FMRadio.cancelSeek();
  };

  seekAgainReq.onsuccess = function() {
    ok(false, "The second seekup request should fail.");
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

