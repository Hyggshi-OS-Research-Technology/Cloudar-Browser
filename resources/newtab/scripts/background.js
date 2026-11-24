/* ==========================================================================
   background.js — the wallpaper layer (solid colour, image, live video).

   Three storage problems in the version this replaces:

   0. **Nothing survived a restart.** A cloudar:// page's localStorage and
      IndexedDB live in the renderer's storage partition, and this build clears
      browsing data on exit unless the user opted in to persistent cookies — so
      "upload a wallpaper, reopen the browser, it is gone" was not a bug in this
      file, it was the storage being thrown away underneath it. The bytes now go
      to the browser through newTabBridge.saveWallpaperMedia(), which writes them
      under AppDataLocation (QSettings-tied, not browsing-data-tied) and the page
      gets them back at setup() from initialState().wallpaper. IndexedDB is still
      used as a fast path while the tab is open, and as the store for media the
      browser refused (quota), so a failure is reported rather than silent.

   1. **Images were data: URLs in localStorage.** A 1920x1080 JPEG re-encoded at
      quality 0.85 is roughly 300-600 KB, and base64 makes it ~33% larger.
      localStorage is a ~5 MB string store shared by every cloudar:// page, so
      two photo wallpapers would fill it and start throwing QuotaExceededError —
      which the old code swallowed, leaving the user with a wallpaper that
      silently reverted on restart. Images now go to IndexedDB as Blobs, exactly
      like videos already did. Only the small descriptor (type + source) stays in
      localStorage.
   2. **Object URLs were created and never revoked.** Every preview assignment
      called URL.createObjectURL() and left the previous one alive, so switching
      wallpapers five times pinned five decoded videos in memory for the life of
      the tab. All object URLs now go through one small registry (`urls`) that
      revokes the previous handle whenever it is replaced, on teardown, and on
      pagehide/visibilitychange.

   Everything here is deliberately Blob-based rather than data-URL-based so the
   same code path serves images and videos.
   ========================================================================== */

var CloudarBackground = (function () {
  'use strict';

  var SETTING_KEY = 'cloudar.newtab.background';
  var DB_NAME = 'cloudar_wallpaper_db';
  var DB_VERSION = 2;           // v1 had only the videos store
  var STORE_VIDEO = 'videos';
  var STORE_IMAGE = 'images';
  var VIDEO_ID = 'bg_video';
  var IMAGE_ID = 'bg_image';
  var MAX_IMAGE_DIM = 2560;     // full-screen on a 1440p display, still modest
  var JPEG_QUALITY = 0.85;

  var body = null;
  var bgVideo = null;
  var videoPreview = null;
  var imagePreview = null;
  var colorPicker = null;
  var onStatus = function () {};

  // --- object URL registry ------------------------------------------------
  // One live handle per purpose. `set` returns the new URL and revokes whatever
  // it replaced, which is what the old code was missing.
  var urls = Object.create(null);

  function setObjectUrl(slot, blob) {
    if (urls[slot]) {
      URL.revokeObjectURL(urls[slot]);
      delete urls[slot];
    }
    if (!blob) return '';
    var url = URL.createObjectURL(blob);
    urls[slot] = url;
    return url;
  }

  function releaseObjectUrl(slot) {
    if (!urls[slot]) return;
    URL.revokeObjectURL(urls[slot]);
    delete urls[slot];
  }

  function releaseAll() {
    Object.keys(urls).forEach(function (slot) { releaseObjectUrl(slot); });
  }

  // A tab kept in the background for hours must not hold a decoded video; both
  // events fire when the page stops being visible. Releasing here is safe
  // because applying again re-creates the URL from IndexedDB on demand.
  window.addEventListener('pagehide', releaseAll);
  document.addEventListener('visibilitychange', function () {
    if (document.visibilityState === 'hidden') releaseAll();
  });

  // --- IndexedDB ----------------------------------------------------------
  function openDb(cb) {
    var req = indexedDB.open(DB_NAME, DB_VERSION);
    req.onupgradeneeded = function (e) {
      var db = e.target.result;
      if (!db.objectStoreNames.contains(STORE_VIDEO)) db.createObjectStore(STORE_VIDEO);
      if (!db.objectStoreNames.contains(STORE_IMAGE)) db.createObjectStore(STORE_IMAGE);
    };
    req.onsuccess = function (e) { cb(null, e.target.result); };
    req.onerror = function (e) { cb(e.target.error, null); };
  }

  function withStore(storeName, mode, fn) {
    openDb(function (err, db) {
      if (err) { fn(err, null); return; }
      var tx = db.transaction([storeName], mode);
      var store = tx.objectStore(storeName);
      var req = fn(null, store, tx);
      if (req) {
        req.onerror = function (e) { fn(e.target.error, store, tx); };
      }
      tx.oncomplete = function () { db.close(); };
    });
  }

  function putBlob(storeName, key, blob, cb) {
    openDb(function (err, db) {
      if (err) { if (cb) cb(err); return; }
      var tx = db.transaction([storeName], 'readwrite');
      var req = tx.objectStore(storeName).put(blob, key);
      req.onsuccess = function () { if (cb) cb(null); };
      req.onerror = function (e) { if (cb) cb(e.target.error); };
      tx.oncomplete = function () { db.close(); };
    });
  }

  function getBlob(storeName, key, cb) {
    openDb(function (err, db) {
      if (err) { cb(err, null); return; }
      var tx = db.transaction([storeName], 'readonly');
      var req = tx.objectStore(storeName).get(key);
      req.onsuccess = function (e) { cb(null, e.target.result || null); };
      req.onerror = function (e) { cb(e.target.error, null); };
      tx.oncomplete = function () { db.close(); };
    });
  }

  function deleteBlob(storeName, key, cb) {
    openDb(function (err, db) {
      if (err) { if (cb) cb(err); return; }
      var tx = db.transaction([storeName], 'readwrite');
      tx.objectStore(storeName).delete(key);
      tx.oncomplete = function () { db.close(); if (cb) cb(null); };
      tx.onerror = function (e) { if (cb) cb(e.target.error); };
    });
  }

  function clearMedia(cb) {
    deleteBlob(STORE_VIDEO, VIDEO_ID, function () {
      deleteBlob(STORE_IMAGE, IMAGE_ID, function () { if (cb) cb(null); });
    });
  }

  // --- setting descriptor -------------------------------------------------
  function saveSetting(setting) {
    try {
      localStorage.setItem(SETTING_KEY, JSON.stringify(setting));
    } catch (e) {
      // This store only ever holds type + source for local media, so a quota
      // error here means something else filled localStorage — say so instead of
      // pretending the choice was saved.
      onStatus('Could not save the wallpaper choice (' + (e.name || 'error') +
               '). The page will still apply it until you close the tab.', true);
    }
  }

  function loadSetting() {
    try {
      var raw = localStorage.getItem(SETTING_KEY);
      return raw ? JSON.parse(raw) : null;
    } catch (e) { return null; }
  }

  // --- applying -----------------------------------------------------------
  function stopVideo() {
    if (bgVideo) {
      bgVideo.pause();
      bgVideo.removeAttribute('src');
      bgVideo.load();
    }
    releaseObjectUrl('bg');
    if (body) body.classList.remove('video-bg');
    var controls = document.getElementById('mediaControls');
    if (controls) controls.classList.remove('show');
    if (videoPreview) {
      videoPreview.pause();
      videoPreview.removeAttribute('src');
      videoPreview.load();
    }
    releaseObjectUrl('preview');
    var wrapper = document.getElementById('videoPreviewWrapper');
    if (wrapper) wrapper.classList.remove('show');
  }

  function applyVideo(src, audioEnabled) {
    if (!body || !bgVideo) return;
    body.classList.remove('custom-bg');
    body.classList.add('video-bg');
    body.style.backgroundImage = '';
    body.style.backgroundColor = '#000';

    bgVideo.src = src;
    bgVideo.muted = !audioEnabled;
    bgVideo.loop = true;
    var played = bgVideo.play();
    if (played && played.catch) {
      played.catch(function (e) {
        // Autoplay with audio is blocked without a gesture; the honest fix is to
        // start muted and let the user turn sound on with the visible button.
        console.warn('[background] autoplay blocked', e && e.name);
        bgVideo.muted = true;
        onStatus('Started muted: this window has not had a click yet, so sound stays off until you press the speaker button.', true);
        bgVideo.play().catch(function () {});
      });
    }

    var controls = document.getElementById('mediaControls');
    if (controls) controls.classList.add('show');
    var soundBtn = document.getElementById('soundBtn');
    if (soundBtn) {
      soundBtn.innerHTML = audioEnabled ? '&#128266;' : '&#128263;';
      soundBtn.title = audioEnabled ? 'Mute audio' : 'Enable audio';
    }
    var toggle = document.getElementById('videoAudioToggle');
    if (toggle) toggle.checked = !!audioEnabled;
  }

  function applyColor(hex) {
    if (!body) return;
    body.classList.add('custom-bg');
    body.style.backgroundImage = '';
    body.style.backgroundColor = hex;
    if (colorPicker) colorPicker.value = hex;
  }

  function applyImageUrl(url) {
    if (!body) return;
    body.classList.add('custom-bg');
    body.style.backgroundColor = '';
    body.style.backgroundImage = 'url("' + url + '")';
  }

  // Original bytes, so a restart does not re-compress an already-compressed
  // image. The 2560 px cap in prepareImage() is what keeps this reasonable.
  function prepareImage(file, cb) {
    var reader = new FileReader();
    reader.onerror = function () { cb(reader.error || new Error('could not read file'), null); };
    reader.onload = function () {
      var img = new Image();
      img.onerror = function () { cb(new Error('not a decodable image'), null); };
      img.onload = function () {
        var scale = Math.min(1, MAX_IMAGE_DIM / Math.max(img.width, img.height));
        if (scale >= 1 && file.size <= 4 * 1024 * 1024) {
          // Already small enough: keep the original bytes and its format.
          cb(null, file);
          return;
        }
        var canvas = document.createElement('canvas');
        canvas.width = Math.round(img.width * scale);
        canvas.height = Math.round(img.height * scale);
        var ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
        canvas.toBlob(function (blob) {
          if (!blob) { cb(new Error('could not encode image'), null); return; }
          cb(null, blob);
        }, 'image/jpeg', JPEG_QUALITY);
      };
      img.src = e.target.result;
    };
    reader.readAsDataURL(file);
  }

  function formatBytes(bytes) {
    if (!bytes && bytes !== 0) return '';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
  }

  // --- browser-backed storage --------------------------------------------
  // The page cannot write QSettings itself, so every local upload and every
  // colour/URL choice is mirrored to the browser. This is what makes the choice
  // survive a restart; localStorage alone never did.
  function persist(kind, blobOrNull, value) {
    var bridge = CloudarNewTab.bridge();
    if (!bridge || !bridge.saveWallpaperMedia) {
      onStatus('Saved for this tab only: the browser bridge is not available here, ' +
               'so this choice will not survive a restart.', true);
      return;
    }
    if (!blobOrNull) {
      bridge.saveWallpaperMedia(kind, '', value || '');
      return;
    }
    var reader = new FileReader();
    reader.onerror = function () {
      onStatus('Could not read the file to save it (' +
               ((reader.error && reader.error.name) || 'read error') + ').', true);
    };
    reader.onload = function () {
      // FileReader gives "data:<mime>;base64,<payload>"; the bridge wants the
      // payload alone and does its own base64 decode.
      var raw = String(reader.result || '');
      var comma = raw.indexOf(',');
      var payload = comma >= 0 ? raw.slice(comma + 1) : '';
      var url = bridge.saveWallpaperMedia(kind, payload, '');
      if (!url) {
        onStatus('The browser could not store the file on disk. It still works ' +
                 'until you close the tab.', true);
      }
    };
    reader.readAsDataURL(blobOrNull);
  }

  // Apply whatever the browser says is stored, without touching its own copy.
  function applyFromState(wallpaper) {
    if (!wallpaper || !wallpaper.kind || wallpaper.kind === 'default') return false;
    var kind = wallpaper.kind;
    if (kind === 'color') {
      applyColor(wallpaper.value);
      return true;
    }
    if (kind === 'web') {
      if (/\.(mp4|webm|ogg|ogv|mov|m4v)(\?.*)?$/i.test(wallpaper.value || '')) {
        applyVideo(wallpaper.value, false);
      } else {
        applyImageUrl(wallpaper.value);
      }
      return true;
    }
    if (!wallpaper.url) {
      // Stored preference exists but the file does not. Say so instead of
      // quietly showing the default background.
      onStatus('The saved wallpaper file is missing; showing the default background.', true);
      return false;
    }
    if (kind === 'video') {
      applyVideo(wallpaper.url, false);
      if (videoPreview) {
        videoPreview.src = wallpaper.url;
        var wrapper = document.getElementById('videoPreviewWrapper');
        if (wrapper) wrapper.classList.add('show');
      }
      return true;
    }
    applyImageUrl(wallpaper.url);
    if (imagePreview) {
      imagePreview.style.backgroundImage = 'url("' + wallpaper.url + '")';
      imagePreview.classList.add('show');
    }
    return true;
  }

  // --- public: setting local media ---------------------------------------
  function setLocalImage(file) {
    prepareImage(file, function (err, blob) {
      if (err) {
        onStatus('Could not use that image: ' + err.message, true);
        return;
      }
      releaseObjectUrl('image');
      putBlob(STORE_IMAGE, IMAGE_ID, blob, function (writeErr) {
        if (writeErr) {
          onStatus('Could not store the image (' + (writeErr.name || 'error') + ').', true);
          return;
        }
        deleteBlob(STORE_VIDEO, VIDEO_ID);
        var url = setObjectUrl('image', blob);
        applyImageUrl(url);
        if (imagePreview) {
          imagePreview.style.backgroundImage = 'url("' + url + '")';
          imagePreview.classList.add('show');
        }
        stopVideo();
        body.classList.add('custom-bg');
        saveSetting({ type: 'image', source: 'local' });
        persist('image', blob, '');
        onStatus('Image stored in IndexedDB (' + formatBytes(blob.size) + ').');
      });
    });
  }

  function setLocalVideo(file) {
    releaseObjectUrl('video');
    putBlob(STORE_VIDEO, VIDEO_ID, file, function (err) {
      if (err) {
        onStatus('Could not store the video (' + (err.name || 'error') +
                 '). Browsers cap IndexedDB per site, and video fills it quickly.', true);
        return;
      }
      deleteBlob(STORE_IMAGE, IMAGE_ID);
      if (imagePreview) imagePreview.classList.remove('show');

      var url = setObjectUrl('video', file);
      applyVideo(url, false);
      saveSetting({ type: 'video', source: 'local', sound: false });
      persist('video', file, '');

      if (videoPreview) {
        videoPreview.src = setObjectUrl('preview', file);
        videoPreview.muted = true;
        videoPreview.loop = true;
        var wrapper = document.getElementById('videoPreviewWrapper');
        if (wrapper) wrapper.classList.add('show');
        videoPreview.play().catch(function () {});
      }
      onStatus('Video stored in IndexedDB (' + formatBytes(file.size) +
               '). It is played from memory each time the tab opens.');
    });
  }

  // --- public: setting a web URL -----------------------------------------
  function setWebMedia(rawUrl) {
    var url = (rawUrl || '').trim();
    if (!url) return;
    var isVideo = /\.(mp4|webm|ogg|ogv|mov|m4v)(\?.*)?$/i.test(url);

    if (isVideo) {
      clearMedia();
      releaseObjectUrl('image');
      applyVideo(url, false);
      saveSetting({ type: 'video', source: 'web', value: url, sound: false });
      persist('web', null, url);
      if (videoPreview) {
        videoPreview.src = url;
        var wrapper = document.getElementById('videoPreviewWrapper');
        if (wrapper) wrapper.classList.add('show');
      }
      onStatus('Playing a remote video. Nothing is downloaded to disk.');
    } else {
      stopVideo();
      applyImageUrl(url);
      saveSetting({ type: 'web', value: url });
      persist('web', null, url);
      if (imagePreview) {
        imagePreview.style.backgroundImage = 'url("' + url + '")';
        imagePreview.classList.add('show');
      }
      onStatus('Using a remote image. It is fetched from the network each time.');
    }
  }

  function setColor(hex) {
    stopVideo();
    clearMedia();
    releaseObjectUrl('image');
    applyColor(hex);
    saveSetting({ type: 'color', value: hex });
    persist('color', null, hex);
    if (imagePreview) imagePreview.classList.remove('show');
    onStatus('');
  }

  function reset() {
    stopVideo();
    releaseObjectUrl('image');
    clearMedia();
    if (body) {
      body.classList.remove('custom-bg');
      body.classList.remove('video-bg');
      body.style.backgroundImage = '';
      body.style.backgroundColor = '';
    }
    if (imagePreview) imagePreview.classList.remove('show');
    saveSetting({ type: 'default' });
    persist('default', null, '');
    var bridge = CloudarNewTab.bridge();
    if (bridge && bridge.clearWallpaperMedia) bridge.clearWallpaperMedia();
    onStatus('');
  }

  // --- restore ------------------------------------------------------------
  // Order matters and is the whole fix:
  //   1. what the *browser* has stored (QSettings + AppDataLocation) — this is
  //      the only source that survives a restart;
  //   2. the local IndexedDB blob, if the browser had nothing (a wallpaper the
  //      browser refused to store still works while the profile lives);
  //   3. the old localStorage descriptor, mapped onto a stored URL when possible.
  function restoreFromBrowser() {
    var state = CloudarNewTab.state();
    if (state && state.wallpaper) return applyFromState(state.wallpaper);
    return false;
  }

  function restoreFromIndexedDb() {
    var setting = loadSetting();
    if (!setting || setting.type === 'default' || !setting.type) return false;

    if (setting.type === 'color') {
      applyColor(setting.value);
      return true;
    }
    if (setting.type === 'web') {
      if (/\.(mp4|webm|ogg|ogv|mov|m4v)(\?.*)?$/i.test(setting.value || '')) {
        applyVideo(setting.value, !!setting.sound);
      } else {
        applyImageUrl(setting.value);
      }
      return true;
    }
    if (setting.type === 'video') {
      getBlob(STORE_VIDEO, VIDEO_ID, function (err, blob) {
        if (err || !blob) return;
        applyVideo(setObjectUrl('video', blob), !!setting.sound);
        if (videoPreview) {
          videoPreview.src = setObjectUrl('preview', blob);
          var wrapper = document.getElementById('videoPreviewWrapper');
          if (wrapper) wrapper.classList.add('show');
        }
      });
      return true;
    }
    if (setting.type === 'image') {
      getBlob(STORE_IMAGE, IMAGE_ID, function (err, blob) {
        if (err || !blob) return;
        var url = setObjectUrl('image', blob);
        applyImageUrl(url);
        if (imagePreview) {
          imagePreview.style.backgroundImage = 'url("' + url + '")';
          imagePreview.classList.add('show');
        }
      });
      return true;
    }
    return false;
  }

  function restore() {
    if (restoreFromBrowser()) return;
    restoreFromIndexedDb();
  }

  return {
    init: function (statusFn) {
      body = document.body;
      bgVideo = document.getElementById('bgVideo');
      videoPreview = document.getElementById('videoPreview');
      imagePreview = document.getElementById('imagePreview');
      colorPicker = document.getElementById('colorPicker');
      onStatus = statusFn || function () {};

      // Losing the WebGL/decoding context happens when the renderer is suspended
      // (background tab, memory pressure). Re-apply so the user does not come
      // back to a black window.
      document.addEventListener('visibilitychange', function () {
        if (document.visibilityState === 'visible' && body.classList.contains('video-bg')
            && bgVideo && !bgVideo.src) {
          restore();
        }
      });

      // Applying what the browser stored needs the channel, which is not open
      // yet when this runs (app.js starts it on DOMContentLoaded). So: paint
      // from local storage immediately, then repaint from the browser's copy as
      // soon as the channel answers. The second paint is the one that survives
      // restarts; the first only prevents a flash of the default background.
      restoreFromIndexedDb();
      CloudarNewTab.afterChannel(function () {
        if (restoreFromBrowser())
          onStatus('');
      });
    },

    applyColor: setColor,
    applyWebMedia: setWebMedia,
    applyLocalImage: setLocalImage,
    applyLocalVideo: setLocalVideo,
    reset: reset,
    releaseAll: releaseAll,

    // Exposed for the customize panel's "how much space is this using" line.
    describe: function () {
      var setting = loadSetting() || { type: 'default' };
      return setting;
    },

    formatBytes: formatBytes
  };
})();
