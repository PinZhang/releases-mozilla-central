/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 5000;

SpecialPowers.addPermission("fmradio", true, document);

let FMRadio = window.navigator.mozFMRadio;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(FMRadio, "FMRadio");
  is(FMRadio.enabled, false, "FMRadio.enabled");
  log('frequency:' + FMRadio.frequency);
  log('antenna: ' + FMRadio.antennaAvailable);
  log('upperBound: ' + FMRadio.frequencyUpperBound);
  log('lowerBound: ' + FMRadio.frequencyLowerBound);
  log('channelWidth: ' + FMRadio.channelWidth);

  finish();
}

verifyInitialState();

