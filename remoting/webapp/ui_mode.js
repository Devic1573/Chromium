// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Functions related to controlling the modal UI state of the app. UI states
 * are expressed as HTML attributes with a dotted hierarchy. For example, the
 * string 'host.shared' will match any elements with an associated attribute
 * of 'host' or 'host.shared', showing those elements and hiding all others.
 * Elements with no associated attribute are ignored.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @enum {string} */
// TODO(jamiewalch): Move 'in-session' to a separate web-page so that the
// 'home' state applies to all elements and can be removed.
remoting.AppMode = {
  HOME: 'home',
    UNAUTHENTICATED: 'home.auth',
    HOST: 'home.host',
      HOST_WAITING_FOR_CODE: 'home.host.waiting-for-code',
      HOST_WAITING_FOR_CONNECTION: 'home.host.waiting-for-connection',
      HOST_SHARED: 'home.host.shared',
      HOST_SHARE_FAILED: 'home.host.share-failed',
      HOST_SHARE_FINISHED: 'home.host.share-finished',
    CLIENT: 'home.client',
      CLIENT_UNCONNECTED: 'home.client.unconnected',
      CLIENT_PIN_PROMPT: 'home.client.pin-prompt',
      CLIENT_CONNECTING: 'home.client.connecting',
      CLIENT_CONNECT_FAILED_IT2ME: 'home.client.connect-failed.it2me',
      CLIENT_CONNECT_FAILED_ME2ME: 'home.client.connect-failed.me2me',
      CLIENT_SESSION_FINISHED_IT2ME: 'home.client.session-finished.it2me',
      CLIENT_SESSION_FINISHED_ME2ME: 'home.client.session-finished.me2me',
    HISTORY: 'home.history',
    CONFIRM_HOST_DELETE: 'home.confirm-host-delete',
    HOST_SETUP: 'home.host-setup',
      HOST_SETUP_INSTALL: 'home.host-setup.install',
      HOST_SETUP_INSTALL_PENDING: 'home.host-setup.install-pending',
      HOST_SETUP_ASK_PIN: 'home.host-setup.ask-pin',
      HOST_SETUP_PROCESSING: 'home.host-setup.processing',
      HOST_SETUP_DONE: 'home.host-setup.done',
      HOST_SETUP_ERROR: 'home.host-setup.error',
  IN_SESSION: 'in-session'
};

/**
 * @param {Element} element The element to check.
 * @param {string} attrName The attribute on the element to check.
 * @param {Array.<string>} modes The modes to check for.
 * @return {boolean} True if any mode in |modes| is found within the attribute.
 */
remoting.hasModeAttribute = function(element, attrName, modes) {
  var attr = element.getAttribute(attrName);
  for (var i = 0; i < modes.length; ++i) {
    if (attr.match(new RegExp('(\\s|^)' + modes[i] + '(\\s|$)')) != null) {
      return true;
    }
  }
  return false;
};

/**
 * Update the DOM by showing or hiding elements based on whether or not they
 * have an attribute matching the specified name.
 * @param {string} mode The value against which to match the attribute.
 * @param {string} attr The attribute name to match.
 * @return {void} Nothing.
 */
remoting.updateModalUi = function(mode, attr) {
  var modes = mode.split('.');
  for (var i = 1; i < modes.length; ++i)
    modes[i] = modes[i - 1] + '.' + modes[i];
  var elements = document.querySelectorAll('[' + attr + ']');
  // Hide elements first so that we don't end up trying to show two modal
  // dialogs at once (which would break keyboard-navigation confinement).
  for (var i = 0; i < elements.length; ++i) {
    var element = /** @type {Element} */ elements[i];
    if (!remoting.hasModeAttribute(element, attr, modes)) {
      element.hidden = true;
    }
  }
  for (var i = 0; i < elements.length; ++i) {
    var element = /** @type {Element} */ elements[i];
    if (remoting.hasModeAttribute(element, attr, modes)) {
      element.hidden = false;
      var autofocusNode = element.querySelector('[autofocus]');
      if (autofocusNode) {
        autofocusNode.focus();
      }
    }
  }
};

/**
 * @type {remoting.AppMode} The current app mode
 */
remoting.currentMode = remoting.AppMode.HOME;

/**
 * Change the app's modal state to |mode|, determined by the data-ui-mode
 * attribute.
 *
 * @param {remoting.AppMode} mode The new modal state.
 */
remoting.setMode = function(mode) {
  remoting.updateModalUi(mode, 'data-ui-mode');
  console.log('App mode: ' + mode);
  remoting.currentMode = mode;
  if (mode == remoting.AppMode.IN_SESSION) {
    document.removeEventListener('keydown', remoting.ConnectionStats.onKeydown,
                                 false);
    document.addEventListener('webkitvisibilitychange',
                              remoting.onVisibilityChanged, false);
  } else {
    document.addEventListener('keydown', remoting.ConnectionStats.onKeydown,
                              false);
    document.removeEventListener('webkitvisibilitychange',
                                 remoting.onVisibilityChanged, false);
  }
};

/**
 * Get the major mode that the app is running in.
 * @return {string} The app's current major mode.
 */
remoting.getMajorMode = function() {
  return remoting.currentMode.split('.')[0];
};

remoting.showOrHideIt2MeUi = function() {
  var visited = !!window.localStorage.getItem('it2me-visited');
  document.getElementById('it2me-first-run').hidden = visited;
  document.getElementById('it2me-content').hidden = !visited;
};

remoting.showOrHideMe2MeUi = function() {
  var visited = !!window.localStorage.getItem('me2me-visited');
  document.getElementById('me2me-first-run').hidden = visited;
  document.getElementById('me2me-content').hidden = !visited;
};

remoting.showIt2MeUiAndSave = function() {
  window.localStorage.setItem('it2me-visited', true);
  remoting.showOrHideIt2MeUi();
};

remoting.showMe2MeUiAndSave = function() {
  window.localStorage.setItem('me2me-visited', true);
  remoting.showOrHideMe2MeUi();
};

remoting.resetInfographics = function() {
  window.localStorage.removeItem('it2me-visited');
  window.localStorage.removeItem('me2me-visited');
  remoting.showOrHideIt2MeUi();
  remoting.showOrHideMe2MeUi();
}


/**
 * Initialize all modal dialogs (class kd-modaldialog), adding event handlers
 * to confine keyboard navigation to child controls of the dialog when it is
 * shown and restore keyboard navigation when it is hidden.
 */
remoting.initModalDialogs = function() {
  var dialogs = document.querySelectorAll('.kd-modaldialog');
  var observer = new WebKitMutationObserver(confineOrRestoreFocus_);
  var options = {
    subtree: false,
    attributes: true
  };
  for (var i = 0; i < dialogs.length; ++i) {
    observer.observe(dialogs[i], options);
  }
};

/**
 * @param {Array.<MutationRecord>} mutations The set of mutations affecting
 *     an observed node.
 */
function confineOrRestoreFocus_(mutations) {
  // The list of mutations can include duplicates, so reduce it to a canonical
  // show/hide list.
  /** @type {Array.<Element>} */
  var shown = [];
  /** @type {Array.<Element>} */
  var hidden = [];
  for (var i = 0; i < mutations.length; ++i) {
    var mutation = mutations[i];
    if (mutation.type == 'attributes' &&
        mutation.attributeName == 'hidden') {
      var node = mutation.target;
      if (node.hidden && hidden.indexOf(node) == -1) {
        hidden.push(node);
      } else if (!node.hidden && shown.indexOf(node) == -1) {
        shown.push(node);
      }
    }
  }
  var kSavedAttributeName = 'data-saved-tab-index';
  // If any dialogs have been dismissed, restore all the tabIndex attributes.
  if (hidden.length != 0) {
    var elements = document.querySelectorAll('[' + kSavedAttributeName + ']');
    for (var i = 0 ; i < elements.length; ++i) {
      var element = /** @type {Element} */ elements[i];
      element.tabIndex = element.getAttribute(kSavedAttributeName);
      element.removeAttribute(kSavedAttributeName);
    }
  }
  // If any dialogs have been shown, confine keyboard navigation to the first
  // one. We don't have nested modal dialogs, so this will suffice for now.
  if (shown.length != 0) {
    var selector = '[tabIndex],a,area,button,input,select,textarea';
    var disable = document.querySelectorAll(selector);
    var except = shown[0].querySelectorAll(selector);
    for (var i = 0; i < disable.length; ++i) {
      var element = /** @type {Element} */ disable[i];
      var removeFromKeyboardNavigation = true;
      for (var j = 0; j < except.length; ++j) {  // No indexOf on NodeList
        if (element == except[j]) {
          removeFromKeyboardNavigation = false;
          break;
        }
      }
      if (removeFromKeyboardNavigation) {
        element.setAttribute(kSavedAttributeName, element.tabIndex);
        element.tabIndex = -1;
      }
    }
  }
}
