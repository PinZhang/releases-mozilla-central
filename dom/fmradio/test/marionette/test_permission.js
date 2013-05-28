/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


let FMRadio = window.navigator.mozFMRadio;

SpecialPowers.addPermission("fmradio", false, document);

function checkFMRadio() {
  log("Verifying FMRadio");
  ok(!FMRadio, "no FMRadio");
  finish();
}

checkFMRadio();

