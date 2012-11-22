(function() {
  function debug(msg) {
    dump('**b2g shell: **' + msg + '\n');
  }

  Cu.import('resource://gre/modules/ObjectWrapper.jsm');
  Cu.import('resource://gre/modules/Webapps.jsm');
  Cu.import('resource://gre/modules/AppsUtils.jsm');

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
    }
  };

  window.addEventListener('load', function b2gshell_onload() {
    debug('!!load');
    window.removeEventListener('load', b2gshell_onload);
    window.setTimeout(function() {
      shell.start();
    }, 10);
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

