// chrome-api-shim.js
//
// Runs once per page, in an isolated JS world shared by every extension's
// content scripts (see WebExtensionManager::EXTENSIONS_WORLD_ID) — never in
// the page's own world, so ordinary site JS never sees any of this. It is
// concatenated *after* qwebchannel.js by WebExtensionManager, so `QWebChannel`
// is already defined by the time this file runs.
//
// Everything here is declared with `var`, not `window.x = `, on purpose:
// inside a QtWebEngine isolated world, `var` stays private to that world,
// while `window.x = ...` would leak the property onto the real page's
// `window` object. Background pages are the one exception (see
// WebExtensionManager::startBackgroundPage), where `window.chrome` is set
// deliberately since that page is 100% our own content.
//
// Talks to ChromeExtensionAPI (see src/ChromeExtensionAPI.h) over the
// shared "chromeBridge" QWebChannel object. Every namespace below is only
// attached to the returned `chrome` object when the extension's manifest
// actually declares the matching permission — mirrors real Chrome hiding
// APIs an extension never asked for. The C++ side re-checks permissions on
// every call too (see ChromeExtensionAPI.cpp), since this is just UX, not
// the actual security boundary.

var __lbExtBridge = null;
var __lbExtChannelReady = false;
var __lbExtQueue = [];
var __lbExtRegistry = {}; // extensionId -> already-built chrome-shaped API object

function __lbExtRunOrQueue(fn) {
    if (__lbExtChannelReady) fn();
    else __lbExtQueue.push(fn);
}

function __lbExtInitChannel() {
    if (typeof qt === "undefined" || !qt.webChannelTransport) {
        setTimeout(__lbExtInitChannel, 20);
        return;
    }
    new QWebChannel(qt.webChannelTransport, function (channel) {
        __lbExtBridge = channel.objects.chromeBridge;
        __lbExtChannelReady = true;

        __lbExtBridge.messageReceived.connect(function (extId, senderContextId, messageJson) {
            var api = __lbExtRegistry[extId];
            if (!api || senderContextId === api.__contextId) return;
            var message;
            try { message = JSON.parse(messageJson); } catch (e) { message = messageJson; }
            (api.__onMessageListeners || []).forEach(function (fn) {
                try { fn(message, { id: extId }, function () {}); } catch (e) { /* listener error, ignore */ }
            });
        });

        __lbExtBridge.scriptExecuted.connect(function (extId, contextId, requestId, resultJson) {
            var api = __lbExtRegistry[extId];
            if (!api || contextId !== api.__contextId) return;
            var cb = api.__pendingScripts && api.__pendingScripts[requestId];
            if (!cb) return;
            delete api.__pendingScripts[requestId];
            var parsed;
            try { parsed = JSON.parse(resultJson); } catch (e) { parsed = {}; }
            cb(parsed && "result" in parsed ? [parsed.result] : []);
        });

        __lbExtBridge.alarmFired.connect(function (extId, name) {
            var api = __lbExtRegistry[extId];
            if (!api) return;
            (api.__onAlarmListeners || []).forEach(function (fn) {
                try { fn({ name: name }); } catch (e) { /* listener error, ignore */ }
            });
        });

        var pending = __lbExtQueue;
        __lbExtQueue = [];
        pending.forEach(function (fn) { fn(); });
    });
}
__lbExtInitChannel();

// --- Chrome match-pattern matching (scheme://host/path, "*" wildcards,
// "<all_urls>") — used to gate content scripts against manifest `matches`,
// since QWebEngineScript itself has no notion of per-script URL matching.
// (ChromeExtensionAPI.cpp has its own copy of this same logic, used to
// enforce host permissions server-side — the two need to agree.) ---
function __lbExtEscapeRegExp(s) {
    return s.replace(/[.+?^${}()|[\]\\]/g, "\\$&");
}
function __lbExtPatternTest(pattern, href) {
    if (pattern === "<all_urls>") return /^(https?|file|ftp):/.test(href);
    var m = pattern.match(/^(\*|https?|file|ftp):\/\/(\*|(?:\*\.)?[^\/]+)(\/.*)$/);
    if (!m) return false;
    var scheme = m[1], host = m[2], path = m[3];
    var u = href.match(/^([a-zA-Z]+):\/\/([^\/]*)(\/.*)?$/);
    if (!u) return false;
    var uScheme = u[1], uHost = u[2], uPath = u[3] || "/";
    if (scheme !== "*" && scheme.toLowerCase() !== uScheme.toLowerCase()) return false;
    if (host !== "*") {
        if (host.indexOf("*.") === 0) {
            var base = host.slice(2);
            if (uHost !== base && uHost.slice(-(base.length + 1)) !== ("." + base)) return false;
        } else if (host !== uHost) return false;
    }
    var re = new RegExp("^" + path.split("*").map(__lbExtEscapeRegExp).join(".*") + "$");
    return re.test(uPath);
}
function __lbExtMatches(patterns) {
    for (var i = 0; i < patterns.length; i++) {
        if (__lbExtPatternTest(patterns[i], location.href)) return true;
    }
    return false;
}

var __lbExtRequestSeq = 0;
function __lbExtNextRequestId() {
    __lbExtRequestSeq += 1;
    return "r" + __lbExtRequestSeq;
}

// --- Builds (and caches) the chrome-shaped object for one extension. Every
// content script instance of the same extension on the same page shares one
// of these, so e.g. onMessage listeners registered by one content script
// see messages regardless of which content script's IIFE is "active". ---
function __lbExtGetAPI(extId, manifest, contextId) {
    if (__lbExtRegistry[extId]) return __lbExtRegistry[extId];

    var perms = manifest.permissions || [];
    function has(p) { return perms.indexOf(p) !== -1; }

    var api = {
        __contextId: contextId,
        __onMessageListeners: [],
        __onAlarmListeners: [],
        __pendingScripts: {},
        runtime: {
            id: extId,
            getManifest: function () { return manifest; },
            sendMessage: function (message, callback) {
                __lbExtRunOrQueue(function () {
                    __lbExtBridge.runtimeSendMessage(extId, contextId, JSON.stringify(message));
                    if (callback) callback();
                });
            },
            onMessage: {
                addListener: function (fn) { api.__onMessageListeners.push(fn); }
            }
        },
        storage: {
            local: __lbExtMakeStorageArea(extId, "local"),
            session: __lbExtMakeStorageArea(extId, "session")
        },
        permissions: {
            // Real chrome.permissions.contains checks both `permissions`
            // and `origins` (host match patterns) in one request object.
            contains: function (request, callback) {
                var wantedPerms = (request && request.permissions) || [];
                var wantedOrigins = (request && request.origins) || [];
                __lbExtRunOrQueue(function () {
                    var ok = wantedPerms.every(function (p) { return __lbExtBridge.hasPermission(extId, p); })
                        && wantedOrigins.every(function (o) { return __lbExtBridge.hasHostPermission(extId, o); });
                    callback(ok);
                });
            }
        }
    };

    // chrome.tabs is only exposed at all when the manifest declares the
    // "tabs" permission — mirrors real Chrome's behaviour of hiding APIs
    // an extension never asked for. C++ re-checks the same permission on
    // every call (see ChromeExtensionAPI::tabsCreate/Update/Remove).
    if (has("tabs")) {
        api.tabs = {
            query: function (queryInfo, callback) {
                __lbExtRunOrQueue(function () {
                    var tabs = JSON.parse(__lbExtBridge.tabsQuery(extId));
                    if (queryInfo && queryInfo.active) tabs = tabs.filter(function (t) { return t.active; });
                    callback(tabs);
                });
            },
            create: function (createProperties, callback) {
                __lbExtRunOrQueue(function () {
                    var id = __lbExtBridge.tabsCreate(extId, (createProperties && createProperties.url) || "",
                        !createProperties || createProperties.active !== false);
                    if (callback) callback({ id: id });
                });
            },
            update: function (tabId, updateProperties, callback) {
                __lbExtRunOrQueue(function () {
                    var ok = __lbExtBridge.tabsUpdate(extId, tabId, (updateProperties && updateProperties.url) || "");
                    if (callback) callback(ok ? { id: tabId } : undefined);
                });
            },
            remove: function (tabId, callback) {
                __lbExtRunOrQueue(function () {
                    __lbExtBridge.tabsRemove(extId, tabId);
                    if (callback) callback();
                });
            },
            // Best-effort: this runtime tracks one shared context id per
            // extension for *all* of its content-script instances (see
            // WebExtensionManager::injectContentScripts), not one per tab,
            // so a real per-tab-targeted send isn't distinguishable from a
            // runtime.sendMessage broadcast yet. Same limitation the C++
            // side documents for runtime messaging in general.
            sendMessage: function (tabId, message, callback) {
                __lbExtRunOrQueue(function () {
                    __lbExtBridge.runtimeSendMessage(extId, contextId, JSON.stringify(message));
                    if (callback) callback();
                });
            }
        };
    }

    // chrome.scripting.executeScript — requires "scripting" *and* either
    // "activeTab" or a host_permissions match for the target tab's current
    // URL, enforced in ChromeExtensionAPI::scriptingExecute, not just here.
    if (has("scripting")) {
        api.scripting = {
            executeScript: function (injection, callback) {
                __lbExtRunOrQueue(function () {
                    var tabId = injection && injection.target && injection.target.tabId;
                    var fn = injection && injection.func;
                    var args = (injection && injection.args) || [];
                    if (typeof fn !== "function") {
                        if (callback) callback([]);
                        return;
                    }
                    var code = "(" + fn.toString() + ")(" + args.map(function (a) { return JSON.stringify(a); }).join(",") + ")";
                    var reqId = __lbExtNextRequestId();
                    api.__pendingScripts[reqId] = function (results) {
                        if (callback) callback(results.map(function (r) { return { result: r }; }));
                    };
                    __lbExtBridge.scriptingExecute(extId, contextId, tabId, code, reqId);
                });
            }
        };
    }

    // chrome.alarms — gated the same way; C++ also refuses to create an
    // alarm for an extension that lacks the permission.
    if (has("alarms")) {
        api.alarms = {
            create: function (name, alarmInfo) {
                if (typeof name === "object") { alarmInfo = name; name = ""; }
                alarmInfo = alarmInfo || {};
                __lbExtRunOrQueue(function () {
                    __lbExtBridge.alarmsCreate(extId, name || "",
                        alarmInfo.delayInMinutes || 0, alarmInfo.periodInMinutes || 0);
                });
            },
            clear: function (name, callback) {
                __lbExtRunOrQueue(function () {
                    __lbExtBridge.alarmsClear(extId, name || "");
                    if (callback) callback(true);
                });
            },
            clearAll: function (callback) {
                __lbExtRunOrQueue(function () {
                    __lbExtBridge.alarmsClearAll(extId);
                    if (callback) callback(true);
                });
            },
            getAll: function (callback) {
                __lbExtRunOrQueue(function () {
                    callback(JSON.parse(__lbExtBridge.alarmsGetAll(extId)));
                });
            },
            onAlarm: {
                addListener: function (fn) { api.__onAlarmListeners.push(fn); }
            }
        };
    }

    // chrome.action — badge/title only (no icon rendering pipeline yet);
    // not permission-gated in real Chrome either, so always present.
    api.action = {
        setBadgeText: function (details) {
            __lbExtRunOrQueue(function () { __lbExtBridge.actionSetBadgeText(extId, (details && details.text) || ""); });
        },
        setBadgeBackgroundColor: function (details) {
            __lbExtRunOrQueue(function () { __lbExtBridge.actionSetBadgeBackgroundColor(extId, (details && details.color) || ""); });
        },
        setTitle: function (details) {
            __lbExtRunOrQueue(function () { __lbExtBridge.actionSetTitle(extId, (details && details.title) || ""); });
        }
    };

    __lbExtRegistry[extId] = api;
    return api;
}

// One storage.local / storage.session sub-object; both share the same
// call shapes, only the `area` string sent to the bridge differs.
function __lbExtMakeStorageArea(extId, area) {
    return {
        get: function (keys, callback) {
            __lbExtRunOrQueue(function () {
                var arg = keys == null ? "null" : JSON.stringify(Array.isArray(keys) ? keys : Object.keys(keys));
                callback(JSON.parse(__lbExtBridge.storageGet(extId, area, arg)));
            });
        },
        set: function (items, callback) {
            __lbExtRunOrQueue(function () {
                __lbExtBridge.storageSet(extId, area, JSON.stringify(items));
                if (callback) callback();
            });
        },
        remove: function (keys, callback) {
            __lbExtRunOrQueue(function () {
                __lbExtBridge.storageRemove(extId, area, JSON.stringify(Array.isArray(keys) ? keys : [keys]));
                if (callback) callback();
            });
        },
        clear: function (callback) {
            __lbExtRunOrQueue(function () {
                __lbExtBridge.storageClear(extId, area);
                if (callback) callback();
            });
        }
    };
}
