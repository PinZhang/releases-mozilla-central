(function() {
  function debug(msg) {
    dump('**b2g shell: **' + msg + '\n');
  }

  XPCOMUtils.defineLazyServiceGetter(this, 'gSystemMessenger',
                                     '@mozilla.org/system-message-internal;1',
                                     'nsISystemMessagesInternal');

  Cu.import('resource://gre/modules/ObjectWrapper.jsm');
  Cu.import('resource://gre/modules/Webapps.jsm');
  Cu.import('resource://gre/modules/AppsUtils.jsm');
  Cu.import('resource://gre/modules/Keyboard.jsm');

  var shell = {
    isWebappsRegistryReady: false,

    sendEvent: function shell_sendEvent(content, type, details) {
      let event = content.document.createEvent('CustomEvent');
      event.initCustomEvent(type, true, true, details ? details : {});
      content.dispatchEvent(event);
    },

    sendChromeEvent: function shell_sendChromeEvent(details) {
      this.sendEvent(this.contentWindow, "mozChromeEvent",
                     ObjectWrapper.wrap(details, this.contentWindow));
    },

    get contentBrowser() {
      delete this.contentBrowser;
      return this.contentBrowser = document.getElementById('content');
    },

    get contentWindow() {
      return this.contentBrowser.contentWindow;
    },

    start: function() {
      WebappsHelper.init();
      this.contentBrowser.addEventListener('DOMContentLoaded',
                                           function content_onload(event) {
        if (shell.isWebappsRegistryReady) {
          // Fire chrome event, make sure the system APP got such a event
          window.setTimeout(function() {
            shell.sendChromeEvent({ type: 'webapps-registry-ready' });
          }, 1000);
        }
      });

      // Handle key events
      window.addEventListener('keydown',  this, true);
      window.addEventListener('keyup',    this, true);
      window.addEventListener('keypress', this, true);
    },

    handleEvent: function b2gshell_handleEvent(evt) {
      debug('key pressed: ' + evt.type);
      switch (evt.type) {
        case 'keydown':
        case 'keypress':
        case 'keyup':
          this.filterHardwareKeys(evt);
          break;
      }
    },

    // If this key event actually represents a hardware button, filter it here
    // and send a mozChromeEvent with detail.type set to xxx-button-press or
    // xxx-button-release instead.
    filterHardwareKeys: function shell_filterHardwareKeys(evt) {
      var type;
      switch (evt.keyCode) {
        case evt.DOM_VK_HOME:         // Home button
          type = 'home-button';
          break;
        case evt.DOM_VK_SLEEP:        // Sleep button
        case evt.DOM_VK_END:          // On desktop we don't have a sleep button
          type = 'sleep-button';
          break;
        case evt.DOM_VK_PAGE_UP:      // Volume up button
          type = 'volume-up-button';
          break;
        case evt.DOM_VK_PAGE_DOWN:    // Volume down button
          type = 'volume-down-button';
          break;
        case evt.DOM_VK_ESCAPE:       // Back button (should be disabled)
          type = 'back-button';
          break;
        case evt.DOM_VK_CONTEXT_MENU: // Menu button
          type = 'menu-button';
          break;
        case evt.DOM_VK_F1: // headset button
          type = 'headset-button';
          break;
        default:                      // Anything else is a real key
          return;  // Don't filter it at all; let it propagate to Gaia
      }

      // If we didn't return, then the key event represents a hardware key
      // and we need to prevent it from propagating to Gaia
      evt.stopImmediatePropagation();
      evt.preventDefault(); // Prevent keypress events (when #501496 is fixed).

      // If it is a key down or key up event, we send a chrome event to Gaia.
      // If it is a keypress event we just ignore it.
      switch (evt.type) {
        case 'keydown':
          type = type + '-press';
          break;
        case 'keyup':
          type = type + '-release';
          break;
        case 'keypress':
          return;
      }

      // Let applications receive the headset button key press/release event.
      if (evt.keyCode == evt.DOM_VK_F1 && type !== this.lastHardwareButtonEventType) {
        this.lastHardwareButtonEventType = type;
        gSystemMessenger.broadcastMessage('headset-button', type);
        return;
      }

      debug('Send key: ' + type);
      // On my device, the physical hardware buttons (sleep and volume)
      // send multiple events (press press release release), but the
      // soft home button just sends one.  This hack is to manually
      // "debounce" the keys. If the type of this event is the same as
      // the type of the last one, then don't send it.  We'll never send
      // two presses or two releases in a row.
      // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=761067
      if (type !== this.lastHardwareButtonEventType) {
        this.lastHardwareButtonEventType = type;
        this.sendChromeEvent({type: type});
      }
    },

    lastHardwareButtonEventType: null, // property for the hack above
  };

  window.addEventListener('load', function b2gshell_onload() {
    window.removeEventListener('load', b2gshell_onload);
    window.setTimeout(function() {
      shell.start();
    }, 10);
  });

  window.addEventListener('unload', function b2gshell_unload() {
    window.removeEventListener('unload', b2gshell_unload);
    window.removeEventListener('keydown', shell, true);
    window.removeEventListener('keyup', shell, true);
    window.removeEventListener('keypress', shell, true);
  });

  Services.obs.addObserver(function onWebappsReady(subject, topic, data) {
    shell.isWebappsRegistryReady = true;
  }, 'webapps-registry-ready', false);

  var WebappsHelper = {
    init: function webapps_init() {
      debug('Webapps helper is initialized.');
      Services.obs.addObserver(this, "webapps-launch", false);
      Services.obs.addObserver(this, "webapps-ask-install", false);
    },

    observe: function webapps_observe(subject, topic, data) {
      try {
      let json = JSON.parse(data);
      json.mm = subject;

      debug('observe: ' + topic);
      switch(topic) {
        case "webapps-launch":
          DOMApplicationRegistry.getManifestFor(json.origin, function(aManifest) {
            if (!aManifest)
              return;

            let manifest = new ManifestHelper(aManifest, json.origin);
            dump('Send chrome event to launch app: ' + json.manifestURL);
            shell.sendChromeEvent({
              "type": "webapps-launch",
              "url": manifest.fullLaunchPath(json.startPoint),
              "manifestURL": json.manifestURL
            });
          });
          break;
        case "webapps-ask-install":
          let id = this.registerInstaller(json);
          shell.sendChromeEvent({
            type: "webapps-ask-install",
            id: id,
            app: json.app
          });
          break;
      }
      } catch (e) {
        dump(e);
      }
    }
  };
})();

