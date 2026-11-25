/* ==========================================================================
   customize.js — the slide-out panel that drives background.js.

   Split out from the wallpaper code on purpose: this file only translates
   clicks into CloudarBackground calls and keeps the controls in sync. It used
   to be interleaved with the storage logic, which is how the panel ended up
   holding a second, divergent copy of "is a video playing".
   ========================================================================== */

var CloudarCustomize = (function () {
  'use strict';

  var PRESET_COLORS = ['#10131a', '#1a2233', '#0f2027', '#2c1e3d',
                       '#1b2a1d', '#3a1f1f', '#1f2937', '#0b132b'];

  var panel = null;
  var swatchesEl = null;
  var colorPicker = null;
  var statusEl = null;

  function setStatus(text, warn) {
    if (!statusEl) return;
    statusEl.textContent = text || '';
    statusEl.classList.toggle('warn', !!warn);
  }

  function buildSwatches() {
    if (!swatchesEl) return;
    swatchesEl.innerHTML = '';
    PRESET_COLORS.forEach(function (hex) {
      var el = document.createElement('div');
      el.className = 'swatch';
      el.style.background = hex;
      el.title = hex;
      el.addEventListener('click', function () {
        CloudarBackground.applyColor(hex);
        markActive(hex);
      });
      swatchesEl.appendChild(el);
    });
  }

  function markActive(hex) {
    if (!swatchesEl) return;
    var kids = swatchesEl.querySelectorAll('.swatch');
    for (var i = 0; i < kids.length; i++) {
      kids[i].classList.toggle('active', !hex || kids[i].title === hex);
    }
  }

  // Remember which kind of media is live so the audio checkbox and the
  // play/pause button act on something that actually exists.
  function videoElement() { return document.getElementById('bgVideo'); }

  return {
    init: function () {
      panel = document.getElementById('panel');
      swatchesEl = document.getElementById('swatches');
      colorPicker = document.getElementById('colorPicker');
      statusEl = document.getElementById('bgStatus');

      buildSwatches();

      // Opening the panel is a page-wide state, not a panel-local class: two
      // window-anchored buttons (sound, pause) have to move out from under it.
      function setPanelOpen(isOpen) {
        if (!panel) return;
        panel.classList.toggle('open', isOpen);
        document.body.classList.toggle('panel-open', isOpen);
      }

      var open = document.getElementById('customizeBtn');
      var close = document.getElementById('closePanel');
      if (open && panel) open.addEventListener('click', function () { setPanelOpen(true); });
      if (close && panel) close.addEventListener('click', function () { setPanelOpen(false); });

      // --- background media from this device -------------------------------
      var fileInput = document.getElementById('imageFile');
      if (fileInput) {
        fileInput.addEventListener('change', function () {
          var file = fileInput.files && fileInput.files[0];
          if (!file) return;
          var isVideo = /^video\//.test(file.type) ||
                        /\.(mp4|webm|ogg|ogv|mov|m4v|mkv)$/i.test(file.name);
          if (isVideo) {
            CloudarBackground.applyLocalVideo(file);
          } else {
            CloudarBackground.applyLocalImage(file);
          }
          // Allow choosing the same file again after switching away from it.
          fileInput.value = '';
        });
      }

      // --- background media from the web -----------------------------------
      var webInput = document.getElementById('webImageUrl');
      var webApply = document.getElementById('applyWebImage');
      if (webApply && webInput) {
        webApply.addEventListener('click', function () {
          var url = webInput.value.trim();
          if (!url) { webInput.focus(); return; }
          if (!/^https?:\/\//i.test(url)) {
            setStatus('Only http(s) URLs can be used as a wallpaper.', true);
            return;
          }
          CloudarBackground.applyWebMedia(url);
        });
        webInput.addEventListener('keydown', function (e) {
          if (e.key === 'Enter') { e.preventDefault(); webApply.click(); }
        });
      }

      // --- colour ----------------------------------------------------------
      if (colorPicker) {
        colorPicker.addEventListener('input', function () {
          CloudarBackground.applyColor(colorPicker.value);
        });
        colorPicker.addEventListener('change', function () {
          CloudarBackground.applyColor(colorPicker.value);
          markActive(colorPicker.value);
        });
      }

      // --- the frame editor takes over the screen ---------------------------
      // The panel is 300 px of controls sitting exactly where the user needs to
      // look at (and drag) the wallpaper, so it gets out of the way. It reopens
      // on close because the user is still in the middle of customising.
      var panelWasOpen = false;
      document.addEventListener('cloudar:framing-open', function () {
        if (!panel) return;
        panelWasOpen = panel.classList.contains('open');
        if (panelWasOpen) setPanelOpen(false);
      });
      document.addEventListener('cloudar:framing-close', function () {
        if (panel && panelWasOpen) setPanelOpen(true);
        panelWasOpen = false;
      });

      // --- reset -----------------------------------------------------------
      var resetBtn = document.getElementById('resetBg');
      if (resetBtn) {
        resetBtn.addEventListener('click', function () {
          CloudarBackground.reset();
          if (webInput) webInput.value = '';
          markActive(null);
        });
      }

      // --- audio toggle ----------------------------------------------------
      var audioToggle = document.getElementById('videoAudioToggle');
      if (audioToggle) {
        audioToggle.addEventListener('change', function () {
          var v = videoElement();
          if (!v) return;
          v.muted = !audioToggle.checked;
          var soundBtn = document.getElementById('soundBtn');
          if (soundBtn) {
            soundBtn.innerHTML = v.muted ? '&#128263;' : '&#128266;';
            soundBtn.title = v.muted ? 'Enable audio' : 'Mute audio';
          }
          saveSound(!v.muted);
        });
      }

      var soundBtn = document.getElementById('soundBtn');
      if (soundBtn) {
        soundBtn.addEventListener('click', function () {
          var v = videoElement();
          if (!v) return;
          v.muted = !v.muted;
          var toggle = document.getElementById('videoAudioToggle');
          if (toggle) toggle.checked = !v.muted;
          soundBtn.innerHTML = v.muted ? '&#128263;' : '&#128266;';
          soundBtn.title = v.muted ? 'Enable audio' : 'Mute audio';
          saveSound(!v.muted);
        });
      }

      var playPause = document.getElementById('playPauseBtn');
      if (playPause) {
        playPause.addEventListener('click', function () {
          var v = videoElement();
          if (!v) return;
          if (v.paused) {
            v.play().catch(function () {});
            playPause.innerHTML = '&#9208;';
            playPause.title = 'Pause';
          } else {
            v.pause();
            playPause.innerHTML = '&#9658;';
            playPause.title = 'Play';
          }
        });
      }

      // Reflect whatever the *browser* has stored, which is the authoritative
      // copy; the page's own localStorage is only a fast path while this tab is
      // open. Without this the panel showed "no image" even when a wallpaper was
      // loaded, because nothing ever read the browser's answer back.
      CloudarNewTab.whenReady(function (state) {
        var wallpaper = (state && state.wallpaper) || { kind: 'default' };
        if (wallpaper.kind === 'color' && wallpaper.value) {
          markActive(wallpaper.value);
        } else if (wallpaper.kind === 'web' && wallpaper.value) {
          if (webInput) webInput.value = wallpaper.value;
        } else if (wallpaper.kind === 'image' || wallpaper.kind === 'video') {
          var size = CloudarBackground.formatBytes(wallpaper.bytes || 0);
          setStatus((wallpaper.kind === 'video' ? 'Video' : 'Image') +
                    ' wallpaper stored by the browser' +
                    (size ? ' (' + size + ')' : '') + '.');
        } else {
          setStatus('');
        }
      });

      CloudarBackground.init(setStatus);
    }
  };

  // Persist the sound choice without touching the rest of the descriptor, so a
  // mute toggle cannot silently downgrade a stored local video to a web one.
  function saveSound(enabled) {
    try {
      var raw = localStorage.getItem('cloudar.newtab.background');
      if (!raw) return;
      var s = JSON.parse(raw);
      if (s && s.type === 'video') {
        s.sound = !!enabled;
        localStorage.setItem('cloudar.newtab.background', JSON.stringify(s));
      }
    } catch (e) { /* storage disabled: the choice lasts for this tab */ }
  }
})();

CloudarNewTab.onReady(function () { CloudarCustomize.init(); });
