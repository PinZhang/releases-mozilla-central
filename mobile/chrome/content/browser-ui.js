// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "PluralForm", function() {
  Cu.import("resource://gre/modules/PluralForm.jsm");
  return PluralForm;
});

XPCOMUtils.defineLazyGetter(this, "PlacesUtils", function() {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils;
});

XPCOMUtils.defineLazyServiceGetter(window, "gHistSvc", "@mozilla.org/browser/nav-history-service;1", "nsINavHistoryService", "nsIBrowserHistory");
XPCOMUtils.defineLazyServiceGetter(window, "gURIFixup", "@mozilla.org/docshell/urifixup;1", "nsIURIFixup");
XPCOMUtils.defineLazyServiceGetter(window, "gFaviconService", "@mozilla.org/browser/favicon-service;1", "nsIFaviconService");
XPCOMUtils.defineLazyServiceGetter(window, "gFocusManager", "@mozilla.org/focus-manager;1", "nsIFocusManager");

[
  ["AllPagesList", "popup_autocomplete", "cmd_openLocation"],
  ["HistoryList", "history-items", "cmd_history"],
  ["BookmarkList", "bookmarks-items", "cmd_bookmarks"],
#ifdef MOZ_SERVICES_SYNC
  ["RemoteTabsList", "remotetabs-items", "cmd_remoteTabs"]
#endif
].forEach(function(aPanel) {
  let [name, id, command] = aPanel;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    return new AwesomePanel(id, command);
  });
});

/**
 * Cache of commonly used elements.
 */
let Elements = {};

[
  ["browserBundle",      "bundle_browser"],
  ["contentShowing",     "bcast_contentShowing"],
  ["urlbarState",        "bcast_urlbarState"],
  ["stack",              "stack"],
  ["tabs",               "tabs-container"],
  ["controls",           "browser-controls"],
  ["panelUI",            "panel-container"],
  ["viewBuffer",         "view-buffer"],
  ["toolbarContainer",   "toolbar-container"],
].forEach(function (aElementGlobal) {
  let [name, id] = aElementGlobal;
  XPCOMUtils.defineLazyGetter(Elements, name, function() {
    return document.getElementById(id);
  });
});

const TOOLBARSTATE_LOADING  = 1;
const TOOLBARSTATE_LOADED   = 2;

var BrowserUI = {
  _edit : null,
  _throbber : null,
  _favicon : null,
  _dialogs: [],

  _domWillOpenModalDialog: function(aBrowser) {
    // We're about to open a modal dialog, make sure the opening
    // tab is brought to the front.

    for (let i = 0; i < Browser.tabs.length; i++) {
      if (Browser._tabs[i].browser == aBrowser) {
        Browser.selectedTab = Browser.tabs[i];
        break;
      }
    }
  },

  _titleChanged: function(aBrowser) {
    var browser = Browser.selectedBrowser;
    if (browser && aBrowser != browser)
      return;

    var url = this.getDisplayURI(browser);
    var caption = browser.contentTitle || url;

    if (Util.isURLEmpty(url))
      caption = "";

    this._setURI(caption);
  },

  /*
   * Dispatched by window.close() to allow us to turn window closes into tabs
   * closes.
   */
  _domWindowClose: function(aBrowser) {
     // Find the relevant tab, and close it.
     let browsers = Browser.browsers;
     for (let i = 0; i < browsers.length; i++) {
      if (browsers[i] == aBrowser) {
        Browser.closeTab(Browser.getTabAtIndex(i));
        return { preventDefault: true };
      }
    }
  },

  _updateButtons: function(aBrowser) {
    let back = document.getElementById("cmd_back");
    let forward = document.getElementById("cmd_forward");

    back.setAttribute("disabled", !aBrowser.canGoBack);
    forward.setAttribute("disabled", !aBrowser.canGoForward);
  },

  _updateToolbar: function _updateToolbar() {
    let mode = Elements.urlbarState.getAttribute("mode");
    if (Browser.selectedTab.isLoading() && mode != "loading") {
      Elements.urlbarState.setAttribute("mode", "loading");
    }
    else if (mode != "view") {
      Elements.urlbarState.setAttribute("mode", "view");
    }
  },

  _tabSelect: function(aEvent) {
    let browser = Browser.selectedBrowser;
    this._titleChanged(browser);
    this._updateToolbar();
    this._updateButtons(browser);
    this._updateIcon(browser.mIconURL);
    this.updateStar();
  },

  showToolbar: function showToolbar(aEdit) {
    this.hidePanel();
    this._editURI(aEdit);
    if (aEdit)
      this.showAutoComplete();
  },

  _toolbarLocked: 0,

  isToolbarLocked: function isToolbarLocked() {
    return this._toolbarLocked;
  },

  lockToolbar: function lockToolbar() {
    this._toolbarLocked++;
    document.getElementById("toolbar-moveable-container").top = "0";
    if (this._toolbarLocked == 1)
      Browser.forceChromeReflow();
  },

  unlockToolbar: function unlockToolbar() {
    if (!this._toolbarLocked)
      return;

    this._toolbarLocked--;
    if (!this._toolbarLocked)
      document.getElementById("toolbar-moveable-container").top = "";
  },

  _setURI: function _setURI(aCaption) {
    if (this.isAutoCompleteOpen())
      this._edit.defaultValue = aCaption;
    else
      this._edit.value = aCaption;
  },

  _editURI: function _editURI(aEdit) {
    if (aEdit) {
      // If the urlbar is not opened yet, inform the broadcaster and then
      // save the current value as a default value to display once the awesome
      // panel will be dismissed
      let isOpened = this._edit.hasAttribute("open");
      if (!isOpened) {
        Elements.urlbarState.setAttribute("mode", "edit");
        this._edit.defaultValue = this._edit.value;

        // Now, replace the web page title by the url of the page
        let urlString = this.getDisplayURI(Browser.selectedBrowser);
        if (Util.isURLEmpty(urlString))
          urlString = "";
        this._edit.value = urlString;
      }

      // If the urlbar readOnly state is set to false or if the window is in
      // portrait then we refresh the IME state to display the VKB if any
      if (!this._edit.readOnly || Util.isPortrait()) {
        // This is a workaround needed to cycle focus for the IME state
        // to be set properly (bug 488420)
        this._edit.blur();
        gFocusManager.setFocus(this._edit, Ci.nsIFocusManager.FLAG_NOSCROLL);
      }

      this._edit.readOnly = false;
    }
    else if (!aEdit) {
      this._updateToolbar();
    }
  },

  updateAwesomeHeader: function updateAwesomeHeader(aVisible) {
    document.getElementById("awesome-header").hidden = aVisible;
  },

  _closeOrQuit: function _closeOrQuit() {
    // Close active dialog, if we have one. If not then close the application.
    if (this.activePanel) {
      this.activePanel = null;
    } else if (this.activeDialog) {
      this.activeDialog.close();
    } else {
      // Check to see if we should really close the window
      if (Browser.closing())
        window.close();
    }
  },

  _activePanel: null,
  get activePanel() {
    return this._activePanel;
  },

  set activePanel(aPanel) {
    if (this._activePanel == aPanel)
      return;

    let container = document.getElementById("awesome-panels");
    if (aPanel) {
      container.hidden = false;
      aPanel.open();
    } else {
      container.hidden = true;
      BrowserUI.showToolbar(false);
    }

    if (this._activePanel)
      this._activePanel.close();
    this._activePanel = aPanel;
  },

  get activeDialog() {
    // Return the topmost dialog
    if (this._dialogs.length)
      return this._dialogs[this._dialogs.length - 1];
    return null;
  },

  pushDialog: function pushDialog(aDialog) {
    // If we have a dialog push it on the stack and set the attr for CSS
    if (aDialog) {
      this.lockToolbar();
      this._dialogs.push(aDialog);
      document.getElementById("toolbar-main").setAttribute("dialog", "true");
      Elements.contentShowing.setAttribute("disabled", "true");
    }
  },

  popDialog: function popDialog() {
    if (this._dialogs.length) {
      this._dialogs.pop();
      this.unlockToolbar();
    }

    // If no more dialogs are being displayed, remove the attr for CSS
    if (!this._dialogs.length) {
      document.getElementById("toolbar-main").removeAttribute("dialog");
      Elements.contentShowing.removeAttribute("disabled");
    }
  },

  pushPopup: function pushPopup(aPanel, aElements) {
    this._hidePopup();
    this._popup =  { "panel": aPanel,
                     "elements": (aElements instanceof Array) ? aElements : [aElements] };
    this._dispatchPopupChanged();
  },

  popPopup: function popPopup() {
    this._popup = null;
    this._dispatchPopupChanged();
  },

  _dispatchPopupChanged: function _dispatchPopupChanged() {
    let stack = document.getElementById("stack");
    let event = document.createEvent("Events");
    event.initEvent("PopupChanged", true, false);
    event.popup = this._popup;
    stack.dispatchEvent(event);
  },

  _hidePopup: function _hidePopup() {
    if (!this._popup)
      return;
    let panel = this._popup.panel;
    if (panel.hide)
      panel.hide();
  },

  _isEventInsidePopup: function _isEventInsidePopup(aEvent) {
    if (!this._popup)
      return false;
    let elements = this._popup.elements;
    let targetNode = aEvent ? aEvent.target : null;
    while (targetNode && elements.indexOf(targetNode) == -1)
      targetNode = targetNode.parentNode;
    return targetNode ? true : false;
  },

  switchPane: function switchPane(id) {
    let button = document.getElementsByAttribute("linkedpanel", id)[0];
    if (button)
      button.checked = true;

    this.blurFocusedElement();

    let pane = document.getElementById(id);
    document.getElementById("panel-items").selectedPanel = pane;
  },

  get toolbarH() {
    if (!this._toolbarH) {
      let toolbar = document.getElementById("toolbar-main");
      this._toolbarH = toolbar.boxObject.height;
    }
    return this._toolbarH;
  },

  get sidebarW() {
    delete this._sidebarW;
    return this._sidebarW = Elements.controls.getBoundingClientRect().width;
  },

  get starButton() {
    delete this.starButton;
    return this.starButton = document.getElementById("tool-star");
  },

  sizeControls: function(windowW, windowH) {
    // tabs
    document.getElementById("tabs").resize();

    // awesomebar and related panels
    let popup = document.getElementById("awesome-panels");
    popup.top = this.toolbarH;
    popup.height = windowH - this.toolbarH;
    popup.width = windowW;

    // content navigator helper
    let contentHelper = document.getElementById("content-navigator");
    contentHelper.top = windowH - contentHelper.getBoundingClientRect().height;
  },

  init: function() {
    this._edit = document.getElementById("urlbar-edit");
    this._throbber = document.getElementById("urlbar-throbber");
    this._favicon = document.getElementById("urlbar-favicon");
    this._favicon.addEventListener("error", this, false);

    this._edit.addEventListener("click", this, false);
    this._edit.addEventListener("mousedown", this, false);

    BadgeHandlers.register(this._edit.popup);

    let awesomePopup = document.getElementById("popup_autocomplete");
    awesomePopup.addEventListener("popupshown", this, false);
    awesomePopup.addEventListener("popuphidden", this, false);

    document.getElementById("toolbar-main").ignoreDrag = true;

    let tabs = document.getElementById("tabs");
    tabs.addEventListener("TabSelect", this, true);
    tabs.addEventListener("TabOpen", this, true);

    // listen content messages
    messageManager.addMessageListener("DOMLinkAdded", this);
    messageManager.addMessageListener("DOMTitleChanged", this);
    messageManager.addMessageListener("DOMWillOpenModalDialog", this);
    messageManager.addMessageListener("DOMWindowClose", this);

    messageManager.addMessageListener("Browser:OpenURI", this);
    messageManager.addMessageListener("Browser:SaveAs:Return", this);

    // listening mousedown for automatically dismiss some popups (e.g. larry)
    window.addEventListener("mousedown", this, true);

    // listening escape to dismiss dialog on VK_ESCAPE
    window.addEventListener("keypress", this, true);

    // listening AppCommand to handle special keys
    window.addEventListener("AppCommand", this, true);

    // Push the panel initialization out of the startup path
    // (Using a message because we have no good way to delay-init [Bug 535366])
    messageManager.addMessageListener("DOMContentLoaded", function() {
      // We only want to delay one time
      messageManager.removeMessageListener("DOMContentLoaded", arguments.callee, true);

      // We unhide the panelUI so the XBL and settings can initialize
      Elements.panelUI.hidden = false;

      // Init the views
      ExtensionsView.init();
      DownloadsView.init();
      PreferencesView.init();
      ConsoleView.init();

#ifdef MOZ_IPC
      // Pre-start the content process
      Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
          .ensureContentProcess();
#endif

#ifdef MOZ_SERVICES_SYNC
      // Init the sync system
      WeaveGlue.init();
#endif
    });

    FormHelperUI.init();
    FindHelperUI.init();
    PageActions.init();
  },

  uninit: function() {
    ExtensionsView.uninit();
    ConsoleView.uninit();
    FormHelperUI.uninit();
  },

  update: function(aState) {
    let browser = Browser.selectedBrowser;

    switch (aState) {
      case TOOLBARSTATE_LOADED:
        if (Elements.urlbarState.getAttribute("mode") != "edit")
          this._updateToolbar();

        this._updateIcon(browser.mIconURL);
        this.unlockToolbar();
        break;

      case TOOLBARSTATE_LOADING:
        if (Elements.urlbarState.getAttribute("mode") != "edit")
          this._updateToolbar();

        browser.mIconURL = "";
        this._updateIcon();
        this.lockToolbar();
        break;
    }
  },

  _updateIcon: function(aIconSrc) {
    this._favicon.src = aIconSrc || "";
    if (Browser.selectedTab.isLoading()) {
      this._throbber.hidden = false;
      this._throbber.setAttribute("loading", "true");
      this._favicon.hidden = true;
    }
    else {
      this._favicon.hidden = false;
      this._throbber.hidden = true;
      this._throbber.removeAttribute("loading");
    }
  },

  getDisplayURI: function(browser) {
    let uri = browser.currentURI;
    try {
      uri = gURIFixup.createExposableURI(uri);
    } catch (ex) {}

    return uri.spec;
  },

  /* Set the location to the current content */
  updateURI: function() {
    var browser = Browser.selectedBrowser;

    // FIXME: deckbrowser should not fire TabSelect on the initial tab (bug 454028)
    if (!browser.currentURI)
      return;

    // Update the navigation buttons
    this._updateButtons(browser);

    // Check for a bookmarked page
    this.updateStar();

    var urlString = this.getDisplayURI(browser);
    if (Util.isURLEmpty(urlString))
      urlString = "";

    this._setURI(urlString);
  },

  goToURI: function(aURI) {
    aURI = aURI || this._edit.value;
    if (!aURI)
      return;

    // Make sure we're online before attempting to load
    Util.forceOnline();

    // Give the new page lots of room
    Browser.hideSidebars();
    this.closeAutoComplete(true);

    this._edit.value = aURI;

    Browser.loadURI(aURI, { flags: Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP });

    // Delay doing the fixup so the raw URI is passed to loadURIWithFlags
    // and the proper third-party fixup can be done
    let fixupFlags = Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    let uri = gURIFixup.createFixupURI(aURI, fixupFlags);
    gHistSvc.markPageAsTyped(uri);
  },

  showAutoComplete: function showAutoComplete() {
    if (this.isAutoCompleteOpen())
      return;

    this._hidePopup();
    this.activePanel = AllPagesList;
  },

  closeAutoComplete: function closeAutoComplete(aResetInput) {
    if (!this.isAutoCompleteOpen())
      return;

    if (aResetInput)
      this._edit.popup.close();
    else
      this._edit.popup.closePopup();

    // Because the controller is not detached during a blur event for Meego
    // compatibility with the VKB, we need to detach it manually
    this._edit.detachController();
    this.activePanel = null;
  },

  isAutoCompleteOpen: function isAutoCompleteOpen() {
    return this._edit.popup.popupOpen;
  },

  doOpenSearch: function doOpenSearch(aName) {
    // save the current value of the urlbar
    let searchValue = this._edit.value;

    // Give the new page lots of room
    Browser.hideSidebars();
    this.closeAutoComplete(false);

    // Make sure we're online before attempting to load
    Util.forceOnline();

    let engine = Services.search.getEngineByName(aName);
    let submission = engine.getSubmission(searchValue, null);
    Browser.loadURI(submission.uri.spec, { postData: submission.postData });
  },

  updateStar: function() {
    if (PlacesUtils.getMostRecentBookmarkForURI(Browser.selectedBrowser.currentURI) != -1)
      this.starButton.setAttribute("starred", "true");
    else
      this.starButton.removeAttribute("starred");
  },

  newTab: function newTab(aURI, aOwner) {
    aURI = aURI || "about:blank";
    let tab = Browser.addTab(aURI, true, aOwner);

    this.hidePanel();

    if (aURI == "about:blank") {
      // Display awesomebar UI
      this.showToolbar(true);
    }
    else {
      // Give the new page lots of room
      Browser.hideSidebars();
      this.closeAutoComplete(true);
    }

    return tab;
  },

  newOrSelectTab: function newOrSelectTab(aURI, aOwner) {
    let tabs = Browser.tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.currentURI.spec == aURI) {
        Browser.selectedTab = tabs[i];
        return;
      }
    }
    this.newTab(aURI, aOwner);
  },

  closeTab: function closeTab(aTab) {
    // If no tab is passed in, assume the current tab
    Browser.closeTab(aTab || Browser.selectedTab);
  },

  selectTab: function selectTab(aTab) {
    this.activePanel = null;
    Browser.selectedTab = aTab;
  },

  undoCloseTab: function undoCloseTab(aIndex) {
    let tab = null;
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    if (ss.getClosedTabCount(window) > (aIndex || 0)) {
      tab = ss.undoCloseTab(window, aIndex || 0);
    }
    return tab;
  },

  isTabsVisible: function isTabsVisible() {
    // The _1, _2 and _3 are to make the js2 emacs mode happy
    let [leftvis,_1,_2,_3] = Browser.computeSidebarVisibility();
    return (leftvis > 0.002);
  },

  showPanel: function showPanel(aPage) {
    if (this.activePanel)
      this.activePanel = null;

    Elements.panelUI.left = 0;
    Elements.panelUI.hidden = false;
    Elements.contentShowing.setAttribute("disabled", "true");

    if (aPage != undefined)
      this.switchPane(aPage);
  },

  hidePanel: function hidePanel() {
    if (!this.isPanelVisible())
      return;
    Elements.panelUI.hidden = true;
    Elements.contentShowing.removeAttribute("disabled");
    this.blurFocusedElement();
  },

  isPanelVisible: function isPanelVisible() {
    return (!Elements.panelUI.hidden && Elements.panelUI.left == 0);
  },

  blurFocusedElement: function blurFocusedElement() {
    let focusedElement = document.commandDispatcher.focusedElement;
    if (focusedElement)
      focusedElement.blur();
  },

  switchTask: function switchTask() {
    try {
      let phone = Cc["@mozilla.org/phone/support;1"].createInstance(Ci.nsIPhoneSupport);
      phone.switchTask();
    } catch(e) { }
  },

  handleEscape: function (aEvent) {
    aEvent.stopPropagation();

    // Check open popups
    if (this._popup) {
      this._hidePopup();
      return;
    }

    // Check active panel
    if (this.activePanel) {
      this.activePanel = null;
      return;
    }

    // Check open dialogs
    let dialog = this.activeDialog;
    if (dialog) {
      dialog.close();
      return;
    }

    // Check open modal elements
    let modalElementsLength = document.getElementsByClassName("modal-block").length;
    if (modalElementsLength > 0)
      return;

    // Check open panel
    if (this.isPanelVisible()) {
      this.hidePanel();
      return;
    }

    // Check content helper
    let contentHelper = document.getElementById("content-navigator");
    if (contentHelper.isActive) {
      contentHelper.hide();
      return;
    }

    // Only if there are no dialogs, popups, or panels open
    let tab = Browser.selectedTab;
    let browser = tab.browser;

    if (browser.canGoBack)
      browser.goBack();
    else if (tab.owner)
      this.closeTab(tab);
#ifdef ANDROID
    else
      window.QueryInterface(Ci.nsIDOMChromeWindow).minimize();
#endif
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      // Browser events
      case "TabSelect":
        this._tabSelect(aEvent);
        break;
      case "TabOpen":
      {
        let [tabsVisibility,,,] = Browser.computeSidebarVisibility();
        if (!(tabsVisibility == 1.0) && Browser.selectedTab.chromeTab != aEvent.target)
          NewTabPopup.show(aEvent.target);

        // Workaround to hide the tabstrip if it is partially visible
        // See bug 524469
        if (tabsVisibility > 0.0 && tabsVisibility < 1.0)
          Browser.hideSidebars();

        break;
      }
      // Window events
      case "keypress":
        if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE)
          this.handleEscape(aEvent);
        break;
      case "AppCommand":
        aEvent.stopPropagation();
        switch (aEvent.command) {
          case "Menu":
            this.doCommand("cmd_menu");
            break;
          case "Search":
            this.doCommand("cmd_openLocation");
            break;
          default:
            break;
        }
        break;
      // URL textbox events
      case "click":
        this.doCommand("cmd_openLocation");
        break;
      case "mousedown":
        if (!this._isEventInsidePopup(aEvent))
          this._hidePopup();

        let selectAll = Services.prefs.getBoolPref("browser.urlbar.doubleClickSelectsAll");
        if (aEvent.detail == 2 && aEvent.button == 0 && selectAll && aEvent.target == this._edit) {
          this._edit.editor.selectAll();
          aEvent.preventDefault();
        }
        break;
      // Favicon events
      case "error":
        this._favicon.src = "";
        break;
      // Awesome popup event
      case "popupshown":
        this._edit.setAttribute("open", "true");
        break;
      case "popuphidden":
        this._edit.removeAttribute("open");
        this._edit.readOnly = true;
        break;
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    let browser = aMessage.target;
    let json = aMessage.json;
    switch (aMessage.name) {
      case "DOMTitleChanged":
        this._titleChanged(browser);
        break;
      case "DOMWillOpenModalDialog":
        return this._domWillOpenModalDialog(browser);
        break;
      case "DOMWindowClose":
        return this._domWindowClose(browser);
        break;
      case "DOMLinkAdded":
        if (Browser.selectedBrowser == browser)
          this._updateIcon(Browser.selectedBrowser.mIconURL);
        break;
      case "Browser:SaveAs:Return":
        if (json.type != Ci.nsIPrintSettings.kOutputFormatPDF)
          return;

        let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
        let db = dm.DBConnection;
        let stmt = db.createStatement("UPDATE moz_downloads SET endTime = :endTime, state = :state WHERE id = :id");
        stmt.params.endTime = Date.now() * 1000;
        stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_FINISHED;
        stmt.params.id = json.id;
        stmt.execute();
        stmt.finalize();

        let download = dm.getDownload(json.id);
        try {
          DownloadsView.downloadCompleted(download);
          let element = DownloadsView.getElementForDownload(json.id);
          element.setAttribute("state", Ci.nsIDownloadManager.DOWNLOAD_FINISHED);
          element.setAttribute("endTime", Date.now());
          element.setAttribute("referrer", json.referrer);
          DownloadsView._updateTime(element);
          DownloadsView._updateStatus(element);
        }
        catch(e) {}
        Services.obs.notifyObservers(download, "dl-done", null);
        break;

      case "Browser:OpenURI":
        Browser.addTab(json.uri, json.bringFront, Browser.selectedTab);
        break;
    }
  },

  supportsCommand : function(cmd) {
    var isSupported = false;
    switch (cmd) {
      case "cmd_back":
      case "cmd_forward":
      case "cmd_reload":
      case "cmd_forceReload":
      case "cmd_stop":
      case "cmd_go":
      case "cmd_openLocation":
      case "cmd_star":
      case "cmd_opensearch":
      case "cmd_bookmarks":
      case "cmd_history":
      case "cmd_remoteTabs":
      case "cmd_quit":
      case "cmd_close":
      case "cmd_menu":
      case "cmd_newTab":
      case "cmd_closeTab":
      case "cmd_undoCloseTab":
      case "cmd_actions":
      case "cmd_panel":
      case "cmd_sanitize":
      case "cmd_zoomin":
      case "cmd_zoomout":
      case "cmd_volumeLeft":
      case "cmd_volumeRight":
      case "cmd_lockscreen":
        isSupported = true;
        break;
      default:
        isSupported = false;
        break;
    }
    return isSupported;
  },

  isCommandEnabled : function(cmd) {
    let elem = document.getElementById(cmd);
    if (elem && (elem.getAttribute("disabled") == "true"))
      return false;
    return true;
  },

  doCommand : function(cmd) {
    if (!this.isCommandEnabled(cmd))
      return;
    let browser = getBrowser();
    switch (cmd) {
      case "cmd_back":
        browser.goBack();
        break;
      case "cmd_forward":
        browser.goForward();
        break;
      case "cmd_reload":
        browser.reload();
        break;
      case "cmd_forceReload":
      {
        // Simulate a new page
        browser.lastLocation = null;

        const reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY |
                            Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        browser.reloadWithFlags(reloadFlags);
        break;
      }
      case "cmd_stop":
        browser.stop();
        break;
      case "cmd_go":
        this.goToURI();
        break;
      case "cmd_openLocation":
        this.showToolbar(true);
        break;
      case "cmd_star":
      {
        let bookmarkURI = browser.currentURI;
        let autoClose = false;

        if (PlacesUtils.getMostRecentBookmarkForURI(bookmarkURI) == -1) {
          let bookmarkTitle = browser.contentTitle || bookmarkURI.spec;
          let bookmarkService = PlacesUtils.bookmarks;
          let bookmarkId = bookmarkService.insertBookmark(BookmarkList.panel.mobileRoot, bookmarkURI,
                                                          bookmarkService.DEFAULT_INDEX,
                                                          bookmarkTitle);
          this.updateStar();

          // autoclose the bookmark popup
          autoClose = true;
        }

        // Show/hide bookmark popup
        BookmarkPopup.toggle(autoClose);
        break;
      }
      case "cmd_opensearch":
        this._edit.blur();

        MenuListHelperUI.show({
          title: Elements.browserBundle.getString("opensearch.searchWith"),
          menupopup: { children: BrowserSearch.engines },
          set selectedIndex(aIndex) {
            let name = this.menupopup.children[aIndex].label;
            BrowserUI.doOpenSearch(name);
          }
        });
        break;
      case "cmd_bookmarks":
        this.activePanel = BookmarkList;
        break;
      case "cmd_history":
        this.activePanel = HistoryList;
        break;
      case "cmd_remoteTabs":
        this.activePanel = RemoteTabsList;
        break;
      case "cmd_quit":
        goQuitApplication();
        break;
      case "cmd_close":
        this._closeOrQuit();
        break;
      case "cmd_menu":
        getIdentityHandler().toggle();
        break;
      case "cmd_newTab":
        this.newTab();
        break;
      case "cmd_closeTab":
        this.closeTab();
        break;
      case "cmd_undoCloseTab":
        this.undoCloseTab();
        break;
      case "cmd_sanitize":
      {
        // disable the button temporarily to indicate something happened
        let button = document.getElementById("prefs-clear-data");
        button.disabled = true;
        setTimeout(function() { button.disabled = false; }, 5000);

        Sanitizer.sanitize();
        break;
      }
      case "cmd_panel":
      {
        if (BrowserUI.isPanelVisible())
          this.hidePanel();
        else
          this.showPanel();
        break;
      }
      case "cmd_zoomin":
        Browser.zoom(-1);
        break;
      case "cmd_zoomout":
        Browser.zoom(1);
        break;
      case "cmd_volumeLeft":
        // Zoom in (portrait) or out (landscape)
        Browser.zoom(Util.isPortrait() ? -1 : 1);
        break;
      case "cmd_volumeRight":
        // Zoom out (portrait) or in (landscape)
        Browser.zoom(Util.isPortrait() ? 1 : -1);
        break;
      case "cmd_lockscreen":
      {
        let locked = Services.prefs.getBoolPref("toolkit.screen.lock");
        Services.prefs.setBoolPref("toolkit.screen.lock", !locked);

        let strings = Elements.browserBundle;
        let alerts = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        alerts.showAlertNotification(null, strings.getString("alertLockScreen"),
                                     strings.getString("alertLockScreen." + (!locked ? "locked" : "unlocked")), false, "", null);
        break;
      }
    }
  }
};

var TapHighlightHelper = {
  get _overlay() {
    delete this._overlay;
    return this._overlay = document.getElementById("content-overlay");
  },

  show: function show(aRects) {
    let browser = getBrowser();
    let scroll = browser.getPosition();

    let canvasArea = aRects.reduce(function(a, b) {
      return a.expandToContain(b);
    }, new Rect(0, 0, 0, 0)).map(function(val) val * browser.scale)
                            .translate(-scroll.x, -scroll.y);

    let overlay = this._overlay;
    overlay.width = canvasArea.width;
    overlay.style.width = canvasArea.width + "px";
    overlay.height = canvasArea.height;
    overlay.style.height = canvasArea.height + "px";

    let ctx = overlay.getContext("2d");
    ctx.save();
    ctx.translate(-canvasArea.left, -canvasArea.top);
    ctx.scale(browser.scale, browser.scale);

    overlay.style.left = canvasArea.left + "px";
    overlay.style.top = canvasArea.top + "px";
    ctx.fillStyle = "rgba(0, 145, 255, .5)";
    for (let i = aRects.length - 1; i >= 0; i--) {
      let rect = aRects[i];
      ctx.fillRect(rect.left - scroll.x / browser.scale, rect.top - scroll.y / browser.scale, rect.width, rect.height);
    }
    ctx.restore();
    overlay.style.display = "block";

    addEventListener("MozBeforePaint", this, false);
    mozRequestAnimationFrame();
  },

  /**
   * Hide the highlight. aGuaranteeShowMsecs specifies how many milliseconds the
   * highlight should be shown before it disappears.
   */
  hide: function hide(aGuaranteeShowMsecs) {
    if (this._overlay.style.display == "none")
      return;

    this._guaranteeShow = Math.max(0, aGuaranteeShowMsecs);
    if (this._guaranteeShow) {
      // _shownAt is set once highlight has been painted
      if (this._shownAt)
        setTimeout(this._hide.bind(this),
                   Math.max(0, this._guaranteeShow - (mozAnimationStartTime - this._shownAt)));
    } else {
      this._hide();
    }
  },

  /** Helper function that hides popup immediately. */
  _hide: function _hide() {
    this._shownAt = 0;
    this._guaranteeShow = 0;
    this._overlay.style.display = "none";
  },

  handleEvent: function handleEvent(ev) {
    removeEventListener("MozBeforePaint", this, false);
    this._shownAt = ev.timeStamp;
    // hide has been called, so hide the tap highlight after it has
    // been shown for a moment.
    if (this._guaranteeShow)
      this.hide(this._guaranteeShow);
  }
};

var PageActions = {
  init: function init() {
    document.getElementById("pageactions-container").addEventListener("click", this, false);

    this.register("pageaction-reset", this.updatePagePermissions, this);
    this.register("pageaction-password", this.updateForgetPassword, this);
#ifdef NS_PRINTING
    this.register("pageaction-saveas", this.updatePageSaveAs, this);
#endif
    this.register("pageaction-share", this.updateShare, this);
    this.register("pageaction-search", BrowserSearch.updatePageSearchEngines, BrowserSearch);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        getIdentityHandler().hide();
        break;
    }
  },

  /**
   * @param aId id of a pageaction element
   * @param aCallback function that takes an element and returns true if it should be visible
   * @param aThisObj (optional) scope object for aCallback
   */
  register: function register(aId, aCallback, aThisObj) {
    this._handlers.push({id: aId, callback: aCallback, obj: aThisObj});
  },

  _handlers: [],

  updateSiteMenu: function updateSiteMenu() {
    this._handlers.forEach(function(action) {
      let node = document.getElementById(action.id);
      node.hidden = !action.callback.call(action.obj, node);
    });
    this._updateAttributes();
  },

  get _loginManager() {
    delete this._loginManager;
    return this._loginManager = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  },

  // This is easy for an addon to add his own perm type here
  _permissions: ["popup", "offline-app", "geo"],

  _forEachPermissions: function _forEachPermissions(aHost, aCallback) {
    let pm = Services.perms;
    for (let i = 0; i < this._permissions.length; i++) {
      let type = this._permissions[i];
      if (!pm.testPermission(aHost, type))
        continue;

      let perms = pm.enumerator;
      while (perms.hasMoreElements()) {
        let permission = perms.getNext().QueryInterface(Ci.nsIPermission);
        if (permission.host == aHost.asciiHost && permission.type == type)
          aCallback(type);
      }
    }
  },

  updatePagePermissions: function updatePagePermissions(aNode) {
    let host = Browser.selectedBrowser.currentURI;
    let permissions = [];

    this._forEachPermissions(host, function(aType) {
      permissions.push("pageactions." + aType);
    });

    if (!this._loginManager.getLoginSavingEnabled(host.prePath)) {
      permissions.push("pageactions.password");
    }

    let descriptions = permissions.map(function(s) Elements.browserBundle.getString(s));
    aNode.setAttribute("description", descriptions.join(", "));

    return (permissions.length > 0);
  },

  updateForgetPassword: function updateForgetPassword(aNode) {
    let host = Browser.selectedBrowser.currentURI;
    let logins = this._loginManager.findLogins({}, host.prePath, "", null);

    return logins.some(function(login) login.hostname == host.prePath);
  },

  forgetPassword: function forgetPassword(aEvent) {
    let host = Browser.selectedBrowser.currentURI;
    let lm = this._loginManager;

    lm.findLogins({}, host.prePath, "", null).forEach(function(login) {
      if (login.hostname == host.prePath)
        lm.removeLogin(login);
    });

    this.hideItem(aEvent.target);
    aEvent.stopPropagation(); // Don't hide the site menu.
  },

  clearPagePermissions: function clearPagePermissions(aEvent) {
    let pm = Services.perms;
    let host = Browser.selectedBrowser.currentURI;
    this._forEachPermissions(host, function(aType) {
      pm.remove(host.asciiHost, aType);
    });

    let lm = this._loginManager;
    if (!lm.getLoginSavingEnabled(host.prePath))
      lm.setLoginSavingEnabled(host.prePath, true);

    this.hideItem(aEvent.target);
    aEvent.stopPropagation(); // Don't hide the site menu.
  },

  savePageAsPDF: function saveAsPDF() {
    let browser = Browser.selectedBrowser;
    let fileName = getDefaultFileName(browser.contentTitle, browser.documentURI, null, null);
    fileName = fileName.trim() + ".pdf";
#ifdef MOZ_PLATFORM_MAEMO
    fileName = fileName.replace(/[\*\:\?]+/g, " ");
#endif

    let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    let downloadsDir = dm.defaultDownloadsDirectory;

#ifdef ANDROID
    let file = downloadsDir.clone();
    file.append(fileName);
#else
    let strings = Elements.browserBundle;
    let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    picker.init(window, strings.getString("pageactions.saveas.pdf"), Ci.nsIFilePicker.modeSave);
    picker.appendFilter("PDF", "*.pdf");
    picker.defaultExtension = "pdf";

    picker.defaultString = fileName;

    picker.displayDirectory = downloadsDir;
    let rv = picker.show();
    if (rv == Ci.nsIFilePicker.returnCancel)
      return;

    let file = picker.file;
#endif

    // We must manually add this to the download system
    let db = dm.DBConnection;

    let stmt = db.createStatement(
      "INSERT INTO moz_downloads (name, source, target, startTime, endTime, state, referrer) " +
      "VALUES (:name, :source, :target, :startTime, :endTime, :state, :referrer)"
    );

    let current = browser.currentURI.spec;
    stmt.params.name = file.leafName;
    stmt.params.source = current;
    stmt.params.target = Services.io.newFileURI(file).spec;
    stmt.params.startTime = Date.now() * 1000;
    stmt.params.endTime = Date.now() * 1000;
    stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED;
    stmt.params.referrer = current;
    stmt.execute();
    stmt.finalize();

    let newItemId = db.lastInsertRowID;
    let download = dm.getDownload(newItemId);
    try {
      DownloadsView.downloadStarted(download);
    }
    catch(e) {}
    Services.obs.notifyObservers(download, "dl-start", null);

    let data = {
      type: Ci.nsIPrintSettings.kOutputFormatPDF,
      id: newItemId,
      referrer: current,
      filePath: file.path
    };

    Browser.selectedBrowser.messageManager.sendAsyncMessage("Browser:SaveAs", data);
  },

  updatePageSaveAs: function updatePageSaveAs(aNode) {
    // Check for local XUL content
    let contentWindow = Browser.selectedBrowser.contentWindow;
    return !(contentWindow && contentWindow.document instanceof XULDocument);
  },

  updateShare: function updateShare(aNode) {
    return Util.isShareableScheme(Browser.selectedBrowser.currentURI.scheme);
  },

  hideItem: function hideItem(aNode) {
    aNode.hidden = true;
    this._updateAttributes();
  },

  _updateAttributes: function _updateAttributes() {
    let container = document.getElementById("pageactions-container");
    let visibleNodes = container.querySelectorAll("pageaction:not([hidden=true])");
    let visibleCount = visibleNodes.length;

    let first = null, last = null;
    for (let i = 0; i < visibleCount; i++) {
      let node = visibleNodes[i];
      node.removeAttribute("selector");
      // Note: CSS indexes start at one, so even/odd are swapped.
      node.setAttribute("even", (i % 2) ? "true" : "false");
    }

    if (visibleCount >= 1) {
      visibleNodes[visibleCount - 1].setAttribute("selector", "last-child");
      visibleNodes[0].setAttribute("selector", "first-child");
    }

    if (visibleCount >= 2) {
      visibleNodes[visibleCount - 2].setAttribute("selector", "second-last-child");
      visibleNodes[0].setAttribute("selector", "first-child");
      visibleNodes[1].setAttribute("selector", "second-child");
    }
  }
};

var NewTabPopup = {
  _timeout: 0,
  _tabs: [],

  get box() {
    delete this.box;
    let box = document.getElementById("newtab-popup");

    // Move the popup on the other side if we are in RTL
    let [leftSidebar, rightSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    if (leftSidebar.left > rightSidebar.left) {
      let margin = box.getAttribute("left");
      box.removeAttribute("left");
      box.setAttribute("right", margin);
    }

    return this.box = box;
  },

  _updateLabel: function() {
    let newtabStrings = Elements.browserBundle.getString("newtabpopup.opened");
    let label = PluralForm.get(this._tabs.length, newtabStrings).replace("#1", this._tabs.length);

    this.box.firstChild.setAttribute("value", label);
  },

  hide: function() {
    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = 0;
    }

    this._tabs = [];
    this.box.hidden = true;
    BrowserUI.popPopup();
  },

  show: function(aTab) {
    BrowserUI.pushPopup(this, this.box);

    this._tabs.push(aTab);
    this._updateLabel();

    this.box.top = aTab.getBoundingClientRect().top + (aTab.getBoundingClientRect().height / 3);
    this.box.hidden = false;

    if (this._timeout)
      clearTimeout(this._timeout);

    this._timeout = setTimeout(function(self) {
      self.hide();
    }, 2000, this);
  },

  selectTab: function() {
    BrowserUI.selectTab(this._tabs.pop());
    this.hide();
  }
};

var AwesomePanel = function(aElementId, aCommandId) {
  let command = document.getElementById(aCommandId);

  this.panel = document.getElementById(aElementId),

  this.open = function aw_open() {
    BrowserUI.pushDialog(this);
    command.setAttribute("checked", "true");
    this.panel.hidden = false;

    if (this.panel.hasAttribute("onshow")) {
      let func = new Function("panel", this.panel.getAttribute("onshow"));
      func.call(this.panel);
    }

    if (this.panel.open)
      this.panel.open();
  },

  this.close = function aw_close() {
    if (this.panel.hasAttribute("onhide")) {
      let func = new Function("panel", this.panel.getAttribute("onhide"));
      func.call(this.panel);
    }

    if (this.panel.close)
      this.panel.close();

    this.panel.blur();
    this.panel.hidden = true;
    command.removeAttribute("checked", "true");
    BrowserUI.popDialog();
  },

  this.openLink = function aw_openLink(aEvent) {
    let item = aEvent.originalTarget;
    let uri = item.getAttribute("url") || item.getAttribute("uri");
    if (uri != "") {
      BrowserUI.goToURI(uri);
      BrowserUI.activePanel = null;
    }
  }
};

var BookmarkPopup = {
  get box() {
    delete this.box;
    this.box = document.getElementById("bookmark-popup");

    const margin = 10;
    let [tabsSidebar, controlsSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    this.box.setAttribute(tabsSidebar.left < controlsSidebar.left ? "right" : "left", controlsSidebar.width + margin);
    this.box.top  = BrowserUI.starButton.getBoundingClientRect().top + margin;

    // Hide the popup if there is any new page loading
    let self = this;
    messageManager.addMessageListener("pagehide", function(aMessage) {
      self.hide();
    });

    return this.box;
  },

  _bookmarkPopupTimeout: -1,

  hide : function hide() {
    if (this._bookmarkPopupTimeout != -1) {
      clearTimeout(this._bookmarkPopupTimeout);
      this._bookmarkPopupTimeout = -1;
    }
    this.box.hidden = true;
    BrowserUI.popPopup();
  },

  show : function show(aAutoClose) {
    this.box.hidden = false;

    if (aAutoClose) {
      this._bookmarkPopupTimeout = setTimeout(function (self) {
        self._bookmarkPopupTimeout = -1;
        self.hide();
      }, 2000, this);
    }

    // include starButton here, so that click-to-dismiss works as expected
    BrowserUI.pushPopup(this, [this.box, BrowserUI.starButton]);
  },

  toggle : function toggle(aAutoClose) {
    if (this.box.hidden)
      this.show(aAutoClose);
    else
      this.hide();
  }
};

var BookmarkHelper = {
  _panel: null,
  _editor: null,

  edit: function BH_edit(aURI) {
    if (!aURI)
      aURI = getBrowser().currentURI;

    let itemId = PlacesUtils.getMostRecentBookmarkForURI(aURI);
    if (itemId == -1)
      return;

    let title = PlacesUtils.bookmarks.getItemTitle(itemId);
    let tags = PlacesUtils.tagging.getTagsForURI(aURI, {});

    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    this._editor = document.createElementNS(XULNS, "placeitem");
    this._editor.setAttribute("id", "bookmark-item");
    this._editor.setAttribute("flex", "1");
    this._editor.setAttribute("type", "bookmark");
    this._editor.setAttribute("ui", "manage");
    this._editor.setAttribute("title", title);
    this._editor.setAttribute("uri", aURI.spec);
    this._editor.setAttribute("itemid", itemId);
    this._editor.setAttribute("tags", tags.join(", "));
    this._editor.setAttribute("onclose", "BookmarkHelper.hide()");
    document.getElementById("bookmark-form").appendChild(this._editor);

    let toolbar = document.getElementById("toolbar-main");
    let top = toolbar.top + toolbar.boxObject.height;

    this._panel = document.getElementById("bookmark-container");
    this._panel.top = (top < 0 ? 0 : top);
    this._panel.hidden = false;
    BrowserUI.pushPopup(this, this._panel);

    let self = this;
    Browser.forceChromeReflow();
    self._editor.startEditing();
  },

  save: function BH_save() {
    this._editor.stopEditing(true);
  },

  hide: function BH_hide() {
    BrowserUI.updateStar();

    // Note: the _editor will have already saved the data, if needed, by the time
    // this method is called, since this method is called via the "close" event.
    this._editor.parentNode.removeChild(this._editor);
    this._editor = null;

    this._panel.hidden = true;
    BrowserUI.popPopup();
  },

  removeBookmarksForURI: function BH_removeBookmarksForURI(aURI) {
    //XXX blargle xpconnect! might not matter, but a method on
    // nsINavBookmarksService that takes an array of items to
    // delete would be faster. better yet, a method that takes a URI!
    let itemIds = PlacesUtils.getBookmarksForURI(aURI);
    itemIds.forEach(PlacesUtils.bookmarks.removeItem);

    BrowserUI.updateStar();
  }
};

var FindHelperUI = {
  type: "find",
  commands: {
    next: "cmd_findNext",
    previous: "cmd_findPrevious",
    close: "cmd_findClose"
  },

  init: function findHelperInit() {
    this._textbox = document.getElementById("find-helper-textbox");
    this._container = document.getElementById("content-navigator");

    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    // Listen for form assistant messages from content
    messageManager.addMessageListener("FindAssist:Show", this);
    messageManager.addMessageListener("FindAssist:Hide", this);

    // Listen for events where form assistant should be closed
    document.getElementById("tabs").addEventListener("TabSelect", this, true);
    document.getElementById("browsers").addEventListener("URLChanged", this, true);
  },

  receiveMessage: function findHelperReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch(aMessage.name) {
      case "FindAssist:Show":
        if (json.rect)
          this._zoom(Rect.fromRect(json.rect));
        break;

      case "FindAssist:Hide":
        if (this._container.getAttribute("type") == this.type)
          this.hide();
        break;
    }
  },

  handleEvent: function findHelperHandleEvent(aEvent) {
    if (aEvent.type == "TabSelect" || aEvent.type == "URLChanged")
      this.hide();
  },

  show: function findHelperShow() {
    this._container.show(this);
    this.search("");
    this._textbox.focus();
  },

  hide: function findHelperHide() {
    this._textbox.value = "";
    this._container.hide(this);
  },

  goToPrevious: function findHelperGoToPrevious() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Previous", { });
  },

  goToNext: function findHelperGoToNext() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Next", { });
  },

  search: function findHelperSearch(aValue) {
    this.updateCommands(aValue);
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Find", { searchString: aValue });
  },

  updateCommands: function findHelperUpdateCommands(aValue) {
    this._cmdPrevious.setAttribute("disabled", aValue == "");
    this._cmdNext.setAttribute("disabled", aValue == "");
  },

  _zoom: function _findHelperZoom(aElementRect) {
    // Zoom to a specified Rect
    if (aElementRect && Browser.selectedTab.allowZoom && Services.prefs.getBoolPref("findhelper.autozoom")) {
      let zoomLevel = Browser._getZoomLevelForRect(aElementRect);
      zoomLevel = Math.min(Math.max(kBrowserFormZoomLevelMin, zoomLevel), kBrowserFormZoomLevelMax);

      let zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, zoomLevel);
      Browser.animatedZoomTo(zoomRect);
    }
  }
};

/**
 * Responsible for navigating forms and filling in information.
 *  - Navigating forms is handled by next and previous commands.
 *  - When an element is focused, the browser view zooms in to the control.
 *  - The caret positionning and the view are sync to keep the type
 *    in text into view for input fields (text/textarea).
 *  - Provides autocomplete box for input fields.
 */
var FormHelperUI = {
  type: "form",
  commands: {
    next: "cmd_formNext",
    previous: "cmd_formPrevious",
    close: "cmd_formClose"
  },

  //for resize/rotate case
  _currentCaretRect: null,
  _currentElementRect: null,

  init: function formHelperInit() {
    this._container = document.getElementById("content-navigator");
    this._autofillContainer = document.getElementById("form-helper-autofill");
    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);
    this._visibleScreenArea = new Rect(0, 0, 0, 0);

    // Listen for form assistant messages from content
    messageManager.addMessageListener("FormAssist:Show", this);
    messageManager.addMessageListener("FormAssist:Hide", this);
    messageManager.addMessageListener("FormAssist:Update", this);
    messageManager.addMessageListener("FormAssist:Resize", this);
    messageManager.addMessageListener("FormAssist:AutoComplete", this);

    // Listen for events where form assistant should be closed
    document.getElementById("tabs").addEventListener("TabSelect", this, true);
    document.getElementById("browsers").addEventListener("URLChanged", this, true);

    // Listen for modal dialog to show/hide the UI
    messageManager.addMessageListener("DOMWillOpenModalDialog", this);
    messageManager.addMessageListener("DOMModalDialogClosed", this);

    Services.obs.addObserver(this, "softkb-change", false);
  },

  uninit: function formHelperUninit() {
    Services.obs.removeObserver(this, "softkb-change");
  },

  show: function formHelperShow(aElement, aHasPrevious, aHasNext) {
    this._open = true;

    // Update the next/previous commands
    this._cmdPrevious.setAttribute("disabled", !aHasPrevious);
    this._cmdNext.setAttribute("disabled", !aHasNext);

    let lastElement = this._currentElement || null;
    this._currentElement = {
      id: aElement.id,
      name: aElement.name,
      value: aElement.value,
      maxLength: aElement.maxLength,
      type: aElement.type,
      isAutocomplete: aElement.isAutocomplete,
      list: aElement.choices
    }
    this._updateContainer(lastElement, this._currentElement);

    //hide all sidebars, this will adjust the visible rect.
    Browser.hideSidebars();

    //save the element Rect and reuse it to avoid jumps in cases the element moves slighty on the website.
    this._currentElementRect = Rect.fromRect(aElement.rect);
    this._zoom(this._currentElementRect, Rect.fromRect(aElement.caretRect));
  },

  hide: function formHelperHide() {
    if (!this._open)
      return;

    // reset current Element and Caret Rect
    this._currentElementRect = null;
    this._currentCaretRect = null;

    this._updateContainerForSelect(this._currentElement, null);
    this._open = false;
  },

  handleEvent: function formHelperHandleEvent(aEvent) {
    if (aEvent.type == "TabSelect" || aEvent.type == "URLChanged")
      this.hide();
  },

  receiveMessage: function formHelperReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "FormAssist:Show":
        // if the user has manually disabled the Form Assistant UI we still
        // want to show a UI for <select /> element but not managed by
        // FormHelperUI
        let enabled = Services.prefs.getBoolPref("formhelper.enabled");
        enabled ? this.show(json.current, json.hasPrevious, json.hasNext)
                : SelectHelperUI.show(json.current.choices);
        break;

      case "FormAssist:Hide":
        this.hide();
        break;

      case "FormAssist:AutoComplete":
        this._updateAutocompleteFor(json.current);
        this._container.contentHasChanged();
        break;

      case "FormAssist:Resize":
        // First hide all tool/sidebars they take to much space, this will adjust the visible rect.
        Browser.hideSidebars();
        this._zoom(this._currentElementRect, this._currentCaretRect);
        this._container.contentHasChanged();
        break;

       case "FormAssist:Update":
        // Using currentElementRect here is maybe not 100% perfect since
        // elements might change there position while typing
        // out of screen movement is covered by simply following the caret
        // as long as we see what we type, let the element move
        this._zoom(this._currentElementRect, Rect.fromRect(json.caretRect));
        break;

      case "DOMWillOpenModalDialog":
        if (this._open && aMessage.target == Browser.selectedBrowser) {
          this._container.style.display = "none";
          this._container._spacer.hidden = true;
        }
        break;

      case "DOMModalDialogClosed":
        if (this._open && aMessage.target == Browser.selectedBrowser) {
          this._container.style.display = "-moz-box";
          this._container._spacer.hidden = false;
        }
        break;
    }
  },

  observe: function formHelperObserve(aSubject, aTopic, aData) {
    let rect = Rect.fromRect(JSON.parse(aData));
    rect.height = rect.bottom - rect.top;
    rect.width  = rect.right - rect.left;

    this._visibleScreenArea = rect;
    BrowserUI.sizeControls(rect.width, rect.height);
    this._zoom(this._currentElementRect, this._currentCaretRect);
  },

  goToPrevious: function formHelperGoToPrevious() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:Previous", { });
  },

  goToNext: function formHelperGoToNext() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:Next", { });
  },

  doAutoComplete: function formHelperDoAutoComplete(aElement) {
    // Suggestions are only in <label>s. Ignore the rest.
    if (aElement instanceof Ci.nsIDOMXULLabelElement)
      Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:AutoComplete", { value: aElement.value });
  },

  get _open() {
    return (this._container.getAttribute("type") == this.type);
  },

  set _open(aVal) {
    if (aVal == this._open)
      return;

    this._container.hidden = !aVal;
    this._container.contentHasChanged();

    if (aVal) {
      this._zoomStart();
      this._container.show(this);
    } else {
      this._zoomFinish();
      this._currentElement = null;
      this._container.hide(this);
    }

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("FormUI", true, true, window, aVal);
    this._container.dispatchEvent(evt);
  },

  _updateAutocompleteFor: function _formHelperUpdateAutocompleteFor(aElement) {
    let suggestions = this._getAutocompleteSuggestions(aElement);
    this._displaySuggestions(suggestions);
  },

  _displaySuggestions: function _formHelperDisplaySuggestions(aSuggestions) {
    let autofill = this._autofillContainer;
    while (autofill.hasChildNodes())
      autofill.removeChild(autofill.lastChild);

    let fragment = document.createDocumentFragment();
    for (let i = 0; i < aSuggestions.length; i++) {
      let value = aSuggestions[i];
      let button = document.createElement("label");
      button.setAttribute("value", value);
      button.className = "form-helper-autofill-label";
      fragment.appendChild(button);
    }
    autofill.appendChild(fragment);
    autofill.collapsed = !aSuggestions.length;
  },

  /** Retrieve the autocomplete list from the autocomplete service for an element */
  _getAutocompleteSuggestions: function _formHelperGetAutocompleteSuggestions(aElement) {
    if (!aElement.isAutocomplete)
      return [];

    let suggestions = [];

    let autocompleteService = Cc["@mozilla.org/satchel/form-autocomplete;1"].getService(Ci.nsIFormAutoComplete);
    let results = autocompleteService.autoCompleteSearch(aElement.name, aElement.value, aElement, null);
    if (results.matchCount > 0) {
      for (let i = 0; i < results.matchCount; i++) {
        let value = results.getValueAt(i);
        suggestions.push(value);
      }
    }

    return suggestions;
  },

  /** Update the form helper container to reflect new element user is editing. */
  _updateContainer: function _formHelperUpdateContainer(aLastElement, aCurrentElement) {
    this._updateContainerForSelect(aLastElement, aCurrentElement);

    // Setup autofill UI
    this._updateAutocompleteFor(aCurrentElement);
    this._container.contentHasChanged();
  },

  /** Helper for _updateContainer that handles the case where the new element is a select. */
  _updateContainerForSelect: function _formHelperUpdateContainerForSelect(aLastElement, aCurrentElement) {
    let lastHasChoices = aLastElement && (aLastElement.list != null);
    let currentHasChoices = aCurrentElement && (aCurrentElement.list != null);

    if (!lastHasChoices && currentHasChoices) {
      SelectHelperUI.dock(this._container);
      SelectHelperUI.show(aCurrentElement.list);
    } else if (lastHasChoices && currentHasChoices) {
      SelectHelperUI.reset();
      SelectHelperUI.show(aCurrentElement.list);
    } else if (lastHasChoices && !currentHasChoices) {
      SelectHelperUI.hide();
    }
  },

  /** Zoom and move viewport so that element is legible and touchable. */
  _zoom: function _formHelperZoom(aElementRect, aCaretRect) {
    let browser = getBrowser();
    if (aElementRect && aCaretRect && this._open) {
      this._currentCaretRect = aCaretRect;

      let visibleScreenArea = !this._visibleScreenArea.isEmpty() ? this._visibleScreenArea : new Rect(0, 0, window.innerWidth, window.innerHeight);

      // respect the helper container in setting the correct viewAreaHeight
      let viewAreaHeight = visibleScreenArea.height - this._container.getBoundingClientRect().height;
      let viewAreaWidth = visibleScreenArea.width;
      let caretLines = Services.prefs.getIntPref("formhelper.caretLines.portrait");
      let harmonizeValue = Services.prefs.getIntPref("formhelper.harmonizeValue");

      if (!Util.isPortrait())
        caretLines = Services.prefs.getIntPref("formhelper.caretLines.landscape");

      // hide titlebar if the remaining space would be smaller than the height of the titlebar itself
      // if there is enough space left than adjust the height. Since this adjust the the visible rects
      // there is no need to adjust the y later
      let toolbar = document.getElementById("toolbar-main");
      if (viewAreaHeight - toolbar.boxObject.height <= toolbar.boxObject.height * 2)
        Browser.hideTitlebar();
      else
        viewAreaHeight -= toolbar.boxObject.height;

      // To ensure the correct calculation when the sidebars are visible - get the sidebar size and
      // use them as margin
      let [leftvis, rightvis, leftW, rightW] = Browser.computeSidebarVisibility(0, 0);
      let marginLeft = leftvis ? leftW : 0;
      let marginRight = rightvis ? rightW : 0;

      // The height and Y of the caret might change during writing - even in cases the
      // fontsize keeps the same. To avoid unneeded zooming and scrolling
      let harmonizedCaretHeight = 0;
      let harmonizedCaretY = 0;

      // Start calculation here, the order is important
      // All calculations are done in non_zoomed_coordinates => 1:1 to "screen-pixels"

      // for buttons and non input field elements a caretRect with the height of 0 gets reported
      // cover this case.
      if (!aCaretRect.isEmpty()) {
        // the height and y position may vary from letter to letter
        // adjust position and zooming only if a bigger step was done.
        harmonizedCaretHeight = aCaretRect.height - aCaretRect.height % harmonizeValue;
        harmonizedCaretY = aCaretRect.y - aCaretRect.y % harmonizeValue;
      } else {
        harmonizedCaretHeight = 30; // fallback height

        // use the element as position
        harmonizedCaretY = aElementRect.y;
        aCaretRect.x = aElementRect.x;
      }

      let zoomLevel = browser.scale;
      let enableZoom = Browser.selectedTab.allowZoom && Services.prefs.getBoolPref("formhelper.autozoom");
      if (enableZoom) {
        zoomLevel = (viewAreaHeight / caretLines) / harmonizedCaretHeight;
        zoomLevel = Math.min(Math.max(kBrowserFormZoomLevelMin, zoomLevel), kBrowserFormZoomLevelMax);
      }
      viewAreaWidth /= zoomLevel;

      const margin = Services.prefs.getIntPref("formhelper.margin");

      // if the viewAreaWidth is smaller than the neutralized position + margins.
      // [YES] use the x position of the element minus margins as x position for our visible rect.
      // [NO] use the x position of the caret minus margins as the x position for our visible rect.
      let x = (marginLeft + marginRight + margin + aCaretRect.x - aElementRect.x) < viewAreaWidth
               ? aElementRect.x - margin - marginLeft
               : aCaretRect.x - viewAreaWidth + margin + marginRight;
      // Use the adjusted Caret Y minus a margin for our visible rect
      let y = harmonizedCaretY - margin;
      x *= browser.scale;
      y *= browser.scale;

      let scroll = browser.getPosition();

      // from here on play with zoomed values
      // if we want to have it animated, build up zoom rect and animate.
      if (enableZoom && browser.scale != zoomLevel) {
        // don't use browser functions they are bogus for this case
        let zoomRatio = zoomLevel / browser.scale;

        let visW = window.innerWidth, visH = window.innerHeight;
        let newVisW = visW / zoomRatio, newVisH = visH / zoomRatio;
        let zoomRect = new Rect(x, y, newVisW, newVisH);

        Browser.animatedZoomTo(zoomRect);
      }
      else { // no zooming at all
        browser.scrollTo(x, y);
      }
    }
  },

  /* Store the current zoom level, and scroll positions to restore them if needed */
  _zoomStart: function _formHelperZoomStart() {
    if (!Services.prefs.getBoolPref("formhelper.restore"))
      return;

    this._restore = {
      scale: getBrowser().scale,
      contentScrollOffset: Browser.getScrollboxPosition(Browser.contentScrollboxScroller),
      pageScrollOffset: Browser.getScrollboxPosition(Browser.pageScrollboxScroller)
    };
  },

  /** Element is no longer selected. Restore zoom level if setting is enabled. */
  _zoomFinish: function _formHelperZoomFinish() {
    if(!Services.prefs.getBoolPref("formhelper.restore"))
      return;

    let restore = this._restore;
    getBrowser().scale = restore.scale;
    Browser.contentScrollboxScroller.scrollTo(restore.contentScrollOffset.x, restore.contentScrollOffset.y);
    Browser.pageScrollboxScroller.scrollTo(restore.pageScrollOffset.x, restore.pageScrollOffset.y);
  },

  _getOffsetForCaret: function _formHelperGetOffsetForCaret(aCaretRect, aRect) {
    // Determine if we need to move left or right to bring the caret into view
    let deltaX = 0;
    if (aCaretRect.right > aRect.right)
      deltaX = aCaretRect.right - aRect.right;
    if (aCaretRect.left < aRect.left)
      deltaX = aCaretRect.left - aRect.left;

    // Determine if we need to move up or down to bring the caret into view
    let deltaY = 0;
    if (aCaretRect.bottom > aRect.bottom)
      deltaY = aCaretRect.bottom - aRect.bottom;
    if (aCaretRect.top < aRect.top)
      deltaY = aCaretRect.top - aRect.top;

    return [deltaX, deltaY];
  }
};

/**
 * SelectHelperUI: Provides an interface for making a choice in a list.
 *   Supports simultaneous selection of choices and group headers.
 */
var SelectHelperUI = {
  _list: null,
  _selectedIndexes: null,

  get _panel() {
    delete this._panel;
    return this._panel = document.getElementById("select-container");
  },

  get _textbox() {
    delete this._textbox;
    return this._textbox = document.getElementById("select-helper-textbox");
  },

  show: function(aList) {
    this._list = aList;

    this._container = document.getElementById("select-list");
    this._container.setAttribute("multiple", aList.multiple ? "true" : "false");

    this._selectedIndexes = this._getSelectedIndexes();
    let firstSelected = null;

    let choices = aList.choices;
    for (let i = 0; i < choices.length; i++) {
      let choice = choices[i];
      let item = document.createElement("option");
      item.className = "chrome-select-option";
      item.setAttribute("label", choice.text);
      choice.disabled ? item.setAttribute("disabled", choice.disabled)
                      : item.removeAttribute("disabled");
      this._container.appendChild(item);

      if (choice.group) {
        item.classList.add("optgroup");
        continue;
      }

      item.optionIndex = choice.optionIndex;
      item.choiceIndex = i;

      if (choice.inGroup)
        item.classList.add("in-optgroup");

      if (choice.selected) {
        item.setAttribute("selected", "true");
        firstSelected = firstSelected || item;
      }
    }

    this._panel.hidden = false;
    this._panel.height = this._panel.getBoundingClientRect().height;

    if (!this._docked)
      BrowserUI.pushPopup(this, this._panel);

    this._scrollElementIntoView(firstSelected);

    this._container.addEventListener("click", this, false);
  },

  dock: function dock(aContainer) {
    aContainer.insertBefore(this._panel, aContainer.lastChild);
    this._panel.style.maxHeight = (window.innerHeight / 1.8) + "px";
    this._textbox.hidden = false;

    this._docked = true;
  },

  undock: function undock() {
    let rootNode = Elements.stack;
    rootNode.insertBefore(this._panel, rootNode.lastChild);
    this._panel.style.maxHeight = "";
    this._docked = false;
  },

  reset: function() {
    this._updateControl();
    let empty = this._container.cloneNode(false);
    this._container.parentNode.replaceChild(empty, this._container);
    this._container = empty;
    this._list = null;
    this._selectedIndexes = null;
    this._panel.height = "";
    this._textbox.value = "";
  },

  hide: function() {
    this._container.removeEventListener("click", this, false);
    this._panel.hidden = this._textbox.hidden = true;

    if (this._docked)
      this.undock();
    else
      BrowserUI.popPopup();

    this.reset();
  },

  filter: function(aValue) {
    let reg = new RegExp(aValue, "gi");
    let options = this._container.childNodes;
    for (let i = 0; i < options.length; i++) {
      let option = options[i];
      option.getAttribute("label").match(reg) ? option.removeAttribute("filtered")
                                              : option.setAttribute("filtered", "true");
    }
  },

  unselectAll: function() {
    let choices = this._list.choices;
    this._forEachOption(function(aItem, aIndex) {
      aItem.selected = false;
      choices[aIndex].selected = false;
    });
  },

  selectByIndex: function(aIndex) {
    let choices = this._list.choices;
    for (let i = 0; i < this._container.childNodes.length; i++) {
      let option = this._container.childNodes[i];
      if (option.optionIndex == aIndex) {
        option.selected = true;
        this._choices[i].selected = true;
        this._scrollElementIntoView(option);
        break;
      }
    }
  },

  _getSelectedIndexes: function() {
    let indexes = [];
    let choices = this._list.choices;
    let choiceLength = choices.length;
    for (let i = 0; i < choiceLength; i++) {
      let choice = choices[i];
      if (choice.selected)
        indexes.push(choice.optionIndex);
    }
    return indexes;
  },

  _scrollElementIntoView: function(aElement) {
    if (!aElement)
      return;

    let index = -1;
    this._forEachOption(
      function(aItem, aIndex) {
        if (aElement.optionIndex == aItem.optionIndex)
          index = aIndex;
      }
    );

    if (index == -1)
      return;

    let scrollBoxObject = this._container.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    let itemHeight = aElement.getBoundingClientRect().height;
    let visibleItemsCount = this._container.boxObject.height / itemHeight;
    if ((index + 1) > visibleItemsCount) {
      let delta = Math.ceil(visibleItemsCount / 2);
      scrollBoxObject.scrollTo(0, ((index + 1) - delta) * itemHeight);
    }
    else {
      scrollBoxObject.scrollTo(0, 0);
    }
  },

  _forEachOption: function(aCallback) {
    let children = this._container.children;
    for (let i = 0; i < children.length; i++) {
      let item = children[i];
      if (!item.hasOwnProperty("optionIndex"))
        continue;
      aCallback(item, i);
    }
  },

  _updateControl: function() {
    let currentSelectedIndexes = this._getSelectedIndexes();

    let isIdentical = (this._selectedIndexes && this._selectedIndexes.length == currentSelectedIndexes.length);
    if (isIdentical) {
      for (let i = 0; i < currentSelectedIndexes.length; i++) {
        if (currentSelectedIndexes[i] != this._selectedIndexes[i]) {
          isIdentical = false;
          break;
        }
      }
    }

    if (isIdentical)
      return;

    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceChange", { });
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "click":
        let item = aEvent.target;
        if (item && item.hasOwnProperty("optionIndex")) {
          if (this._list.multiple) {
            // Toggle the item state
            item.selected = !item.selected;
          }
          else {
            this.unselectAll();

            // Select the new one and update the control
            item.selected = true;
          }
          this.onSelect(item.optionIndex, item.selected, !this._list.multiple);
        }
        break;
    }
  },

  onSelect: function(aIndex, aSelected, aClearAll) {
    let json = {
      index: aIndex,
      selected: aSelected,
      clearAll: aClearAll
    };
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceSelect", json);
  }
};

var MenuListHelperUI = {
  get _container() {
    delete this._container;
    return this._container = document.getElementById("menulist-container");
  },

  get _popup() {
    delete this._popup;
    return this._popup = document.getElementById("menulist-popup");
  },

  get _title() {
    delete this._title;
    return this._title = document.getElementById("menulist-title");
  },

  _currentList: null,
  show: function mn_show(aMenulist) {
    this._currentList = aMenulist;
    this._title.value = aMenulist.title || "";

    let container = this._container;
    let listbox = this._popup.lastChild;
    while (listbox.firstChild)
      listbox.removeChild(listbox.firstChild);

    let children = this._currentList.menupopup.children;
    for (let i = 0; i < children.length; i++) {
      let child = children[i];
      let item = document.createElement("richlistitem");
      // Add selected as a class name instead of an attribute to not being overidden
      // by the richlistbox behavior (it sets the "current" and "selected" attribute
      item.setAttribute("class", "menulist-command" + (child.selected ? " selected" : ""));

      let image = document.createElement("image");
      image.setAttribute("src", child.image || "");
      item.appendChild(image);

      let label = document.createElement("label");
      label.setAttribute("value", child.label);
      item.appendChild(label);

      listbox.appendChild(item);
    }

    window.addEventListener("resize", this, true);
    container.hidden = false;
    this.sizeToContent();
    BrowserUI.pushPopup(this, [this._popup]);
  },

  hide: function mn_hide() {
    this._currentList = null;
    this._container.hidden = true;
    window.removeEventListener("resize", this, true);
    BrowserUI.popPopup();
  },

  selectByIndex: function mn_selectByIndex(aIndex) {
    this._currentList.selectedIndex = aIndex;

    // Dispatch a xul command event to the attached menulist
    if (this._currentList.dispatchEvent) {
      let evt = document.createEvent("XULCommandEvent");
      evt.initCommandEvent("command", true, true, window, 0, false, false, false, false, null);
      this._currentList.dispatchEvent(evt);
    }

    this.hide();
  },

  sizeToContent: function sizeToContent() {
    this._popup.maxWidth = window.innerWidth * 0.75;
  },

  handleEvent: function handleEvent(aEvent) {
    this.sizeToContent();
  }
}

var ContextHelper = {
  popupState: null,

  get _panel() {
    delete this._panel;
    return this._panel = document.getElementById("context-container");
  },

  get _popup() {
    delete this._popup;
    return this._popup = document.getElementById("context-popup");
  },

  showPopup: function ch_showPopup(aMessage) {
    this.popupState = aMessage.json;
    this.popupState.target = aMessage.target;

    let first = null;
    let last = null;
    let commands = document.getElementById("context-commands");
    for (let i=0; i<commands.childElementCount; i++) {
      let command = commands.children[i];
      command.removeAttribute("selector");
      command.hidden = true;

      let types = command.getAttribute("type").split(/\s+/);
      for (let i=0; i<types.length; i++) {
        if (this.popupState.types.indexOf(types[i]) != -1) {
          first = first || command;
          last = command;
          command.hidden = false;
          break;
        }
      }
    }

    if (!first) {
      this.popupState = null;
      return false;
    }

    // Allow the first and last *non-hidden* elements to be selected in CSS.
    first.setAttribute("selector", "first-child");
    last.setAttribute("selector", "last-child");

    let label = document.getElementById("context-hint");
    label.value = this.popupState.label;

    this._panel.hidden = false;
    window.addEventListener("resize", this, true);

    this.sizeToContent();
    BrowserUI.pushPopup(this, [this._popup]);
    return true;
  },

  hide: function ch_hide() {
    this.popupState = null;
    this._panel.hidden = true;
    window.removeEventListener("resize", this, true);

    BrowserUI.popPopup();
  },

  sizeToContent: function sizeToContent() {
    this._popup.maxWidth = window.innerWidth * 0.75;
  },

  handleEvent: function handleEvent(aEvent) {
    this.sizeToContent();
  }
};

var ContextCommands = {
  openInNewTab: function cc_openInNewTab() {
    Browser.addTab(ContextHelper.popupState.linkURL, false, Browser.selectedTab);
  },

  saveLink: function cc_saveLink() {
    let browser = ContextHelper.popupState.target;
    saveURL(ContextHelper.popupState.linkURL, null, "SaveLinkTitle", false, true, browser.documentURI);
  },

  saveImage: function cc_saveImage() {
    let browser = ContextHelper.popupState.target;
    saveImageURL(ContextHelper.popupState.mediaURL, null, "SaveImageTitle", false, true, browser.documentURI);
  },

  shareLink: function cc_shareLink() {
    let state = ContextHelper.popupState;
    SharingUI.show(state.linkURL, state.linkTitle);
  },

  shareMedia: function cc_shareMedia() {
    SharingUI.show(ContextHelper.popupState.mediaURL, null);
  },

  editBookmark: function cc_editBookmark() {
    let target = ContextHelper.popupState.target;
    target.startEditing();
  },

  removeBookmark: function cc_removeBookmark() {
    let target = ContextHelper.popupState.target;
    target.remove();
  }
}

var SharingUI = {
  _dialog: null,

  show: function show(aURL, aTitle) {
    this._dialog = importDialog(window, "chrome://browser/content/share.xul", null);
    document.getElementById("share-title").value = aTitle || aURL;

    BrowserUI.pushPopup(this, this._dialog);

    let bbox = document.getElementById("share-buttons-box");
    this._handlers.forEach(function(handler) {
      let button = document.createElement("button");
      button.className = "prompt-button";
      button.setAttribute("label", handler.name);
      button.addEventListener("command", function() {
        SharingUI.hide();
        handler.callback(aURL || "", aTitle || "");
      }, false);
      bbox.appendChild(button);
    });
    this._dialog.waitForClose();
    BrowserUI.popPopup();
  },

  hide: function hide() {
    this._dialog.close();
    this._dialog = null;
  },

  _handlers: [
    {
      name: "Email",
      callback: function callback(aURL, aTitle) {
        let url = "mailto:?subject=" + encodeURIComponent(aTitle) +
                  "&body=" + encodeURIComponent(aURL);
        let uri = Services.io.newURI(url, null, null);
        let extProtocolSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
                             .getService(Ci.nsIExternalProtocolService);
        extProtocolSvc.loadUrl(uri);
      }
    },
    {
      name: "Twitter",
      callback: function callback(aURL, aTitle) {
        let url = "http://twitter.com/home?status=" + encodeURIComponent((aTitle ? aTitle+": " : "")+aURL);
        Browser.addTab(url, true, Browser.selectedTab);
      }
    },
    {
      name: "Google Reader",
      callback: function callback(aURL, aTitle) {
        let url = "http://www.google.com/reader/link?url=" + encodeURIComponent(aURL) +
                  "&title=" + encodeURIComponent(aTitle);
        Browser.addTab(url, true, Browser.selectedTab);
      }
    },
    {
      name: "Facebook",
      callback: function callback(aURL, aTitle) {
        let url = "http://www.facebook.com/share.php?u=" + encodeURIComponent(aURL);
        Browser.addTab(url, true, Browser.selectedTab);
      }
    }
  ]
};


var BadgeHandlers = {
  _handlers: [
    {
      _lastUpdate: 0,
      _lastCount: 0,
      url: "http://mail.google.com",
      updateBadge: function(aBadge) {
        // Use the cache if possible
        let now = Date.now();
        if (this._lastCount && this._lastUpdate > now - 1000) {
          aBadge.set(this._lastCount);
          return;
        }

        this._lastUpdate = now;

        // Use any saved username and password. If we don't have any login and we are not
        // currently logged into Gmail, we won't get any count.
        let login = BadgeHandlers.getLogin("https://www.google.com");

        // Get the feed and read the count, passing any saved username and password
        // but do not show any security dialogs if we fail
        let req = new XMLHttpRequest();
        req.mozBackgroundRequest = true;
        req.open("GET", "https://mail.google.com/mail/feed/atom", true, login.username, login.password);
        req.onreadystatechange = function(aEvent) {
          if (req.readyState == 4) {
            if (req.status == 200) {
              let count = req.responseXML.getElementsByTagName("fullcount");
              this._lastCount = count ? count[0].childNodes[0].nodeValue : 0;
            } else {
              this._lastCount = 0;
            }
            this._lastCount = BadgeHandlers.setNumberBadge(aBadge, this._lastCount);
          }
        };
        req.send(null);
      }
    }
  ],

  register: function(aPopup) {
    let handlers = this._handlers;
    for (let i = 0; i < handlers.length; i++)
      aPopup.registerBadgeHandler(handlers[i].url, handlers[i]);
  },

  getLogin: function(aURL) {
    let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
    let logins = lm.findLogins({}, aURL, aURL, null);
    let username = logins.length > 0 ? logins[0].username : "";
    let password = logins.length > 0 ? logins[0].password : "";
    return { username: username, password: password };
  },

  clampBadge: function(aValue) {
    if (aValue > 100)
      aValue = "99+";
    return aValue;
  },

  setNumberBadge: function(aBadge, aValue) {
    if (parseInt(aValue) != 0) {
      aValue = this.clampBadge(aValue);
      aBadge.set(aValue);
    } else {
      aBadge.set("");
    }
    return aValue;
  }
};
