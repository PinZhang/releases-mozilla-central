/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const nsIIDBObjectStore = Components.interfaces.nsIIDBObjectStore;
  const UNKNOWN_ERR =
    Components.interfaces.nsIIDBDatabaseException.UNKNOWN_ERR;

  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";
  const objectStoreName = "Objects";

  let request = mozIndexedDB.open(name, 1, description);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  let objectStore = db.createObjectStore(objectStoreName,
                                         { keyPath: "foo" });

  let addedCount = 0;

  for (let i = 0; i < 100; i++) {
    request = objectStore.add({foo: i});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      if (++addedCount == 100) {
        executeSoon(function() { testGenerator.next(); });
      }
    }
  }
  yield;

  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  is(db.objectStoreNames.item(0), objectStoreName, "Correct name");

  db.close();

  let request = mozIndexedDB.open(name, 2, description);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;
  let trans = event.target.transaction;

  let oldObjectStore = trans.objectStore(objectStoreName);
  isnot(oldObjectStore, null, "Correct object store prior to deleting");
  db.deleteObjectStore(objectStoreName);
  is(db.objectStoreNames.length, 0, "Correct objectStores list");
  try {
    trans.objectStore(objectStoreName);
    ok(false, "should have thrown");
  }
  catch(ex) {
    ok(ex instanceof IDBDatabaseException, "Got a IDBDatabaseException");
    is(ex.code, IDBDatabaseException.NOT_FOUND_ERR, "expect a NOT_FOUND_ERR");
  }

  objectStore = db.createObjectStore(objectStoreName, { keyPath: "foo" });
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  is(db.objectStoreNames.item(0), objectStoreName, "Correct name");
  is(trans.objectStore(objectStoreName), objectStore, "Correct new objectStore");
  isnot(oldObjectStore, objectStore, "Old objectStore is not new objectStore");

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.result, undefined, "ObjectStore shouldn't have any items");
    testGenerator.send(event);
  }
  event = yield;

  db.deleteObjectStore(objectStore.name);
  is(db.objectStoreNames.length, 0, "Correct objectStores list");

  continueToNextStep();
  yield;

  db.close();

  let request = mozIndexedDB.open(name, 3, description);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;

  objectStore = db.createObjectStore(objectStoreName, { keyPath: "foo" });

  request = objectStore.add({foo:"bar"});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  db.deleteObjectStore(objectStoreName);

  event = yield;

  finishTest();
  yield;
}
