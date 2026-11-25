/* ==========================================================================
   frame-editor.js — the "adjust the frame" mode for the wallpaper.

   Opened automatically once an image or a video has been applied, and again from
   the "Adjust the frame" button in the Customize panel. Dragging moves the
   wallpaper, the wheel or the slider zooms, and the result is saved through
   CloudarBackground -> newTabBridge.saveWallpaperFraming() -> QSettings.

   It is deliberately WYSIWYG rather than a preview box: it transforms the real
   wallpaper layer. A separate preview would need its own copy of the geometry
   and its own copy of "what does cover mean", which is how a panel ends up
   showing one crop and applying another.

   Only three numbers exist: x and y (which point of the image is centred, each
   -1..1) and zoom (1..4). 0,0,1 is the centred cover the page has always used,
   so every stored wallpaper from before this feature still renders identically.
   ========================================================================== */

var CloudarFrameEditor = (function () {
  'use strict';

  var overlay = null;
  var bar = null;
  var zoomInput = null;
  var zoomValue = null;
  var hint = null;
  var outline = null;
  var adjustButton = null;
  var open = false;

  // Drag bookkeeping. The start point is kept so a pointer that moves 300 px
  // maps to 300 px of image movement regardless of how many move events fire.
  var drag = null;
  var idleTimer = null;

  function bg() {
    return typeof CloudarBackground === 'undefined' ? null : CloudarBackground;
  }

  function bounds() {
    var api = bg();
    return (api && api.framingBounds) ? api.framingBounds() : { min: 1, max: 4 };
  }

  function syncControls() {
    var api = bg();
    if (!api) return;
    var f = api.framing();
    if (zoomInput) zoomInput.value = String(f.zoom);
    if (zoomValue) zoomValue.textContent = Math.round(f.zoom * 100) + '%';
    syncAdjustButton();
  }

  // The button in the panel is only enabled when there is something to frame. A
  // control that is offered but does nothing is a defect, not a placeholder.
  function syncAdjustButton() {
    var api = bg();
    if (!adjustButton) return;
    var available = !!(api && api.isFramable && api.isFramable());
    adjustButton.disabled = !available;
    adjustButton.title = available
      ? 'Drag and zoom the wallpaper'
      : 'Choose an image or a video first';
  }

  function showIdle() {
    if (!overlay) return;
    overlay.classList.add('idle');
    if (idleTimer) clearTimeout(idleTimer);
    idleTimer = null;
  }

  // Fade the outline while dragging so the user judges the picture, not the
  // guide; it comes back when the pointer settles.
  function wake() {
    if (!overlay) return;
    overlay.classList.remove('idle');
    if (idleTimer) clearTimeout(idleTimer);
    idleTimer = setTimeout(showIdle, 1200);
  }

  function openEditor() {
    var api = bg();
    if (!api || !overlay || !api.isFramable()) return false;
    open = true;
    overlay.hidden = false;
    document.body.classList.add('frame-editing');
    syncControls();
    wake();
    // Announce the mode so the panel can close itself and the page can stop
    // animating underneath; the editor does not reach into other components.
    document.dispatchEvent(new CustomEvent('cloudar:framing-open'));
    if (overlay.focus) overlay.focus();
    return true;
  }

  function close(save) {
    if (!open) return;
    open = false;
    if (overlay) overlay.hidden = true;
    if (idleTimer) { clearTimeout(idleTimer); idleTimer = null; }
    document.body.classList.remove('frame-editing');
    drag = null;
    var api = bg();
    // Committing on close turns the debounced write into one final write, so a
    // drag that ends in the same tick as the close is not lost.
    if (save !== false && api && api.commitFraming) api.commitFraming();
    document.dispatchEvent(new CustomEvent('cloudar:framing-close'));
  }

  // --- zoom ---------------------------------------------------------------
  // Zoom keeps the centre of the window fixed, which is what a person expects
  // from a wheel: the middle of the screen stays the middle of the screen.
  function setZoom(next) {
    var api = bg();
    if (!api) return;
    var f = api.framing();
    api.setFraming({ x: f.x, y: f.y, zoom: next });
    syncControls();
  }

  function onWheel(event) {
    var api = bg();
    if (!api) return;
    event.preventDefault();
    var b = bounds();
    var f = api.framing();
    // Multiplicative steps so 100% -> 110% feels the same as 300% -> 330%.
    var factor = event.deltaY < 0 ? 1.08 : 1 / 1.08;
    var zoom = Math.min(b.max, Math.max(b.min, f.zoom * factor));
    setZoom(zoom);
    wake();
  }

  // --- drag ---------------------------------------------------------------
  function onPointerDown(event) {
    // Clicks on the bar and its controls must not start a drag.
    if (bar && bar.contains(event.target)) return;
    var api = bg();
    if (!api) return;
    if (event.button !== undefined && event.button !== 0) return;

    var f = api.framing();
    var limit = api.maxPan ? api.maxPan() : { x: 0, y: 0 };
    drag = {
      id: event.pointerId,
      startX: event.clientX,
      startY: event.clientY,
      x: f.x,
      y: f.y,
      // Pixels of wallpaper travel per pixel of pointer movement, per axis. The
      // background module owns this number because it is the one that knows the
      // layer's real size; dividing by it keeps the wallpaper glued to the
      // pointer instead of racing it when the image is much wider than the
      // window.
      perPixelX: limit.x > 0 ? 1 / limit.x : 0,
      perPixelY: limit.y > 0 ? 1 / limit.y : 0
    };
    if (!drag.perPixelX && !drag.perPixelY) {
      // Only when the aspect ratio matches exactly, so there is genuinely nothing
      // cropped. A 4:3 photo in a 16:9 window pans even at 100%.
      hint.textContent = 'This image matches the window exactly — zoom in to move it.';
      return;
    }
    if (overlay.setPointerCapture && event.pointerId !== undefined) {
      try { overlay.setPointerCapture(event.pointerId); } catch (err) { /* ignore */ }
    }
    if (overlay.classList) overlay.classList.add('dragging');
    event.preventDefault();
    wake();
  }

  function onPointerMove(event) {
    if (!drag) return;
    var api = bg();
    if (!api) return;
    // Dragging right reveals what is on the left, so the focus moves negative.
    var x = drag.x - (event.clientX - drag.startX) * drag.perPixelX;
    var y = drag.y - (event.clientY - drag.startY) * drag.perPixelY;
    api.setFraming({ x: x, y: y, zoom: api.framing().zoom });
    wake();
  }

  function onPointerUp(event) {
    if (!drag) return;
    drag = null;
    if (overlay.classList) overlay.classList.remove('dragging');
    if (overlay.releasePointerCapture && event && event.pointerId !== undefined) {
      try { overlay.releasePointerCapture(event.pointerId); } catch (err) { /* ignore */ }
    }
    var api = bg();
    if (api && api.commitFraming) api.commitFraming();
  }

  // --- buttons ------------------------------------------------------------
  function onFit() {
    var api = bg();
    if (!api) return;
    api.setFraming({ x: 0, y: 0, zoom: bounds().min }, { commit: true });
    syncControls();
    if (hint) hint.textContent = 'Fitted: the whole image fills the screen, edges cropped.';
    wake();
  }

  function onCentre() {
    var api = bg();
    if (!api) return;
    var f = api.framing();
    api.setFraming({ x: 0, y: 0, zoom: f.zoom }, { commit: true });
    if (hint) hint.textContent = 'Centred.';
    wake();
  }

  function init() {
    overlay = document.getElementById('frameOverlay');
    bar = document.querySelector('.frame-bar');
    zoomInput = document.getElementById('frameZoom');
    zoomValue = document.getElementById('frameZoomValue');
    hint = document.getElementById('frameHint');
    outline = document.getElementById('frameOutline');
    adjustButton = document.getElementById('adjustFrame');
    if (!overlay) return;

    var b = bounds();
    if (zoomInput) {
      zoomInput.min = String(b.min);
      zoomInput.max = String(b.max);
      zoomInput.addEventListener('input', function () {
        setZoom(Number(zoomInput.value));
        wake();
      });
    }

    overlay.addEventListener('pointerdown', onPointerDown);
    overlay.addEventListener('pointermove', onPointerMove);
    overlay.addEventListener('pointerup', onPointerUp);
    overlay.addEventListener('pointercancel', onPointerUp);
    overlay.addEventListener('wheel', onWheel, { passive: false });
    overlay.addEventListener('dblclick', function (event) {
      if (bar && bar.contains(event.target)) return;
      onFit();
    });

    overlay.addEventListener('keydown', function (event) {
      if (event.key === 'Escape') { event.preventDefault(); close(true); return; }
      if (event.key === '+' || event.key === '=') { event.preventDefault(); setZoom(bg().framing().zoom * 1.1); }
      if (event.key === '-') { event.preventDefault(); setZoom(bg().framing().zoom / 1.1); }
      if (event.key === 'ArrowLeft') { event.preventDefault(); nudge(-0.05, 0); }
      if (event.key === 'ArrowRight') { event.preventDefault(); nudge(0.05, 0); }
      if (event.key === 'ArrowUp') { event.preventDefault(); nudge(0, -0.05); }
      if (event.key === 'ArrowDown') { event.preventDefault(); nudge(0, 0.05); }
    });

    var done = document.getElementById('frameDone');
    if (done) done.addEventListener('click', function () { close(true); });
    var fit = document.getElementById('frameFit');
    if (fit) fit.addEventListener('click', onFit);
    var centre = document.getElementById('frameCenter');
    if (centre) centre.addEventListener('click', onCentre);

    if (adjustButton) {
      adjustButton.addEventListener('click', function () { openEditor(); });
    }

    // The wallpaper can change without the editor being open; keep the button's
    // enabled state honest.
    document.addEventListener('cloudar:framing', syncControls);
    document.addEventListener('cloudar:wallpaper-applied', function () {
      syncControls();
      if (!open && bg() && bg().isFramable()) openEditor();
    });

    var api = bg();
    if (api && api.setFrameEditor) api.setFrameEditor({ open: openEditor, close: close });
    syncAdjustButton();
  }

  // Arrow keys move by a fraction of the croppable overflow, so the step feels
  // the same at every zoom level.
  function nudge(dx, dy) {
    var api = bg();
    if (!api) return;
    var f = api.framing();
    var limit = api.maxPan ? api.maxPan() : { x: 0, y: 0 };
    if (!limit.x && !limit.y) {
      if (hint) hint.textContent = 'This image matches the window exactly — zoom in to move it.';
      return;
    }
    // 5% of the window, converted through the same scale the drag uses.
    var stepX = limit.x > 0 ? (window.innerWidth * 0.05) / limit.x : 0;
    var stepY = limit.y > 0 ? (window.innerHeight * 0.05) / limit.y : 0;
    api.setFraming({ x: f.x + dx * 12 * stepX, y: f.y + dy * 12 * stepY, zoom: f.zoom });
    wake();
  }

  CloudarNewTab.onReady(function () { init(); });

  return {
    open: openEditor,
    close: close,
    isOpen: function () { return open; },
    syncAdjustButton: syncAdjustButton
  };
})();
