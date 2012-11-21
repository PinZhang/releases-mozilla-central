Cu.import('resource://gre/modules/ObjectWrapper.jsm');

var B2GShell = {
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
    this.contentBrowser.addEventListener('DOMContentLoaded',
                                         function content_onload(event) {
      if (B2GShell.isWebappsRegistryReady) {
        // Fire chrome event, make sure the system APP got such a event
        window.setTimeout(function() {
          B2GShell.sendChromeEvent({ type: 'webapps-registry-ready' });
        }, 1000);
      }
    });
  },

  isWebappsRegistryReady: false
};

window.addEventListener('load', function b2gshell_onload() {
  window.removeEventListener('load', b2gshell_onload);
  window.setTimeout(function() {
    B2GShell.start();
  }, 10);
});

Services.obs.addObserver(function onWebappsReady(subject, topic, data) {
  B2GShell.isWebappsRegistryReady = true;
}, 'webapps-registry-ready', false);

