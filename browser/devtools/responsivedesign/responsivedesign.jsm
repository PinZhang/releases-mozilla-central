/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/FloatingScrollbars.jsm");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");

this.EXPORTED_SYMBOLS = ["ResponsiveUIManager"];

const MIN_WIDTH = 50;
const MIN_HEIGHT = 50;

const MAX_WIDTH = 10000;
const MAX_HEIGHT = 10000;

this.ResponsiveUIManager = {
  /**
   * Check if the a tab is in a responsive mode.
   * Leave the responsive mode if active,
   * active the responsive mode if not active.
   *
   * @param aWindow the main window.
   * @param aTab the tab targeted.
   */
  toggle: function(aWindow, aTab) {
    if (aTab.__responsiveUI) {
      aTab.__responsiveUI.close();
    } else {
      aTab.__responsiveUI = new ResponsiveUI(aWindow, aTab);
    }
  },

  /**
   * Handle gcli commands.
   *
   * @param aWindow the browser window.
   * @param aTab the tab targeted.
   * @param aCommand the command name.
   * @param aArgs command arguments.
   */
  handleGcliCommand: function(aWindow, aTab, aCommand, aArgs) {
    switch (aCommand) {
      case "resize to":
        if (!aTab.__responsiveUI) {
          aTab.__responsiveUI = new ResponsiveUI(aWindow, aTab);
        }
        aTab.__responsiveUI.setSize(aArgs.width, aArgs.height);
        break;
      case "resize on":
        if (!aTab.__responsiveUI) {
          aTab.__responsiveUI = new ResponsiveUI(aWindow, aTab);
        }
        break;
      case "resize off":
        if (aTab.__responsiveUI) {
          aTab.__responsiveUI.close();
        }
        break;
      case "resize toggle":
          this.toggle(aWindow, aTab);
      default:
    }
  },

  get events() {
    if (!this._eventEmitter) {
      this._eventEmitter = new EventEmitter();
    }
    return this._eventEmitter;
  },
}

let presets = [
  // Phones
  {key: "320x480", width: 320, height: 480},    // iPhone, B2G, with <meta viewport>
  {key: "360x640", width: 360, height: 640},    // Android 4, phones, with <meta viewport>

  // Tablets
  {key: "768x1024", width: 768, height: 1024},   // iPad, with <meta viewport>
  {key: "800x1280", width: 800, height: 1280},   // Android 4, Tablet, with <meta viewport>

  // Default width for mobile browsers, no <meta viewport>
  {key: "980x1280", width: 980, height: 1280},

  // Computer
  {key: "1280x600", width: 1280, height: 600},
  {key: "1920x900", width: 1920, height: 900},
];

function ResponsiveUI(aWindow, aTab)
{
  this.mainWindow = aWindow;
  this.tab = aTab;
  this.tabContainer = aWindow.gBrowser.tabContainer;
  this.browser = aTab.linkedBrowser;
  this.chromeDoc = aWindow.document;
  this.container = aWindow.gBrowser.getBrowserContainer(this.browser);
  this.stack = this.container.querySelector(".browserStack");

  // Try to load presets from prefs
  if (Services.prefs.prefHasUserValue("devtools.responsiveUI.presets")) {
    try {
      presets = JSON.parse(Services.prefs.getCharPref("devtools.responsiveUI.presets"));
    } catch(e) {
      // User pref is malformated.
      Cu.reportError("Could not parse pref `devtools.responsiveUI.presets`: " + e);
    }
  }

  this.customPreset = {key: "custom", custom: true};

  if (Array.isArray(presets)) {
    this.presets = [this.customPreset].concat(presets);
  } else {
    Cu.reportError("Presets value (devtools.responsiveUI.presets) is malformated.");
    this.presets = [this.customPreset];
  }

  try {
    let width = Services.prefs.getIntPref("devtools.responsiveUI.customWidth");
    let height = Services.prefs.getIntPref("devtools.responsiveUI.customHeight");
    this.customPreset.width = Math.min(MAX_WIDTH, width);
    this.customPreset.height = Math.min(MAX_HEIGHT, height);

    this.currentPresetKey = Services.prefs.getCharPref("devtools.responsiveUI.currentPreset");
  } catch(e) {
    // Default size. The first preset (custom) is the one that will be used.
    let bbox = this.stack.getBoundingClientRect();

    this.customPreset.width = bbox.width - 40; // horizontal padding of the container
    this.customPreset.height = bbox.height - 80; // vertical padding + toolbar height

    this.currentPresetKey = this.customPreset.key;
  }

  this.container.setAttribute("responsivemode", "true");
  this.stack.setAttribute("responsivemode", "true");

  // Let's bind some callbacks.
  this.bound_presetSelected = this.presetSelected.bind(this);
  this.bound_addPreset = this.addPreset.bind(this);
  this.bound_removePreset = this.removePreset.bind(this);
  this.bound_rotate = this.rotate.bind(this);
  this.bound_startResizing = this.startResizing.bind(this);
  this.bound_stopResizing = this.stopResizing.bind(this);
  this.bound_onDrag = this.onDrag.bind(this);
  this.bound_onKeypress = this.onKeypress.bind(this);

  // Events
  this.tab.addEventListener("TabClose", this);
  this.tabContainer.addEventListener("TabSelect", this);
  this.mainWindow.document.addEventListener("keypress", this.bound_onKeypress, false);

  this.buildUI();
  this.checkMenus();

  this.inspectorWasOpen = this.mainWindow.InspectorUI.isInspectorOpen;

  try {
    if (Services.prefs.getBoolPref("devtools.responsiveUI.rotate")) {
      this.rotate();
    }
  } catch(e) {}

  if (this._floatingScrollbars)
    switchToFloatingScrollbars(this.tab);

  ResponsiveUIManager.events.emit("on", this.tab, this);
}

ResponsiveUI.prototype = {
  _transitionsEnabled: true,
  _floatingScrollbars: false, // See bug 799471
  get transitionsEnabled() this._transitionsEnabled,
  set transitionsEnabled(aValue) {
    this._transitionsEnabled = aValue;
    if (aValue && !this._resizing && this.stack.hasAttribute("responsivemode")) {
      this.stack.removeAttribute("notransition");
    } else if (!aValue) {
      this.stack.setAttribute("notransition", "true");
    }
  },

  /**
   * Destroy the nodes. Remove listeners. Reset the style.
   */
  close: function RUI_unload() {
    if (this.closing)
      return;
    this.closing = true;

    if (this._floatingScrollbars)
      switchToNativeScrollbars(this.tab);

    this.unCheckMenus();
    // Reset style of the stack.
    let style = "max-width: none;" +
                "min-width: 0;" +
                "max-height: none;" +
                "min-height: 0;";
    this.stack.setAttribute("style", style);

    if (this.isResizing)
      this.stopResizing();

    // Remove listeners.
    this.mainWindow.document.removeEventListener("keypress", this.bound_onKeypress, false);
    this.menulist.removeEventListener("select", this.bound_presetSelected, true);
    this.tab.removeEventListener("TabClose", this);
    this.tabContainer.removeEventListener("TabSelect", this);
    this.rotatebutton.removeEventListener("command", this.bound_rotate, true);
    this.addbutton.removeEventListener("command", this.bound_addPreset, true);
    this.removebutton.removeEventListener("command", this.bound_removePreset, true);

    // Removed elements.
    this.container.removeChild(this.toolbar);
    this.stack.removeChild(this.resizer);
    this.stack.removeChild(this.resizeBar);

    // Unset the responsive mode.
    this.container.removeAttribute("responsivemode");
    this.stack.removeAttribute("responsivemode");

    delete this.tab.__responsiveUI;
    ResponsiveUIManager.events.emit("off", this.tab, this);
  },

  /**
   * Handle keypressed.
   *
   * @param aEvent
   */
  onKeypress: function RUI_onKeypress(aEvent) {
    if (aEvent.keyCode == this.mainWindow.KeyEvent.DOM_VK_ESCAPE &&
        this.mainWindow.gBrowser.selectedBrowser == this.browser) {

      // If the inspector wasn't open at first but is open now,
      // we don't want to close the Responsive Mode on Escape.
      // We let the inspector close first.

      let isInspectorOpen = this.mainWindow.InspectorUI.isInspectorOpen;
      if (this.inspectorWasOpen || !isInspectorOpen) {
        aEvent.preventDefault();
        aEvent.stopPropagation();
        this.close();
      }
    }
  },

  /**
   * Handle events
   */
  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "TabClose":
        this.close();
        break;
      case "TabSelect":
        if (this.tab.selected) {
          this.checkMenus();
        } else if (!this.mainWindow.gBrowser.selectedTab.responsiveUI) {
          this.unCheckMenus();
        }
        break;
    }
  },

  /**
   * Check the menu items.
   */
   checkMenus: function RUI_checkMenus() {
     this.chromeDoc.getElementById("Tools:ResponsiveUI").setAttribute("checked", "true");
   },

  /**
   * Uncheck the menu items.
   */
   unCheckMenus: function RUI_unCheckMenus() {
     this.chromeDoc.getElementById("Tools:ResponsiveUI").setAttribute("checked", "false");
   },

  /**
   * Build the toolbar and the resizers.
   *
   * <vbox class="browserContainer"> From tabbrowser.xml
   *  <toolbar class="devtools-toolbar devtools-responsiveui-toolbar">
   *    <menulist class="devtools-menulist"/> // presets
   *    <toolbarbutton tabindex="0" class="devtools-toolbarbutton" label="rotate"/> // rotate
   *  </toolbar>
   *  <stack class="browserStack"> From tabbrowser.xml
   *    <browser/>
   *    <box class="devtools-responsiveui-resizehandle" bottom="0" right="0"/>
   *    <box class="devtools-responsiveui-resizebar" top="0" right="0"/>
   *  </stack>
   * </vbox>
   */
  buildUI: function RUI_buildUI() {
    // Toolbar
    this.toolbar = this.chromeDoc.createElement("toolbar");
    this.toolbar.className = "devtools-toolbar devtools-responsiveui-toolbar";

    this.menulist = this.chromeDoc.createElement("menulist");
    this.menulist.className = "devtools-menulist";

    this.menulist.addEventListener("select", this.bound_presetSelected, true);

    this.menuitems = new Map();

    let menupopup = this.chromeDoc.createElement("menupopup");
    this.registerPresets(menupopup);
    this.menulist.appendChild(menupopup);

    this.addbutton = this.chromeDoc.createElement("menuitem");
    this.addbutton.setAttribute("label", this.strings.GetStringFromName("responsiveUI.addPreset"));
    this.addbutton.addEventListener("command", this.bound_addPreset, true);

    this.removebutton = this.chromeDoc.createElement("menuitem");
    this.removebutton.setAttribute("label", this.strings.GetStringFromName("responsiveUI.removePreset"));
    this.removebutton.addEventListener("command", this.bound_removePreset, true);

    menupopup.appendChild(this.chromeDoc.createElement("menuseparator"));
    menupopup.appendChild(this.addbutton);
    menupopup.appendChild(this.removebutton);

    this.rotatebutton = this.chromeDoc.createElement("toolbarbutton");
    this.rotatebutton.setAttribute("tabindex", "0");
    this.rotatebutton.setAttribute("label", this.strings.GetStringFromName("responsiveUI.rotate"));
    this.rotatebutton.className = "devtools-toolbarbutton";
    this.rotatebutton.addEventListener("command", this.bound_rotate, true);

    this.toolbar.appendChild(this.menulist);
    this.toolbar.appendChild(this.rotatebutton);

    // Resizers
    this.resizer = this.chromeDoc.createElement("box");
    this.resizer.className = "devtools-responsiveui-resizehandle";
    this.resizer.setAttribute("right", "0");
    this.resizer.setAttribute("bottom", "0");
    this.resizer.onmousedown = this.bound_startResizing;

    this.resizeBar =  this.chromeDoc.createElement("box");
    this.resizeBar.className = "devtools-responsiveui-resizebar";
    this.resizeBar.setAttribute("top", "0");
    this.resizeBar.setAttribute("right", "0");
    this.resizeBar.onmousedown = this.bound_startResizing;

    this.container.insertBefore(this.toolbar, this.stack);
    this.stack.appendChild(this.resizer);
    this.stack.appendChild(this.resizeBar);
  },

  /**
   * Build the presets list and append it to the menupopup.
   *
   * @param aParent menupopup.
   */
  registerPresets: function RUI_registerPresets(aParent) {
    let fragment = this.chromeDoc.createDocumentFragment();
    let doc = this.chromeDoc;

    for (let preset of this.presets) {
      let menuitem = doc.createElement("menuitem");
      menuitem.setAttribute("ispreset", true);
      this.menuitems.set(menuitem, preset);

      if (preset.key === this.currentPresetKey) {
        menuitem.setAttribute("selected", "true");
        this.selectedItem = menuitem;
      }

      if (preset.custom)
        this.customMenuitem = menuitem;

      this.setMenuLabel(menuitem, preset);
      fragment.appendChild(menuitem);
    }
    aParent.appendChild(fragment);
  },

  /**
   * Set the menuitem label of a preset.
   *
   * @param aMenuitem menuitem to edit.
   * @param aPreset associated preset.
   */
  setMenuLabel: function RUI_setMenuLabel(aMenuitem, aPreset) {
    let size = Math.round(aPreset.width) + "x" + Math.round(aPreset.height);
    if (aPreset.custom) {
      let str = this.strings.formatStringFromName("responsiveUI.customResolution", [size], 1);
      aMenuitem.setAttribute("label", str);
    } else if (aPreset.name != null && aPreset.name !== "") {
      let str = this.strings.formatStringFromName("responsiveUI.namedResolution", [size, aPreset.name], 2);
      aMenuitem.setAttribute("label", str);
    } else {
      aMenuitem.setAttribute("label", size);
    }
  },

  /**
   * When a preset is selected, apply it.
   */
  presetSelected: function RUI_presetSelected() {
    if (this.menulist.selectedItem.getAttribute("ispreset") === "true") {
      this.selectedItem = this.menulist.selectedItem;

      this.rotateValue = false;
      let selectedPreset = this.menuitems.get(this.selectedItem);
      this.loadPreset(selectedPreset);
      this.currentPresetKey = selectedPreset.key;
      this.saveCurrentPreset();

      // Update the buttons hidden status according to the new selected preset
      if (selectedPreset == this.customPreset) {
        this.addbutton.hidden = false;
        this.removebutton.hidden = true;
      } else {
        this.addbutton.hidden = true;
        this.removebutton.hidden = false;
      }
    }
  },

  /**
   * Apply a preset.
   *
   * @param aPreset preset to apply.
   */
  loadPreset: function RUI_loadPreset(aPreset) {
    this.setSize(aPreset.width, aPreset.height);
  },

  /**
   * Add a preset to the list and the memory
   */
  addPreset: function RUI_addPreset() {
    let w = this.customPreset.width;
    let h = this.customPreset.height;
    let newName = {};

    let title = this.strings.GetStringFromName("responsiveUI.customNamePromptTitle");
    let message = this.strings.formatStringFromName("responsiveUI.customNamePromptMsg", [w, h], 2);
    let promptOk = Services.prompt.prompt(null, title, message, newName, null, {});

    if (!promptOk) {
      // Prompt has been cancelled
      let menuitem = this.customMenuitem;
      this.menulist.selectedItem = menuitem;
      this.currentPresetKey = this.customPreset.key;
      return;
    }

    let newPreset = {
      key: w + "x" + h,
      name: newName.value,
      width: w,
      height: h
    };

    this.presets.push(newPreset);

    // Sort the presets according to width/height ascending order
    this.presets.sort(function RUI_sortPresets(aPresetA, aPresetB) {
      // We keep custom preset at first
      if (aPresetA.custom && !aPresetB.custom) {
        return 1;
      }
      if (!aPresetA.custom && aPresetB.custom) {
        return -1;
      }

      if (aPresetA.width === aPresetB.width) {
        if (aPresetA.height === aPresetB.height) {
          return 0;
        } else {
          return aPresetA.height > aPresetB.height;
        }
      } else {
        return aPresetA.width > aPresetB.width;
      }
    });

    this.savePresets();

    let newMenuitem = this.chromeDoc.createElement("menuitem");
    newMenuitem.setAttribute("ispreset", true);
    this.setMenuLabel(newMenuitem, newPreset);

    this.menuitems.set(newMenuitem, newPreset);
    let idx = this.presets.indexOf(newPreset);
    let beforeMenuitem = this.menulist.firstChild.childNodes[idx + 1];
    this.menulist.firstChild.insertBefore(newMenuitem, beforeMenuitem);

    this.menulist.selectedItem = newMenuitem;
    this.currentPresetKey = newPreset.key;
    this.saveCurrentPreset();
  },

  /**
   * remove a preset from the list and the memory
   */
  removePreset: function RUI_removePreset() {
    let selectedPreset = this.menuitems.get(this.selectedItem);
    let w = selectedPreset.width;
    let h = selectedPreset.height;

    this.presets.splice(this.presets.indexOf(selectedPreset), 1);
    this.menulist.firstChild.removeChild(this.selectedItem);
    this.menuitems.delete(this.selectedItem);

    this.customPreset.width = w;
    this.customPreset.height = h;
    let menuitem = this.customMenuitem;
    this.setMenuLabel(menuitem, this.customPreset);
    this.menulist.selectedItem = menuitem;
    this.currentPresetKey = this.customPreset.key;

    this.setSize(w, h);

    this.savePresets();
  },

  /**
   * Swap width and height.
   */
  rotate: function RUI_rotate() {
    let selectedPreset = this.menuitems.get(this.selectedItem);
    let width = this.rotateValue ? selectedPreset.height : selectedPreset.width;
    let height = this.rotateValue ? selectedPreset.width : selectedPreset.height;

    this.setSize(height, width);

    if (selectedPreset.custom) {
      this.saveCustomSize();
    } else {
      this.rotateValue = !this.rotateValue;
      this.saveCurrentPreset();
    }
  },

  /**
   * Change the size of the browser.
   *
   * @param aWidth width of the browser.
   * @param aHeight height of the browser.
   */
  setSize: function RUI_setSize(aWidth, aHeight) {
    aWidth = Math.min(Math.max(aWidth, MIN_WIDTH), MAX_WIDTH);
    aHeight = Math.min(Math.max(aHeight, MIN_HEIGHT), MAX_WIDTH);

    // We resize the containing stack.
    let style = "max-width: %width;" +
                "min-width: %width;" +
                "max-height: %height;" +
                "min-height: %height;";

    style = style.replace(/%width/g, aWidth + "px");
    style = style.replace(/%height/g, aHeight + "px");

    this.stack.setAttribute("style", style);

    if (!this.ignoreY)
      this.resizeBar.setAttribute("top", Math.round(aHeight / 2));

    let selectedPreset = this.menuitems.get(this.selectedItem);

    // We uptate the custom menuitem if we are using it
    if (selectedPreset.custom) {
      selectedPreset.width = aWidth;
      selectedPreset.height = aHeight;

      this.setMenuLabel(this.selectedItem, selectedPreset);
    }
  },

  /**
   * Start the process of resizing the browser.
   *
   * @param aEvent
   */
  startResizing: function RUI_startResizing(aEvent) {
    let selectedPreset = this.menuitems.get(this.selectedItem);

    if (!selectedPreset.custom) {
      this.customPreset.width = this.rotateValue ? selectedPreset.height : selectedPreset.width;
      this.customPreset.height = this.rotateValue ? selectedPreset.width : selectedPreset.height;

      let menuitem = this.customMenuitem;
      this.setMenuLabel(menuitem, this.customPreset);

      this.currentPresetKey = this.customPreset.key;
      this.menulist.selectedItem = menuitem;
    }
    this.mainWindow.addEventListener("mouseup", this.bound_stopResizing, true);
    this.mainWindow.addEventListener("mousemove", this.bound_onDrag, true);
    this.container.style.pointerEvents = "none";

    this._resizing = true;
    this.stack.setAttribute("notransition", "true");

    this.lastClientX = aEvent.clientX;
    this.lastClientY = aEvent.clientY;

    this.ignoreY = (aEvent.target === this.resizeBar);

    this.isResizing = true;
  },

  /**
   * Resizing on mouse move.
   *
   * @param aEvent
   */
  onDrag: function RUI_onDrag(aEvent) {
    let deltaX = aEvent.clientX - this.lastClientX;
    let deltaY = aEvent.clientY - this.lastClientY;

    if (this.ignoreY)
      deltaY = 0;

    let width = this.customPreset.width + deltaX;
    let height = this.customPreset.height + deltaY;

    if (width < MIN_WIDTH) {
        width = MIN_WIDTH;
    } else {
        this.lastClientX = aEvent.clientX;
    }

    if (height < MIN_HEIGHT) {
        height = MIN_HEIGHT;
    } else {
        this.lastClientY = aEvent.clientY;
    }

    this.setSize(width, height);
  },

  /**
   * Stop End resizing
   */
  stopResizing: function RUI_stopResizing() {
    this.container.style.pointerEvents = "auto";

    this.mainWindow.removeEventListener("mouseup", this.bound_stopResizing, true);
    this.mainWindow.removeEventListener("mousemove", this.bound_onDrag, true);

    this.saveCustomSize();

    delete this._resizing;
    if (this.transitionsEnabled) {
      this.stack.removeAttribute("notransition");
    }
    this.ignoreY = false;
    this.isResizing = false;
  },

  /**
   * Store the custom size as a pref.
   */
   saveCustomSize: function RUI_saveCustomSize() {
     Services.prefs.setIntPref("devtools.responsiveUI.customWidth", this.customPreset.width);
     Services.prefs.setIntPref("devtools.responsiveUI.customHeight", this.customPreset.height);
   },

  /**
   * Store the current preset as a pref.
   */
   saveCurrentPreset: function RUI_saveCurrentPreset() {
     Services.prefs.setCharPref("devtools.responsiveUI.currentPreset", this.currentPresetKey);
     Services.prefs.setBoolPref("devtools.responsiveUI.rotate", this.rotateValue);
   },

  /**
   * Store the list of all registered presets as a pref.
   */
  savePresets: function RUI_savePresets() {
    // We exclude the custom one
    let registeredPresets = this.presets.filter(function (aPreset) {
      return !aPreset.custom;
    });

    Services.prefs.setCharPref("devtools.responsiveUI.presets", JSON.stringify(registeredPresets));
  },
}

XPCOMUtils.defineLazyGetter(ResponsiveUI.prototype, "strings", function () {
  return Services.strings.createBundle("chrome://browser/locale/devtools/responsiveUI.properties");
});
