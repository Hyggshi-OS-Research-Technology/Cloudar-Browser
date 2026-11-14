// LightBrowser Chrome Extension API shim.
//
// A small, honest subset of the Chrome `chrome.*` / Firefox `browser.*`
// API that content scripts can actually call inside Qt WebEngine, which
// ships no native extension engine. The C++ loader (ChromeExtensionManager)
// embeds this file in a closure that also defines EXT_ID and EXT_MANIFEST,
// so each extension gets its own storage namespace, runtime.id and message
// listener while sharing one in-page object.
//
// This is deliberately not a full implementation: there is no background
// page, no permission model, and chrome.tabs is a stub. It covers the APIs
// a typical content script touches first (storage, runtime.id/getManifest/
// getURL, runtime messages), which is enough for lightweight tweaks.
(function () {
  "use strict";

  if (typeof EXT_ID !== "string") return {};

  function pick(obj, path) {
    var parts = String(path).split(".");
    var cur = obj;
    for (var i = 0; i < parts.length; i++) {
      if (cur == null) return undefined;
      cur = cur[parts[i]];
    }
    return cur;
  }

  // ---- chrome.storage (persisted through localStorage, namespaced) ----
  var STORE_KEY = "__lb_chrome_storage__";
  function storagePrefix() { return "ext:" + EXT_ID + ":"; }
  function readAll() {
    try { return JSON.parse(localStorage.getItem(STORE_KEY) || "{}"); }
    catch (e) { return {}; }
  }
  function readNamespace() {
    var all = readAll();
    var ns = all[storagePrefix()];
    return (ns && typeof ns === "object") ? ns : {};
  }
  function writeNamespace(ns) {
    var all = readAll();
    all[storagePrefix()] = ns;
    try { localStorage.setItem(STORE_KEY, JSON.stringify(all)); } catch (e) {}
  }

  function makeStorageArea() {
    return {
      get: function (keys, cb) {
        var ns = readNamespace();
        var res = {};
        if (keys == null) {
          res = ns;
        } else if (typeof keys === "string") {
          res[keys] = pick(ns, keys);
        } else if (Array.isArray(keys)) {
          keys.forEach(function (k) { res[k] = pick(ns, k); });
        } else if (typeof keys === "object") {
          Object.keys(keys).forEach(function (k) { res[k] = (k in ns) ? ns[k] : keys[k]; });
        }
        return Promise.resolve(res).then(cb || function () {});
      },
      set: function (items, cb) {
        var ns = readNamespace();
        if (items && typeof items === "object") {
          Object.keys(items).forEach(function (k) { ns[k] = items[k]; });
        }
        writeNamespace(ns);
        return Promise.resolve().then(cb || function () {});
      },
      remove: function (keys, cb) {
        var ns = readNamespace();
        var list = typeof keys === "string" ? [keys] : (Array.isArray(keys) ? keys : []);
        list.forEach(function (k) { delete ns[k]; });
        writeNamespace(ns);
        return Promise.resolve().then(cb || function () {});
      },
      clear: function (cb) {
        writeNamespace({});
        return Promise.resolve().then(cb || function () {});
      },
      getBytesInUse: function (keys, cb) {
        var ns = readNamespace();
        var approx = 0;
        var list = keys == null ? Object.keys(ns) : (typeof keys === "string" ? [keys] : keys);
        list.forEach(function (k) {
          if (k in ns) {
            try { approx += String(JSON.stringify(ns[k])).length; } catch (e) {}
          }
        });
        return Promise.resolve(approx).then(cb || function () {});
      }
    };
  }

  // ---- tiny event object used for runtime.onMessage ------------------
  function makeEvent() {
    var listeners = [];
    return {
      addListener: function (fn) { if (listeners.indexOf(fn) < 0) listeners.push(fn); },
      removeListener: function (fn) {
        var i = listeners.indexOf(fn);
        if (i >= 0) listeners.splice(i, 1);
      },
      hasListener: function (fn) { return listeners.indexOf(fn) >= 0; },
      hasListeners: function () { return listeners.length > 0; },
      _dispatch: function () {
        var args = Array.prototype.slice.call(arguments);
        listeners.slice().forEach(function (fn) { try { fn.apply(null, args); } catch (e) {} });
      }
    };
  }

  // Shared in-page message bus so one content script can sendMessage to
  // another extension's onMessage on the same page.
  function bus() {
    if (!globalThis.__lbExtMessageBus) globalThis.__lbExtMessageBus = [];
    return globalThis.__lbExtMessageBus;
  }

  var runtime = {
    id: EXT_ID,
    lastError: null,
    getManifest: function () { return EXT_MANIFEST || {}; },
    getURL: function (path) {
      return "chrome-extension://" + EXT_ID + "/" + String(path).replace(/^\/+/, "");
    },
    onMessage: makeEvent(),
    onConnect: makeEvent(),
    onInstalled: makeEvent(),
    sendMessage: function (message) {
      bus().forEach(function (entry) {
        if (entry.id !== EXT_ID) {
          try {
            entry.onMessage._dispatch(message, { id: entry.id, url: location.href }, function () {});
          } catch (e) {}
        }
      });
      return Promise.resolve();
    }
  };

  // chrome.tabs: the lightweight host has no background / tab model, so this
  // is a safe stub (query returns an empty list rather than crashing).
  var tabs = {
    query: function (queryInfo, cb) {
      return Promise.resolve([]).then(cb || function () {});
    },
    create: function (props, cb) { return Promise.resolve(null).then(cb || function () {}); },
    update: function (tabId, props, cb) { return Promise.resolve().then(cb || function () {}); },
    get: function (tabId, cb) { return Promise.resolve(null).then(cb || function () {}); }
  };

  var api = {
    runtime: runtime,
    tabs: tabs,
    storage: { local: makeStorageArea(), sync: makeStorageArea() },
    extension: { getURL: runtime.getURL, getBackgroundPage: function () { return null; } },
    __lbApi: true
  };

  // Register this extension's onMessage in the shared page bus.
  bus().push({ id: EXT_ID, onMessage: runtime.onMessage });

  // Expose a global `chrome`/`browser` for the page itself (only once, so
  // we never clobber the first extension's object with later ones).
  if (globalThis.chrome === undefined) globalThis.chrome = api;
  if (globalThis.browser === undefined) globalThis.browser = api;

  return api;
})()

