(function() {
    'use strict';

    // ─── Embedded Qt QWebChannel client library ─────────────────────────
    // Official Qt qwebchannel.js (LGPL-3.0/GPL-2.0/GPL-3.0), bundled so this
    // content-script can talk back to the Python side without any network
    // request (works around page CSP, and lets us do REAL downloads for
    // sites like YouTube where a plain <a download> click cannot work).
// Copyright (C) 2016 The Qt Company Ltd.
// Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:critical reason:data-parser

"use strict";

var QWebChannelMessageTypes = {
    signal: 1,
    propertyUpdate: 2,
    init: 3,
    idle: 4,
    debug: 5,
    invokeMethod: 6,
    connectToSignal: 7,
    disconnectFromSignal: 8,
    setProperty: 9,
    response: 10,
};

var QWebChannel = function(transport, initCallback, converters)
{
    if (typeof transport !== "object" || typeof transport.send !== "function") {
        console.error("The QWebChannel expects a transport object with a send function and onmessage callback property." +
                      " Given is: transport: " + typeof(transport) + ", transport.send: " + typeof(transport.send));
        return;
    }

    var channel = this;
    this.transport = transport;

    var converterRegistry =
    {
        Date : function(response) {
            if (typeof response === "string"
                && response.match(
                        /^-?\d+-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d*)?([-+\u2212](\d{2}):(\d{2})|Z)?$/)) {
                var date = new Date(response);
                if (!isNaN(date))
                    return date;
            }
            return undefined; // Return undefined if current converter is not applicable
        }
    };

    this.usedConverters = [];

    this.addConverter = function(converter)
    {
        if (typeof converter === "string") {
            if (converterRegistry.hasOwnProperty(converter))
                this.usedConverters.push(converterRegistry[converter]);
            else
                console.error("Converter '" + converter + "' not found");
        } else if (typeof converter === "function") {
            this.usedConverters.push(converter);
        } else {
            console.error("Invalid converter object type " + typeof converter);
        }
    }

    if (Array.isArray(converters)) {
        for (const converter of converters)
            this.addConverter(converter);
    } else if (converters !== undefined) {
        this.addConverter(converters);
    }

    this.send = function(data)
    {
        if (typeof(data) !== "string") {
            data = JSON.stringify(data);
        }
        channel.transport.send(data);
    }

    this.transport.onmessage = function(message)
    {
        var data = message.data;
        if (typeof data === "string") {
            data = JSON.parse(data);
        }
        switch (data.type) {
            case QWebChannelMessageTypes.signal:
                channel.handleSignal(data);
                break;
            case QWebChannelMessageTypes.response:
                channel.handleResponse(data);
                break;
            case QWebChannelMessageTypes.propertyUpdate:
                channel.handlePropertyUpdate(data);
                break;
            default:
                console.error("invalid message received:", message.data);
                break;
        }
    }

    this.execCallbacks = {};
    this.execId = 0;
    this.exec = function(data, callback)
    {
        if (!callback) {
            // if no callback is given, send directly
            channel.send(data);
            return;
        }
        if (channel.execId === Number.MAX_VALUE) {
            // wrap
            channel.execId = Number.MIN_VALUE;
        }
        if (data.hasOwnProperty("id")) {
            console.error("Cannot exec message with property id: " + JSON.stringify(data));
            return;
        }
        data.id = channel.execId++;
        channel.execCallbacks[data.id] = callback;
        channel.send(data);
    };

    this.objects = {};

    this.handleSignal = function(message)
    {
        var object = channel.objects[message.object];
        if (object) {
            object.signalEmitted(message.signal, message.args);
        } else {
            console.warn("Unhandled signal: " + message.object + "::" + message.signal);
        }
    }

    this.handleResponse = function(message)
    {
        if (!message.hasOwnProperty("id")) {
            console.error("Invalid response message received: ", JSON.stringify(message));
            return;
        }
        channel.execCallbacks[message.id](message.data);
        delete channel.execCallbacks[message.id];
    }

    this.handlePropertyUpdate = function(message)
    {
        message.data.forEach(data => {
            var object = channel.objects[data.object];
            if (object) {
                object.propertyUpdate(data.signals, data.properties);
            } else {
                console.warn("Unhandled property update: " + data.object + "::" + data.signal);
            }
        });
        channel.exec({type: QWebChannelMessageTypes.idle});
    }

    this.debug = function(message)
    {
        channel.send({type: QWebChannelMessageTypes.debug, data: message});
    };

    channel.exec({type: QWebChannelMessageTypes.init}, function(data) {
        for (const objectName of Object.keys(data)) {
            new QObject(objectName, data[objectName], channel);
        }

        // now unwrap properties, which might reference other registered objects
        for (const objectName of Object.keys(channel.objects)) {
            channel.objects[objectName].unwrapProperties();
        }

        if (initCallback) {
            initCallback(channel);
        }
        channel.exec({type: QWebChannelMessageTypes.idle});
    });
};

function QObject(name, data, webChannel)
{
    this.__id__ = name;
    webChannel.objects[name] = this;

    // List of callbacks that get invoked upon signal emission
    this.__objectSignals__ = {};

    // Cache of all properties, updated when a notify signal is emitted
    this.__propertyCache__ = {};

    var object = this;

    // ----------------------------------------------------------------------

    this.unwrapQObject = function(response)
    {
        for (const converter of webChannel.usedConverters) {
            var result = converter(response);
            if (result !== undefined)
                return result;
        }

        if (response instanceof Array) {
            // support list of objects
            return response.map(qobj => object.unwrapQObject(qobj))
        }
        if (!(response instanceof Object))
            return response;

        if (!response["__QObject*__"] || response.id === undefined) {
            var jObj = {};
            for (const propName of Object.keys(response)) {
                jObj[propName] = object.unwrapQObject(response[propName]);
            }
            return jObj;
        }

        var objectId = response.id;
        if (webChannel.objects[objectId])
            return webChannel.objects[objectId];

        if (!response.data) {
            console.error("Cannot unwrap unknown QObject " + objectId + " without data.");
            return;
        }

        var qObject = new QObject( objectId, response.data, webChannel );
        qObject.destroyed.connect(function() {
            if (webChannel.objects[objectId] === qObject) {
                delete webChannel.objects[objectId];
                // reset the now deleted QObject to an empty {} object
                // just assigning {} though would not have the desired effect, but the
                // below also ensures all external references will see the empty map
                // NOTE: this detour is necessary to workaround QTBUG-40021
                Object.keys(qObject).forEach(name => delete qObject[name]);
            }
        });
        // here we are already initialized, and thus must directly unwrap the properties
        qObject.unwrapProperties();
        return qObject;
    }

    this.unwrapProperties = function()
    {
        for (const propertyIdx of Object.keys(object.__propertyCache__)) {
            object.__propertyCache__[propertyIdx] = object.unwrapQObject(object.__propertyCache__[propertyIdx]);
        }
    }

    function addSignal(signalData, isPropertyNotifySignal)
    {
        var signalName = signalData[0];
        var signalIndex = signalData[1];
        object[signalName] = {
            connect: function(callback) {
                if (typeof(callback) !== "function") {
                    console.error("Bad callback given to connect to signal " + signalName);
                    return;
                }

                object.__objectSignals__[signalIndex] = object.__objectSignals__[signalIndex] || [];
                object.__objectSignals__[signalIndex].push(callback);

                // only required for "pure" signals, handled separately for properties in propertyUpdate
                if (isPropertyNotifySignal)
                    return;

                // also note that we always get notified about the destroyed signal
                if (signalName === "destroyed" || signalName === "destroyed()" || signalName === "destroyed(QObject*)")
                    return;

                // and otherwise we only need to be connected only once
                if (object.__objectSignals__[signalIndex].length == 1) {
                    webChannel.exec({
                        type: QWebChannelMessageTypes.connectToSignal,
                        object: object.__id__,
                        signal: signalIndex
                    });
                }
            },
            disconnect: function(callback) {
                if (typeof(callback) !== "function") {
                    console.error("Bad callback given to disconnect from signal " + signalName);
                    return;
                }
                // This makes a new list. This is important because it won't interfere with
                // signal processing if a disconnection happens while emittig a signal
                object.__objectSignals__[signalIndex] = (object.__objectSignals__[signalIndex] || []).filter(function(c) {
                  return c != callback;
                });
                if (!isPropertyNotifySignal && object.__objectSignals__[signalIndex].length === 0) {
                    // only required for "pure" signals, handled separately for properties in propertyUpdate
                    webChannel.exec({
                        type: QWebChannelMessageTypes.disconnectFromSignal,
                        object: object.__id__,
                        signal: signalIndex
                    });
                }
            }
        };
    }

    /**
     * Invokes all callbacks for the given signalname. Also works for property notify callbacks.
     */
    function invokeSignalCallbacks(signalName, signalArgs)
    {
        var connections = object.__objectSignals__[signalName];
        if (connections) {
            connections.forEach(function(callback) {
                callback.apply(callback, signalArgs);
            });
        }
    }

    this.propertyUpdate = function(signals, propertyMap)
    {
        // update property cache
        for (const propertyIndex of Object.keys(propertyMap)) {
            var propertyValue = propertyMap[propertyIndex];
            object.__propertyCache__[propertyIndex] = this.unwrapQObject(propertyValue);
        }

        for (const signalName of Object.keys(signals)) {
            // Invoke all callbacks, as signalEmitted() does not. This ensures the
            // property cache is updated before the callbacks are invoked.
            invokeSignalCallbacks(signalName, signals[signalName]);
        }
    }

    this.signalEmitted = function(signalName, signalArgs)
    {
        invokeSignalCallbacks(signalName, this.unwrapQObject(signalArgs));
    }

    function addMethod(methodData)
    {
        var methodName = methodData[0];
        var methodIdx = methodData[1];

        // Fully specified methods are invoked by id, others by name for host-side overload resolution
        var invokedMethod = methodName[methodName.length - 1] === ')' ? methodIdx : methodName

        object[methodName] = function() {
            var args = [];
            var callback;
            var errCallback;
            for (var i = 0; i < arguments.length; ++i) {
                var argument = arguments[i];
                if (typeof argument === "function")
                    callback = argument;
                else
                    args.push(argument);
            }

            var result;
            // during test, webChannel.exec synchronously calls the callback
            // therefore, the promise must be constucted before calling
            // webChannel.exec to ensure the callback is set up
            if (!callback && (typeof(Promise) === 'function')) {
              result = new Promise(function(resolve, reject) {
                callback = resolve;
                errCallback = reject;
              });
            }

            webChannel.exec({
                "type": QWebChannelMessageTypes.invokeMethod,
                "object": object.__id__,
                "method": invokedMethod,
                "args": args
            }, function(response) {
                if (response !== undefined) {
                    var result = object.unwrapQObject(response);
                    if (callback) {
                        (callback)(result);
                    }
                } else if (errCallback) {
                  (errCallback)();
                }
            });

            return result;
        };
    }

    function bindGetterSetter(propertyInfo)
    {
        var propertyIndex = propertyInfo[0];
        var propertyName = propertyInfo[1];
        var notifySignalData = propertyInfo[2];
        // initialize property cache with current value
        // NOTE: if this is an object, it is not directly unwrapped as it might
        // reference other QObject that we do not know yet
        object.__propertyCache__[propertyIndex] = propertyInfo[3];

        if (notifySignalData) {
            if (notifySignalData[0] === 1) {
                // signal name is optimized away, reconstruct the actual name
                notifySignalData[0] = propertyName + "Changed";
            }
            addSignal(notifySignalData, true);
        }

        Object.defineProperty(object, propertyName, {
            configurable: true,
            get: function () {
                var propertyValue = object.__propertyCache__[propertyIndex];
                if (propertyValue === undefined) {
                    // This shouldn't happen
                    console.warn("Undefined value in property cache for property \"" + propertyName + "\" in object " + object.__id__);
                }

                return propertyValue;
            },
            set: function(value) {
                if (value === undefined) {
                    console.warn("Property setter for " + propertyName + " called with undefined value!");
                    return;
                }
                object.__propertyCache__[propertyIndex] = value;
                var valueToSend = value;
                webChannel.exec({
                    "type": QWebChannelMessageTypes.setProperty,
                    "object": object.__id__,
                    "property": propertyIndex,
                    "value": valueToSend
                });
            }
        });

    }

    // ----------------------------------------------------------------------

    data.methods.forEach(addMethod);

    data.properties.forEach(bindGetterSetter);

    data.signals.forEach(function(signal) { addSignal(signal, false); });

    Object.assign(object, data.enums);
}

QObject.prototype.toJSON = function() {
    if (this.__id__ === undefined) return {};
    return {
        id: this.__id__,
        "__QObject*__": true
    };
};

//required for use with nodejs
if (typeof module === 'object') {
    module.exports = {
        QWebChannel: QWebChannel
    };
}


    // ─── Utility functions ───────────────────────────────────────────────
    function getVideoSources() {
        const sources = [];

        // 1. Direct <video> elements with a src attribute
        document.querySelectorAll('video[src]').forEach(function(v) {
            const src = v.getAttribute('src');
            if (src && !sources.some(s => s.url === src)) {
                sources.push({
                    url: src,
                    name: extractFileName(src, v.getAttribute('title') || v.title || 'video'),
                    type: 'video',
                    mime: v.getAttribute('type') || ''
                });
            }
        });

        // 2. <video> elements with <source> children
        document.querySelectorAll('video').forEach(function(v) {
            v.querySelectorAll('source').forEach(function(s) {
                const src = s.getAttribute('src');
                if (src && !sources.some(ss => ss.url === src)) {
                    sources.push({
                        url: src,
                        name: extractFileName(src, v.getAttribute('title') || v.title || 'video'),
                        type: 'video',
                        mime: s.getAttribute('type') || ''
                    });
                }
            });
        });

        // 3. Detect embedded video iframes (YouTube, Vimeo, etc.)
        document.querySelectorAll('iframe[src*="youtube"], iframe[src*="vimeo"], iframe[src*="dailymotion"], iframe[src*="facebook.com/plugins/video"]').forEach(function(iframe) {
            const src = iframe.getAttribute('src');
            if (src && !sources.some(s => s.url === src)) {
                sources.push({
                    url: src,
                    name: extractFileName(src, 'Embedded Video'),
                    type: 'embed',
                    mime: 'text/html'
                });
            }
        });

        return sources;
    }

    // Minimum rendered/natural size (px) for an <img> to be considered a
    // real photo/picture rather than an icon, avatar, or tracking pixel.
    const MIN_IMAGE_DIMENSION = 96;
    // Cap how many images we surface at once so image-heavy pages
    // (galleries, social feeds) don't produce an unusable giant list.
    const MAX_IMAGES = 40;

    function isDataOrBlobUrl(url) {
        return /^(data:|blob:)/i.test(url || '');
    }

    function imageDimension(img) {
        const w = img.naturalWidth || img.clientWidth || 0;
        const h = img.naturalHeight || img.clientHeight || 0;
        return { w: w, h: h };
    }

    // Minimum estimated pixel dimension (width or height) for an image to
    // be treated as an "original" rather than a thumbnail/preview. Used by
    // the "Chỉ hiện ảnh gốc" toggle in the picker.
    const ORIGINAL_MIN_DIMENSION = 400;

    function extForUrl(url) {
        try {
            const path = new URL(url).pathname.toLowerCase();
            const m = path.match(/\.([a-z0-9]{2,5})$/);
            if (m) return m[1] === 'jpeg' ? 'jpg' : m[1];
        } catch (e) {}
        return '';
    }

    // Picks the highest-resolution candidate out of a `srcset`/`data-srcset`
    // attribute (e.g. "img-400.jpg 400w, img-1200.jpg 1200w"), since that's
    // usually the closest thing to the "original" full-size image — the
    // `src`/`currentSrc` the browser actually rendered is often a smaller
    // variant chosen for the current viewport/DPR.
    function parseSrcsetBest(srcset) {
        if (!srcset) return null;
        const candidates = srcset.split(',').map(function(entry) {
            const parts = entry.trim().split(/\s+/);
            const url = parts[0];
            const desc = parts[1] || '';
            let width = 0, density = 1;
            if (desc.endsWith('w')) width = parseInt(desc, 10) || 0;
            else if (desc.endsWith('x')) density = parseFloat(desc) || 1;
            return { url: url, width: width, density: density };
        }).filter(function(c) { return !!c.url; });
        if (!candidates.length) return null;
        candidates.sort(function(a, b) {
            if (a.width || b.width) return b.width - a.width;
            return b.density - a.density;
        });
        return candidates[0];
    }

    // Detects real <img> content on the page (photos/pictures), filtering
    // out icons, avatars, and 1x1 tracking pixels by size. Cross-origin
    // CDN images are included — triggerDownload() falls back gracefully
    // if a fetch()-based download is blocked by CORS. Where a page offers
    // a `srcset` with a higher-resolution candidate than what's currently
    // rendered, that candidate is used as the download URL instead.
    function getImageSources() {
        const seen = new Set();
        const candidates = [];

        document.querySelectorAll('img').forEach(function(img) {
            let rawSrc = img.getAttribute('src') || '';
            // Common lazy-load patterns: the real image lives in a data-*
            // attribute until scroll-triggered JS swaps it into `src`.
            if (!rawSrc || isDataOrBlobUrl(rawSrc)) {
                rawSrc = img.getAttribute('data-src') ||
                         img.getAttribute('data-original') ||
                         img.getAttribute('data-lazy-src') || '';
            }
            if (!rawSrc || isDataOrBlobUrl(rawSrc)) return;

            const renderedSrc = img.currentSrc || rawSrc;
            const { w, h } = imageDimension(img);

            let bestUrl = resolveUrl(rawSrc);
            let estWidth = w, estHeight = h, isEstimated = false;

            const srcsetAttr = img.getAttribute('srcset') || img.getAttribute('data-srcset');
            const best = parseSrcsetBest(srcsetAttr);
            if (best) {
                const candidateUrl = resolveUrl(best.url);
                if (candidateUrl !== resolveUrl(renderedSrc)) {
                    bestUrl = candidateUrl;
                    if (best.width && w && h) {
                        // Approximate the original's height from the
                        // rendered image's aspect ratio — we can't know the
                        // exact pixel size of a candidate we haven't
                        // fetched/decoded.
                        estWidth = best.width;
                        estHeight = Math.round(best.width * (h / w));
                        isEstimated = true;
                    }
                } else {
                    bestUrl = candidateUrl;
                }
            }

            if (seen.has(bestUrl)) return;
            if (estWidth < MIN_IMAGE_DIMENSION && estHeight < MIN_IMAGE_DIMENSION) return;

            seen.add(bestUrl);
            candidates.push({
                url: bestUrl,
                name: extractFileName(bestUrl, 'image.jpg'),
                type: 'image',
                mime: '',
                previewSrc: renderedSrc,
                width: estWidth,
                height: estHeight,
                dimEstimated: isEstimated,
                ext: extForUrl(bestUrl) || extForUrl(renderedSrc) || '',
                isThumbnail: Math.max(estWidth, estHeight) < ORIGINAL_MIN_DIMENSION,
                area: estWidth * estHeight,
            });
        });

        // Prefer the largest/most prominent images when there are many.
        candidates.sort(function(a, b) { return b.area - a.area; });
        return candidates.slice(0, MAX_IMAGES);
    }

    // Combined list used by the on-demand "media on this page" picker.
    // The passive auto-popup still only triggers off getVideoSources()
    // (see checkForVideos) so pages full of ordinary photos don't get an
    // unsolicited popup — images are opt-in via the floating button.
    function getMediaSources() {
        return getVideoSources().concat(getImageSources());
    }

    function extractFileName(url, fallback) {
        try {
            // Try to extract filename from URL
            const u = new URL(url, window.location.href);
            const pathParts = u.pathname.split('/');
            let last = pathParts[pathParts.length - 1];
            if (last && last.includes('.')) {
                // Decode URI components
                try {
                    last = decodeURIComponent(last);
                } catch(e) {}
                // Remove query params
                const qIndex = last.indexOf('?');
                if (qIndex > -1) last = last.substring(0, qIndex);
                return last;
            }
        } catch(e) {}
        return fallback || 'video.mp4';
    }

    function resolveUrl(url) {
        try {
            return new URL(url, window.location.href).href;
        } catch(e) {
            return url;
        }
    }

    // Best-effort file size lookup for the picker. <img> tags never carry
    // Content-Length, so we ask the server directly; some CDNs reject HEAD
    // (405) so we retry with a 0-byte ranged GET before giving up.
    function fetchFileSizeLabel(url, callback) {
        function done(bytes) {
            callback(bytes ? formatFileSize(parseInt(bytes, 10)) : null);
        }
        fetch(url, { method: 'HEAD', mode: 'cors', credentials: 'omit' })
            .then(function(resp) {
                const len = resp.headers.get('content-length');
                if (len) { done(len); return; }
                // Some CDNs omit Content-Length on HEAD (or reject HEAD
                // outright) but honor a ranged GET, which reports the full
                // resource size via Content-Range: "bytes 0-0/<total>".
                return fetch(url, { method: 'GET', mode: 'cors', credentials: 'omit',
                                     headers: { 'Range': 'bytes=0-0' } })
                    .then(function(r2) {
                        const range = r2.headers.get('content-range');
                        const m = range && range.match(/\/(\d+)$/);
                        done(m ? m[1] : r2.headers.get('content-length'));
                    });
            })
            .catch(function() { callback(null); });
    }

    function formatFileSize(bytes) {
        if (!bytes || bytes <= 0) return 'Unknown size';
        const units = ['B', 'KB', 'MB', 'GB'];
        let i = 0;
        let size = bytes;
        while (size >= 1024 && i < units.length - 1) {
            size /= 1024;
            i++;
        }
        return size.toFixed(1) + ' ' + units[i];
    }

    // ─── YouTube detection ───────────────────────────────────────────────
    // YouTube's own player uses MSE (blob: URLs, segmented streams), so
    // `video[src]` never matches on youtube.com itself. We detect the
    // watch/shorts page directly and hand the real download off to Python
    // (via the bridge below), which uses yt-dlp instead of a raw <a> click.
    function isYouTubeSite() {
        const host = window.location.hostname;
        return host === 'youtu.be' || host === 'youtube.com' ||
               host.endsWith('.youtube.com');
    }

    function isYouTubeWatchUrl() {
        if (!isYouTubeSite()) return false;
        if (window.location.hostname === 'youtu.be') return true;
        return window.location.pathname === '/watch' ||
               window.location.pathname.startsWith('/shorts/');
    }

    function getYouTubeVideoId() {
        if (window.location.hostname === 'youtu.be') {
            return window.location.pathname.slice(1);
        }
        if (window.location.pathname.startsWith('/shorts/')) {
            return window.location.pathname.split('/')[2] || '';
        }
        try {
            return new URLSearchParams(window.location.search).get('v') || '';
        } catch (e) {
            return '';
        }
    }

    function getYouTubeTitle() {
        let t = document.title || 'YouTube Video';
        t = t.replace(/\s*-\s*YouTube\s*$/, '').trim();
        return t || 'YouTube Video';
    }

    // ─── Python bridge (QWebChannel) ──────────────────────────────────────
    let ytBridge = null;
    let ytBridgeSignalsBound = false;
    let ytBridgeRequestInFlight = false;
    const ytInfoCallbacks = {}; // url -> function(info, error)

    // Requests title/thumbnail/quality+size info for `url`. `callback` is
    // invoked once with (info, error) — error is null on success.
    function requestYouTubeVideoInfo(url, callback) {
        ensureYouTubeBridge(function(bridge) {
            if (!bridge) {
                callback(null, 'Không kết nối được trình tải. Hãy mở lại tab.');
                return;
            }
            bridge.isAvailable(function(avail) {
                if (!avail) {
                    callback(null, 'Chưa cài yt-dlp. Chạy: pip install yt-dlp');
                    return;
                }
                ytInfoCallbacks[url] = callback;
                bridge.requestVideoInfo(url);
            });
        });
    }

    // ─── "Don't ask again for this site" preference ───────────────────────
    // Stored in localStorage, scoped per-hostname (youtube.com / youtu.be),
    // so re-opening YouTube won't keep popping the modal every time.
    function dontAskKey() {
        return 'cloudarVD_dontAsk_' + window.location.hostname;
    }
    function isDontAskSet() {
        try { return localStorage.getItem(dontAskKey()) === '1'; } catch (e) { return false; }
    }
    function setDontAsk(value) {
        try { localStorage.setItem(dontAskKey(), value ? '1' : '0'); } catch (e) {}
    }

    function ensureYouTubeBridge(callback) {
        if (ytBridge) { callback(ytBridge); return; }
        if (ytBridgeRequestInFlight) {
            // A connection attempt is already underway; wait briefly.
            setTimeout(function() { ensureYouTubeBridge(callback); }, 200);
            return;
        }
        if (typeof qt === 'undefined' || !qt.webChannelTransport || typeof QWebChannel === 'undefined') {
            callback(null);
            return;
        }
        ytBridgeRequestInFlight = true;
        try {
            new QWebChannel(qt.webChannelTransport, function(channel) {
                ytBridgeRequestInFlight = false;
                ytBridge = (channel && channel.objects && channel.objects.youtubeDownloader) || null;
                if (ytBridge && !ytBridgeSignalsBound) {
                    ytBridgeSignalsBound = true;
                    let lastProgressAt = 0;
                    ytBridge.downloadStarted.connect(function() {
                        showNotification('⏳ Đang tải video YouTube...');
                    });
                    ytBridge.downloadProgress.connect(function(url, percent, speed) {
                        const now = Date.now();
                        if (now - lastProgressAt < 1000) return;
                        lastProgressAt = now;
                        const pct = Math.round(percent || 0);
                        showNotification('⏳ Đang tải... ' + pct + '%' + (speed ? ' — ' + speed : ''));
                    });
                    ytBridge.videoInfoReady.connect(function(url, jsonInfo) {
                        const handler = ytInfoCallbacks[url];
                        if (handler) {
                            delete ytInfoCallbacks[url];
                            try {
                                handler(JSON.parse(jsonInfo), null);
                            } catch (e) {
                                handler(null, 'Không đọc được thông tin video');
                            }
                        }
                    });
                    ytBridge.videoInfoFailed.connect(function(url, error) {
                        const handler = ytInfoCallbacks[url];
                        if (handler) {
                            delete ytInfoCallbacks[url];
                            handler(null, error);
                        }
                    });
                    ytBridge.downloadFinished.connect(function(url, success, message) {
                        if (success) {
                            showNotification('✅ Đã lưu tại: ' + message, 8000);
                            const toastEl = document.getElementById('video-downloader-toast');
                            if (toastEl) {
                                toastEl.style.cursor = 'pointer';
                                toastEl.title = 'Bấm để copy đường dẫn';
                                toastEl.onclick = function() {
                                    if (navigator.clipboard && navigator.clipboard.writeText) {
                                        navigator.clipboard.writeText(message).catch(function() {});
                                    }
                                };
                            }
                        } else if (message === 'Đã hủy tải xuống') {
                            showNotification('⏹️ Đã hủy tải xuống', 3000);
                        } else {
                            showNotification('❌ Tải thất bại: ' + (message || 'lỗi không rõ'), 6000);
                        }
                    });
                }
                callback(ytBridge);
            });
        } catch (e) {
            ytBridgeRequestInFlight = false;
            callback(null);
        }
    }

    function downloadYouTubeVideo(video, quality, container) {
        ensureYouTubeBridge(function(bridge) {
            if (!bridge) {
                showNotification('⚠️ Không kết nối được trình tải. Hãy mở lại tab.');
                return;
            }
            bridge.isAvailable(function(avail) {
                if (!avail) {
                    showNotification('⚠️ Chưa cài yt-dlp. Chạy: pip install yt-dlp');
                    return;
                }
                bridge.downloadVideo(video.url, video.name, quality || '', container || '');
            });
        });
    }

    // ─── Modal UI creation ───────────────────────────────────────────────
    let modalActive = false;

    function formatDuration(seconds) {
        seconds = Math.round(seconds || 0);
        if (!seconds) return '';
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds % 60;
        const pad = n => String(n).padStart(2, '0');
        return h > 0 ? (h + ':' + pad(m) + ':' + pad(s)) : (m + ':' + pad(s));
    }

    // Dedicated popup for YouTube videos: shows thumbnail, title, and a
    // quality picker (360p/720p/1080p/Audio only) with estimated file
    // sizes, plus a "don't ask again for this site" checkbox.
    function createYouTubeModal(video) {
        if (modalActive) return;
        modalActive = true;

        const overlay = document.createElement('div');
        overlay.id = 'video-downloader-overlay';
        overlay.style.cssText = `
            position: fixed; top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0,0,0,0.6); z-index: 999999;
            display: flex; align-items: center; justify-content: center;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        `;

        const modal = document.createElement('div');
        modal.style.cssText = `
            background: #1e1e2e; border-radius: 12px; padding: 20px;
            max-width: 420px; width: 90%; max-height: 85vh; overflow-y: auto;
            box-shadow: 0 8px 32px rgba(0,0,0,0.4); color: #cdd6f4;
        `;

        const closeBtn = document.createElement('button');
        closeBtn.textContent = '\u00D7';
        closeBtn.style.cssText = `
            float: right; background: none; border: none; color: #888;
            font-size: 24px; cursor: pointer; padding: 0; line-height: 1;
        `;
        closeBtn.onclick = closeModal;
        modal.appendChild(closeBtn);

        const title = document.createElement('h2');
        title.textContent = '📥 Video Detected';
        title.style.cssText = 'margin: 0 0 12px 0; font-size: 18px; font-weight: 600; color: #fff;';
        modal.appendChild(title);

        // Thumbnail (placeholder until info arrives)
        const thumbWrap = document.createElement('div');
        thumbWrap.style.cssText = `
            width: 100%; aspect-ratio: 16/9; border-radius: 8px; overflow: hidden;
            background: #181825; display: flex; align-items: center; justify-content: center;
            margin-bottom: 10px; position: relative;
        `;
        const thumbImg = document.createElement('img');
        thumbImg.style.cssText = 'width: 100%; height: 100%; object-fit: cover; display: none;';
        const thumbPlaceholder = document.createElement('div');
        thumbPlaceholder.textContent = '⏳';
        thumbPlaceholder.style.cssText = 'font-size: 28px; color: #6c7086;';
        thumbWrap.appendChild(thumbImg);
        thumbWrap.appendChild(thumbPlaceholder);
        modal.appendChild(thumbWrap);

        const videoTitle = document.createElement('div');
        videoTitle.textContent = video.name;
        videoTitle.style.cssText = `
            font-size: 14px; font-weight: 500; color: #fff; margin-bottom: 4px;
            overflow: hidden; text-overflow: ellipsis; display: -webkit-box;
            -webkit-line-clamp: 2; -webkit-box-orient: vertical;
        `;
        modal.appendChild(videoTitle);

        const metaLine = document.createElement('div');
        metaLine.textContent = 'Đang tải thông tin video...';
        metaLine.style.cssText = 'font-size: 12px; color: #a6adc8; margin-bottom: 10px;';
        modal.appendChild(metaLine);

        // Output container selector (mp4 / webm / mkv / native).
        // Persisted in localStorage so the choice sticks between videos.
        const containerRow = document.createElement('div');
        containerRow.style.cssText = 'display: flex; align-items: center; gap: 8px; margin-bottom: 12px;';
        const containerLbl = document.createElement('span');
        containerLbl.textContent = 'Định dạng:';
        containerLbl.style.cssText = 'font-size: 12px; color: #a6adc8;';
        containerRow.appendChild(containerLbl);

        const containerSelect = document.createElement('select');
        containerSelect.style.cssText = `
            flex: 1; background: #181825; color: #cdd6f4; border: 1px solid #313244;
            border-radius: 6px; padding: 5px 8px; font-size: 12px; cursor: pointer;
        `;
        [
            { value: 'mp4', label: 'MP4 (tương thích tốt nhất)' },
            { value: 'webm', label: 'WebM' },
            { value: 'mkv', label: 'MKV' },
            { value: '', label: 'Giữ định dạng gốc (không ép đổi)' },
        ].forEach(function(opt) {
            const o = document.createElement('option');
            o.value = opt.value;
            o.textContent = opt.label;
            containerSelect.appendChild(o);
        });
        try {
            containerSelect.value = localStorage.getItem('cloudarVD_container') || 'mp4';
        } catch (e) { containerSelect.value = 'mp4'; }
        containerSelect.onchange = function() {
            try { localStorage.setItem('cloudarVD_container', this.value); } catch (e) {}
        };
        containerRow.appendChild(containerSelect);
        modal.appendChild(containerRow);

        const containerNote = document.createElement('div');
        containerNote.textContent = 'Lưu ý: nếu định dạng ép không tương thích với luồng gốc, trình tải sẽ tự lùi về định dạng gốc.';
        containerNote.style.cssText = 'font-size: 10.5px; color: #6c7086; margin: -6px 0 10px 0;';
        modal.appendChild(containerNote);

        // Quality list — populated once video info arrives
        const qualityList = document.createElement('div');
        qualityList.style.cssText = 'display: flex; flex-direction: column; gap: 8px; margin-bottom: 14px;';
        modal.appendChild(qualityList);

        function renderQualityButton(opt) {
            const btn = document.createElement('button');
            btn.disabled = !opt.available;
            btn.style.cssText = `
                display: flex; align-items: center; justify-content: space-between;
                width: 100%; padding: 10px 14px; border-radius: 8px; border: 1px solid #313244;
                background: ${opt.available ? '#181825' : '#15151f'};
                color: ${opt.available ? '#fff' : '#585b70'};
                font-size: 13px; cursor: ${opt.available ? 'pointer' : 'not-allowed'};
                transition: background 0.15s;
            `;
            if (opt.available) {
                btn.onmouseover = function() { this.style.background = '#313244'; };
                btn.onmouseout = function() { this.style.background = '#181825'; };
            }

            const left = document.createElement('span');
            left.textContent = (opt.key === 'audio' ? '🎵 ' : '▶️ ') + opt.label;
            btn.appendChild(left);

            const right = document.createElement('span');
            right.textContent = opt.available ? (opt.size_label || 'Ước tính...') : 'Không có';
            right.style.cssText = 'font-size: 12px; color: #a6adc8;';
            btn.appendChild(right);

            if (opt.available) {
                btn.onclick = function() {
                    const container = opt.key === 'audio' ? '' : containerSelect.value;
                    downloadYouTubeVideo(video, opt.key, container);
                    closeModal();
                };
            }
            qualityList.appendChild(btn);
        }

        // Fallback quality buttons (best-effort, no size yet) shown immediately
        // so the user isn't staring at a blank modal while we fetch info.
        const fallbackTiers = [
            { key: '1080p', label: '1080p', available: true, size_label: null },
            { key: '720p', label: '720p', available: true, size_label: null },
            { key: '360p', label: '360p', available: true, size_label: null },
            { key: 'audio', label: 'Audio only', available: true, size_label: null },
        ];
        fallbackTiers.forEach(renderQualityButton);

        // Footer: "don't ask again" + Not Now
        const footer = document.createElement('div');
        footer.style.cssText = 'display: flex; align-items: center; justify-content: space-between; gap: 8px; padding-top: 10px; border-top: 1px solid #313244;';

        const dontAskLabel = document.createElement('label');
        dontAskLabel.style.cssText = 'display: flex; align-items: center; gap: 6px; font-size: 12px; color: #a6adc8; cursor: pointer;';
        const dontAskCheckbox = document.createElement('input');
        dontAskCheckbox.type = 'checkbox';
        dontAskCheckbox.checked = isDontAskSet();
        dontAskCheckbox.style.cssText = 'cursor: pointer;';
        dontAskCheckbox.onchange = function() { setDontAsk(this.checked); };
        dontAskLabel.appendChild(dontAskCheckbox);
        dontAskLabel.appendChild(document.createTextNode("Đừng hỏi lại cho trang này"));
        footer.appendChild(dontAskLabel);

        const skipBtn = document.createElement('button');
        skipBtn.textContent = 'Not Now';
        skipBtn.style.cssText = `
            padding: 7px 14px; background: transparent; border: 1px solid #45475a;
            border-radius: 8px; color: #a6adc8; font-size: 13px; cursor: pointer;
            flex-shrink: 0;
        `;
        skipBtn.onmouseover = function() { this.style.background = '#313244'; };
        skipBtn.onmouseout = function() { this.style.background = 'transparent'; };
        skipBtn.onclick = closeModal;
        footer.appendChild(skipBtn);

        modal.appendChild(footer);
        overlay.appendChild(modal);
        document.body.appendChild(overlay);

        function closeModal() {
            modalActive = false;
            if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
        }

        // Fetch real info (thumbnail, duration, per-quality sizes) and
        // refresh the modal in place once it arrives.
        requestYouTubeVideoInfo(video.url, function(info, error) {
            if (!overlay.parentNode) return; // modal already closed/dismissed
            if (error || !info) {
                metaLine.textContent = 'Không lấy được thông tin chi tiết (vẫn có thể tải).';
                return;
            }
            if (info.thumbnail) {
                thumbImg.src = info.thumbnail;
                thumbImg.onload = function() {
                    thumbImg.style.display = 'block';
                    thumbPlaceholder.style.display = 'none';
                };
            }
            if (info.title) videoTitle.textContent = info.title;
            const parts = [];
            if (info.uploader) parts.push(info.uploader);
            if (info.duration) parts.push(formatDuration(info.duration));
            metaLine.textContent = parts.length ? parts.join(' • ') : '';

            if (Array.isArray(info.qualities) && info.qualities.length) {
                while (qualityList.firstChild) qualityList.removeChild(qualityList.firstChild);
                info.qualities.forEach(renderQualityButton);
            }
        });
    }

    // Small, unobtrusive floating button shown instead of the auto popup
    // once the user has checked "don't ask again for this site" — lets
    // them still open the download picker on demand.
    function createFloatingYouTubeButton(video) {
        const existing = document.getElementById('video-downloader-fab');
        if (existing) existing.remove();

        const fab = document.createElement('button');
        fab.id = 'video-downloader-fab';
        fab.title = 'Tải video này';
        fab.textContent = '⬇';
        fab.style.cssText = `
            position: fixed; bottom: 24px; right: 24px; width: 44px; height: 44px;
            border-radius: 50%; background: #5865f2; color: white; border: none;
            font-size: 18px; cursor: pointer; z-index: 999998;
            box-shadow: 0 4px 12px rgba(0,0,0,0.35);
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        `;
        fab.onmouseover = function() { this.style.background = '#4752c4'; };
        fab.onmouseout = function() { this.style.background = '#5865f2'; };
        fab.onclick = function() {
            fab.remove();
            createYouTubeModal(video);
        };
        document.body.appendChild(fab);
    }

    // On-demand "media on this page" button for ordinary websites (not
    // YouTube, which has its own dedicated flow above). Only appears if a
    // fresh scan finds at least one video or sizeable image, and re-scans
    // fresh at click time in case more content loaded since.
    let mediaFabShown = false;
    function maybeShowMediaFab() {
        if (isYouTubeSite() || mediaFabShown) return;
        const media = getMediaSources();
        if (media.length === 0) return;
        mediaFabShown = true;

        const fab = document.createElement('button');
        fab.id = 'media-downloader-fab';
        fab.title = 'Tải ảnh/video trên trang này';
        fab.textContent = '🖼️';
        fab.style.cssText = `
            position: fixed; bottom: 24px; right: 24px; width: 44px; height: 44px;
            border-radius: 50%; background: #5865f2; color: white; border: none;
            font-size: 18px; cursor: pointer; z-index: 999998;
            box-shadow: 0 4px 12px rgba(0,0,0,0.35);
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        `;
        fab.onmouseover = function() { this.style.background = '#4752c4'; };
        fab.onmouseout = function() { this.style.background = '#5865f2'; };
        fab.onclick = function() {
            const fresh = getMediaSources();
            if (fresh.length === 0) {
                showNotification('Không tìm thấy ảnh hoặc video nào trên trang này.');
                return;
            }
            createModal(fresh);
        };
        document.body.appendChild(fab);
    }

    function createModal(videos) {
        if (modalActive) return;
        modalActive = true;

        // Overlay
        const overlay = document.createElement('div');
        overlay.id = 'video-downloader-overlay';
        overlay.style.cssText = `
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0,0,0,0.6);
            z-index: 999999;
            display: flex;
            align-items: center;
            justify-content: center;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        `;

        // Modal box
        const modal = document.createElement('div');
        modal.style.cssText = `
            background: #1e1e2e;
            border-radius: 12px;
            padding: 24px;
            max-width: 520px;
            width: 90%;
            max-height: 80vh;
            overflow-y: auto;
            box-shadow: 0 8px 32px rgba(0,0,0,0.4);
            color: #cdd6f4;
        `;

        // Close button
        const closeBtn = document.createElement('button');
        closeBtn.textContent = '\u00D7'; // "×" — avoid innerHTML: sites with Trusted Types CSP (e.g. YouTube) block innerHTML assignment
        closeBtn.style.cssText = `
            float: right;
            background: none;
            border: none;
            color: #888;
            font-size: 24px;
            cursor: pointer;
            padding: 0;
            line-height: 1;
        `;
        closeBtn.onclick = closeModal;
        modal.appendChild(closeBtn);

        // Title
        const title = document.createElement('h2');
        const hasImages = videos.some(v => v.type === 'image');
        const hasVideos = videos.some(v => v.type !== 'image');
        title.textContent = hasImages && hasVideos ? '📥 Media Detected'
            : hasImages ? '🖼️ Images Detected' : '📥 Video Detected';
        title.style.cssText = `
            margin: 0 0 8px 0;
            font-size: 20px;
            font-weight: 600;
            color: #fff;
        `;
        modal.appendChild(title);

        const subtitle = document.createElement('p');
        subtitle.textContent = videos.length > 1
            ? `This page has ${videos.length} downloadable items. Download them?`
            : 'This page has downloadable media. Download it?';
        subtitle.style.cssText = `
            margin: 0 0 16px 0;
            font-size: 14px;
            color: #a6adc8;
        `;
        modal.appendChild(subtitle);

        // ── Filters (only shown when the list actually contains images) ──
        const TYPE_LABELS = { jpg: 'JPG', png: 'PNG', gif: 'GIF', webp: 'WebP', svg: 'SVG', other: 'Khác' };
        const presentExts = new Set();
        videos.forEach(function(v) {
            if (v.type !== 'image') return;
            presentExts.add(TYPE_LABELS.hasOwnProperty(v.ext) ? v.ext : 'other');
        });

        function isTypeEnabled(ext) {
            try {
                const v = localStorage.getItem('cloudarVD_typeFilter_' + ext);
                return v === null ? true : v === '1';
            } catch (e) { return true; }
        }
        function setTypeEnabled(ext, val) {
            try { localStorage.setItem('cloudarVD_typeFilter_' + ext, val ? '1' : '0'); } catch (e) {}
        }
        function isIncludeThumbnails() {
            try { return localStorage.getItem('cloudarVD_includeThumbs') === '1'; } catch (e) { return false; }
        }
        function setIncludeThumbnails(val) {
            try { localStorage.setItem('cloudarVD_includeThumbs', val ? '1' : '0'); } catch (e) {}
        }

        if (presentExts.size > 0) {
            const filterBox = document.createElement('div');
            filterBox.style.cssText = 'margin-bottom: 12px; padding: 10px 12px; background: #181825; border-radius: 8px; border: 1px solid #313244;';

            const filterLabel = document.createElement('div');
            filterLabel.textContent = 'Lọc theo định dạng:';
            filterLabel.style.cssText = 'font-size: 11px; color: #a6adc8; margin-bottom: 6px;';
            filterBox.appendChild(filterLabel);

            const chipsRow = document.createElement('div');
            chipsRow.style.cssText = 'display: flex; flex-wrap: wrap; gap: 12px; margin-bottom: 10px;';
            ['jpg', 'png', 'gif', 'webp', 'svg', 'other'].forEach(function(ext) {
                if (!presentExts.has(ext)) return;
                const lbl = document.createElement('label');
                lbl.style.cssText = 'display: flex; align-items: center; gap: 4px; font-size: 12px; color: #cdd6f4; cursor: pointer;';
                const cb = document.createElement('input');
                cb.type = 'checkbox';
                cb.checked = isTypeEnabled(ext);
                cb.style.cssText = 'cursor: pointer;';
                cb.onchange = function() { setTypeEnabled(ext, this.checked); applyFilters(); };
                lbl.appendChild(cb);
                lbl.appendChild(document.createTextNode(TYPE_LABELS[ext]));
                chipsRow.appendChild(lbl);
            });
            filterBox.appendChild(chipsRow);

            const thumbLbl = document.createElement('label');
            thumbLbl.style.cssText = 'display: flex; align-items: center; gap: 6px; font-size: 12px; color: #a6adc8; cursor: pointer;';
            const thumbCb = document.createElement('input');
            thumbCb.type = 'checkbox';
            thumbCb.checked = isIncludeThumbnails();
            thumbCb.style.cssText = 'cursor: pointer;';
            thumbCb.onchange = function() { setIncludeThumbnails(this.checked); applyFilters(); };
            thumbLbl.appendChild(thumbCb);
            thumbLbl.appendChild(document.createTextNode('Bao gồm ảnh nhỏ / thumbnail (mặc định chỉ hiện ảnh gốc)'));
            filterBox.appendChild(thumbLbl);

            modal.appendChild(filterBox);
        }

        // ── Select-all + selected count ──────────────────────────────────
        const selectHeader = document.createElement('div');
        selectHeader.style.cssText = 'display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;';

        const selectAllLbl = document.createElement('label');
        selectAllLbl.style.cssText = 'display: flex; align-items: center; gap: 6px; font-size: 12px; color: #a6adc8; cursor: pointer;';
        const selectAllCb = document.createElement('input');
        selectAllCb.type = 'checkbox';
        selectAllCb.checked = true;
        selectAllCb.style.cssText = 'cursor: pointer;';
        selectAllCb.onchange = function() {
            list.querySelectorAll('.cloudar-item-row').forEach(function(row) {
                if (row.dataset.hidden === '1') return;
                row.querySelector('.cloudar-item-cb').checked = selectAllCb.checked;
            });
            updateSelectedCount();
        };
        selectAllLbl.appendChild(selectAllCb);
        selectAllLbl.appendChild(document.createTextNode('Chọn tất cả'));
        selectHeader.appendChild(selectAllLbl);

        const countLbl = document.createElement('div');
        countLbl.style.cssText = 'font-size: 12px; color: #a6adc8;';
        selectHeader.appendChild(countLbl);
        modal.appendChild(selectHeader);

        // Media list
        const list = document.createElement('div');
        list.style.cssText = `
            display: flex;
            flex-direction: column;
            gap: 8px;
        `;

        videos.forEach(function(video, index) {
            const item = document.createElement('div');
            item.className = 'cloudar-item-row';
            item.dataset.index = String(index);
            if (video.type === 'image') {
                item.dataset.ext = TYPE_LABELS.hasOwnProperty(video.ext) ? video.ext : 'other';
                item.dataset.thumb = video.isThumbnail ? '1' : '0';
            }
            item.style.cssText = `
                display: flex;
                align-items: center;
                justify-content: space-between;
                padding: 10px 12px;
                background: #181825;
                border-radius: 8px;
                border: 1px solid #313244;
            `;

            const itemCb = document.createElement('input');
            itemCb.type = 'checkbox';
            itemCb.className = 'cloudar-item-cb';
            itemCb.checked = true;
            itemCb.style.cssText = 'margin-right: 10px; flex-shrink: 0; cursor: pointer;';
            itemCb.onchange = updateSelectedCount;
            item.appendChild(itemCb);

            // Small thumbnail preview for images (cheap: reuses the
            // already-loaded <img> src, no extra network request).
            if (video.type === 'image' && video.previewSrc) {
                const thumb = document.createElement('img');
                thumb.src = video.previewSrc;
                thumb.style.cssText = `
                    width: 40px; height: 40px; object-fit: cover;
                    border-radius: 6px; margin-right: 10px; flex-shrink: 0;
                    background: #11111b;
                `;
                item.appendChild(thumb);
            }

            // Media info
            const info = document.createElement('div');
            info.style.cssText = `
                flex: 1;
                min-width: 0;
                margin-right: 8px;
            `;

            const name = document.createElement('div');
            name.textContent = video.name;
            name.style.cssText = `
                font-size: 13px;
                font-weight: 500;
                color: #fff;
                overflow: hidden;
                text-overflow: ellipsis;
                white-space: nowrap;
            `;
            info.appendChild(name);

            const details = document.createElement('div');
            details.style.cssText = `
                font-size: 11px;
                color: #a6adc8;
                margin-top: 2px;
            `;
            if (video.type === 'image') {
                // "1920×1080 • PNG • " then a size span updated once the
                // async HEAD/range lookup resolves — shown progressively
                // rather than blocking the whole list on every request.
                const dimsText = video.width && video.height
                    ? (video.dimEstimated ? '~' : '') + video.width + '\u00D7' + video.height
                    : '';
                const extText = (video.ext || '').toUpperCase();
                details.textContent = ['🖼️ ' + (dimsText || 'Ảnh'), extText].filter(Boolean).join(' • ') + ' • ';
                const sizeSpan = document.createElement('span');
                sizeSpan.textContent = 'đang tính dung lượng…';
                details.appendChild(sizeSpan);
                fetchFileSizeLabel(video.url, function(label) {
                    sizeSpan.textContent = label || 'không rõ dung lượng';
                });
            } else {
                details.textContent = video.type === 'youtube' ? '▶️ YouTube (tải qua yt-dlp)' :
                    (video.type === 'embed' ? '🔗 Embedded Video' : '🎬 Video File');
            }
            info.appendChild(details);

            item.appendChild(info);

            // Download button
            const dlBtn = document.createElement('button');
            dlBtn.textContent = '⬇';
            dlBtn.title = 'Download ' + video.name;
            dlBtn.style.cssText = `
                background: #313244;
                border: none;
                border-radius: 6px;
                color: #cdd6f4;
                font-size: 16px;
                cursor: pointer;
                padding: 6px 10px;
                flex-shrink: 0;
                transition: background 0.2s;
            `;
            dlBtn.onmouseover = function() { this.style.background = '#45475a'; };
            dlBtn.onmouseout = function() { this.style.background = '#313244'; };
            dlBtn.onclick = function(e) {
                e.stopPropagation();
                triggerDownload(video);
                closeModal();
            };
            item.appendChild(dlBtn);

            list.appendChild(item);
        });

        modal.appendChild(list);

        function applyFilters() {
            list.querySelectorAll('.cloudar-item-row').forEach(function(row) {
                let visible = true;
                if (row.dataset.ext !== undefined) {
                    if (!isTypeEnabled(row.dataset.ext)) visible = false;
                    if (visible && row.dataset.thumb === '1' && !isIncludeThumbnails()) visible = false;
                }
                row.style.display = visible ? 'flex' : 'none';
                row.dataset.hidden = visible ? '0' : '1';
            });
            updateSelectedCount();
        }

        function updateSelectedCount() {
            const rows = Array.from(list.querySelectorAll('.cloudar-item-row')).filter(function(r) { return r.dataset.hidden !== '1'; });
            const checked = rows.filter(function(r) { return r.querySelector('.cloudar-item-cb').checked; });
            countLbl.textContent = checked.length + ' / ' + rows.length + ' đã chọn';
            downloadSelectedBtn.disabled = checked.length === 0;
            downloadSelectedBtn.style.opacity = checked.length === 0 ? '0.5' : '1';
            downloadSelectedBtn.style.cursor = checked.length === 0 ? 'not-allowed' : 'pointer';
            selectAllCb.checked = rows.length > 0 && checked.length === rows.length;
        }

        // Footer buttons
        const footer = document.createElement('div');
        footer.style.cssText = `
            display: flex;
            justify-content: flex-end;
            gap: 8px;
            margin-top: 16px;
            padding-top: 12px;
            border-top: 1px solid #313244;
        `;

        const skipBtn = document.createElement('button');
        skipBtn.textContent = 'Not Now';
        skipBtn.style.cssText = `
            padding: 8px 16px;
            background: transparent;
            border: 1px solid #45475a;
            border-radius: 8px;
            color: #a6adc8;
            font-size: 13px;
            cursor: pointer;
            transition: background 0.2s;
        `;
        skipBtn.onmouseover = function() { this.style.background = '#313244'; };
        skipBtn.onmouseout = function() { this.style.background = 'transparent'; };
        skipBtn.onclick = closeModal;
        footer.appendChild(skipBtn);

        const downloadSelectedBtn = document.createElement('button');
        downloadSelectedBtn.textContent = '⬇ Download Selected';
        downloadSelectedBtn.style.cssText = `
            padding: 8px 16px;
            background: #5865f2;
            border: none;
            border-radius: 8px;
            color: white;
            font-size: 13px;
            font-weight: 500;
            cursor: pointer;
            transition: background 0.2s;
        `;
        downloadSelectedBtn.onmouseover = function() { this.style.background = '#4752c4'; };
        downloadSelectedBtn.onmouseout = function() { this.style.background = '#5865f2'; };
        downloadSelectedBtn.onclick = function() {
            if (downloadSelectedBtn.disabled) return;
            list.querySelectorAll('.cloudar-item-row').forEach(function(row) {
                if (row.dataset.hidden === '1') return;
                if (!row.querySelector('.cloudar-item-cb').checked) return;
                triggerDownload(videos[parseInt(row.dataset.index, 10)]);
            });
            closeModal();
        };
        footer.appendChild(downloadSelectedBtn);

        modal.appendChild(footer);
        overlay.appendChild(modal);
        document.body.appendChild(overlay);

        // Apply persisted filter preferences (e.g. "hide thumbnails" from a
        // previous visit) and compute the initial selected count.
        applyFilters();

        function closeModal() {
            modalActive = false;
            if (overlay.parentNode) {
                overlay.parentNode.removeChild(overlay);
            }
        }
    }

    // ─── Download function ───────────────────────────────────────────────
    function triggerDownload(video) {
        if (video.type === 'youtube') {
            downloadYouTubeVideo(video);
            return;
        }

        const url = resolveUrl(video.url);

        if (video.type === 'embed') {
            // For embedded videos, we can't directly download
            // but we can open them in a new tab or notify the user
            showNotification('⏳ Opening embedded video page...');
            window.open(url, '_blank');
            return;
        }

        if (video.type === 'image') {
            triggerImageDownload(video, url);
            return;
        }

        // Create an anchor element to trigger download
        const a = document.createElement('a');
        a.href = url;
        a.download = video.name;
        a.style.display = 'none';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);

        showNotification('✅ Download started: ' + video.name);
    }

    // Images are usually served cross-origin from a CDN, where a plain
    // `<a download>` click is silently ignored by the browser (it just
    // navigates/opens the image instead of saving it). Fetching the bytes
    // ourselves and saving via a blob: URL works regardless of origin,
    // as long as the server's CORS headers allow it; if they don't, fall
    // back to opening the image in a new tab so the user can still save
    // it manually (right-click → Save Image As).
    function triggerImageDownload(image, url) {
        showNotification('⏳ Đang tải ảnh...');
        fetch(url, { mode: 'cors', credentials: 'omit' })
            .then(function(resp) {
                if (!resp.ok) throw new Error('HTTP ' + resp.status);
                return resp.blob();
            })
            .then(function(blob) {
                const objectUrl = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = objectUrl;
                a.download = image.name;
                a.style.display = 'none';
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                setTimeout(function() { URL.revokeObjectURL(objectUrl); }, 4000);
                showNotification('✅ Đã tải ảnh: ' + image.name);
            })
            .catch(function() {
                showNotification('⚠️ Không tải trực tiếp được (CORS). Đang mở ảnh ở tab mới — nhấn phải chuột > Save Image As.');
                window.open(url, '_blank');
            });
    }

    // ─── Toast notification ──────────────────────────────────────────────
    function showNotification(message, durationMs) {
        durationMs = durationMs || 3000;
        const existing = document.getElementById('video-downloader-toast');
        if (existing) existing.remove();

        const toast = document.createElement('div');
        toast.id = 'video-downloader-toast';
        toast.textContent = message;
        toast.style.cssText = `
            position: fixed;
            bottom: 24px;
            right: 24px;
            max-width: 420px;
            word-break: break-all;
            background: #1e1e2e;
            color: #cdd6f4;
            padding: 12px 20px;
            border-radius: 8px;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            font-size: 14px;
            z-index: 1000000;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            border: 1px solid #313244;
            animation: slideIn 0.3s ease;
        `;

        // Add animation
        const style = document.createElement('style');
        style.id = 'video-downloader-style';
        if (!document.getElementById('video-downloader-style')) {
            style.textContent = `
                @keyframes slideIn {
                    from { transform: translateX(100px); opacity: 0; }
                    to { transform: translateX(0); opacity: 1; }
                }
                @keyframes fadeOut {
                    from { opacity: 1; }
                    to { opacity: 0; }
                }
            `;
            document.head.appendChild(style);
        }

        document.body.appendChild(toast);
        setTimeout(function() {
            toast.style.animation = 'fadeOut 0.3s ease';
            setTimeout(function() {
                if (toast.parentNode) toast.parentNode.removeChild(toast);
            }, 300);
        }, durationMs);
    }

    // ─── Main detection logic ────────────────────────────────────────────
    let lastYouTubeVideoId = null;

    function checkForVideos() {
        // Don't show on internal browser pages
        if (window.location.protocol === 'cloudar:' || 
            window.location.protocol === 'about:' ||
            window.location.hostname === 'localhost' ||
            window.location.hostname === '127.0.0.1') {
            return;
        }

        if (isYouTubeWatchUrl()) {
            const vid = getYouTubeVideoId();
            if (!vid || vid === lastYouTubeVideoId) return;
            lastYouTubeVideoId = vid;
            const watchUrl = 'https://www.youtube.com/watch?v=' + encodeURIComponent(vid);
            const video = {
                url: watchUrl,
                name: getYouTubeTitle(),
                type: 'youtube',
                mime: ''
            };
            setTimeout(function() {
                if (isDontAskSet()) {
                    // User opted out of the auto popup for this site — show
                    // a small, unobtrusive button instead so they can still
                    // open the download popup whenever they want.
                    createFloatingYouTubeButton(video);
                } else {
                    createYouTubeModal(video);
                }
            }, 900);
            return;
        }

        const videos = getVideoSources();
        if (videos.length > 0) {
            // Delay slightly to let page fully render, then build the full
            // media list (video + any sizeable images) for the popup so a
            // video download also surfaces nearby images as a bonus.
            setTimeout(function() {
                createModal(getMediaSources());
            }, 500);
        }

        // Whether or not a video was found, offer a small on-demand button
        // for "media on this page" (mainly images) — opt-in, so photo-heavy
        // pages don't get an unsolicited popup on every load.
        setTimeout(maybeShowMediaFab, 700);
    }

    // ─── YouTube SPA navigation watcher ───────────────────────────────────
    // YouTube swaps videos via history.pushState without a full page load,
    // so the normal 'load'/DOMContentLoaded checks only fire once. Re-check
    // whenever YouTube's own navigation event fires, with a URL-polling
    // fallback in case that event isn't dispatched.
    function watchYouTubeNavigation() {
        if (!isYouTubeSite()) return;
        let lastHref = window.location.href;

        function resetForNavigation() {
            modalActive = false;
            const fab = document.getElementById('video-downloader-fab');
            if (fab) fab.remove();
        }

        window.addEventListener('yt-navigate-finish', function() {
            resetForNavigation();
            setTimeout(checkForVideos, 600);
        });

        setInterval(function() {
            if (window.location.href !== lastHref) {
                lastHref = window.location.href;
                resetForNavigation();
                setTimeout(checkForVideos, 800);
            }
        }, 1000);
    }

    // ─── MutationObserver for dynamically loaded videos ──────────────────
    let initialCheckDone = false;
    let dynamicCheckTimer = null;

    function onPageReady() {
        if (initialCheckDone) return;
        initialCheckDone = true;
        checkForVideos();
    }

    // Check when document is ready
    if (document.readyState === 'complete') {
        onPageReady();
    } else {
        window.addEventListener('load', onPageReady);
        // Also check on DOMContentLoaded in case load already fired
        document.addEventListener('DOMContentLoaded', function() {
            // Wait a bit for dynamic content
            setTimeout(onPageReady, 1500);
        });
    }

    // Watch for dynamically added video elements
    const observer = new MutationObserver(function(mutations) {
        if (initialCheckDone) return;
        for (let m of mutations) {
            for (let node of m.addedNodes) {
                if (node.nodeType === 1) {
                    if (node.tagName === 'VIDEO' || node.tagName === 'SOURCE' || 
                        (node.tagName === 'IFRAME' && node.src && 
                         (node.src.includes('youtube') || node.src.includes('vimeo')))) {
                        clearTimeout(dynamicCheckTimer);
                        dynamicCheckTimer = setTimeout(checkForVideos, 3000);
                        return;
                    }
                    if (node.querySelectorAll) {
                        const hasVideo = node.querySelectorAll('video, source[src]').length > 0;
                        if (hasVideo) {
                            clearTimeout(dynamicCheckTimer);
                            dynamicCheckTimer = setTimeout(checkForVideos, 3000);
                            return;
                        }
                    }
                }
            }
        }
    });

    observer.observe(document.body || document.documentElement, {
        childList: true,
        subtree: true
    });

    watchYouTubeNavigation();

})();