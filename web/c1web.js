/* Front end of the c1 web build: asset import, storage, input helpers. */
'use strict';

(function () {
  var $ = function (id) { return document.getElementById(id); };
  var STREAMS = '/streams';
  var TITLE_LEVEL = 's0000019'; /* level 0x19: title sequence, the boot level */
  var ASSET_NAME = /^s[0-9a-f]{7}\.ns[fd]$/;

  /* indices into the engine's keys[] array (see SDLUpdate in src/pc/init.c) */
  var KEYS = {
    up: 0xD2, right: 0xCF, down: 0xD1, left: 0xD0,
    start: 13, select: 32,
    cross: 122, square: 120, circle: 99, triangle: 115,
    l1: 97, r1: 100, l2: 113, r2: 101
  };

  var started = false;

  function setStatus(text, isError) {
    var el = $('status');
    el.textContent = text;
    el.classList.toggle('error', !!isError);
  }

  /* ---- diagnostics ----------------------------------------------------- */

  var BUILD = window.C1_BUILD || { id: 'unknown', commit: 'unknown', flags: '' };
  var output = [];      /* last lines printed by the engine */
  var events = [];      /* what the page did, with timestamps */
  var startedAt = 0;
  var reported = false;

  function stamp() {
    return startedAt ? ((performance.now() - startedAt) / 1000).toFixed(2) + 's' : '-';
  }
  function note(text) {
    events.push('[' + stamp() + '] ' + text);
    if (events.length > 40) { events.shift(); }
  }
  function record(kind, text) {
    output.push('[' + stamp() + '] ' + kind + ' ' + text);
    if (output.length > 80) { output.shift(); }
  }

  /* A report must never fail itself: every item is read defensively. (With
     runtime checks on, touching a Module member that was not exported aborts.) */
  function safe(fn) {
    try { return fn(); } catch (e) { return 'unavailable (' + (e && e.message || e) + ')'; }
  }

  function glInfo() {
    try {
      var c = $('canvas');
      var gl = c.getContext('webgl2') || c.getContext('webgl');
      if (!gl) { return 'no context'; }
      var ext = gl.getExtension('WEBGL_debug_renderer_info');
      return gl.getParameter(gl.VERSION) + ' / ' + (ext ? gl.getParameter(ext.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER));
    } catch (e) { return 'unavailable (' + e.message + ')'; }
  }

  function filesInfo() {
    try {
      var FS = Module.FS;
      var names = FS.readdir(STREAMS).filter(function (n) { return ASSET_NAME.test(n); });
      var total = 0;
      names.forEach(function (n) { total += FS.stat(STREAMS + '/' + n).size; });
      var title = [TITLE_LEVEL + '.nsd', TITLE_LEVEL + '.nsf'].map(function (n) {
        return n + '=' + (names.indexOf(n) >= 0 ? FS.stat(STREAMS + '/' + n).size + ' bytes' : 'MISSING');
      });
      return names.length + ' files, ' + (total / 1048576).toFixed(1) + ' MB; ' + title.join(', ');
    } catch (e) { return 'unavailable (' + e.message + ')'; }
  }

  /* Wasm frames only: the interesting part of a call stack. */
  function cleanStack(stack) {
    if (!stack) { return '(no call stack)'; }
    return String(stack).split('\n').map(function (l) { return l.trim(); })
      .filter(function (l) { return l && !/^Error$/.test(l); })
      .map(function (l) { return l.replace(/https?:\/\/[^ )]*\//g, ''); })
      .slice(0, 40).join('\n');
  }

  function buildReport(title, detail, stack) {
    var c = $('canvas');
    var lines = [
      '=== c1 web error report ===',
      'Build: ' + BUILD.id + ' (' + BUILD.commit + ') ' + BUILD.flags,
      'Error: ' + title,
    ];
    if (detail) { lines.push('Detail: ' + detail); }
    lines.push('Time since Play: ' + stamp());
    lines.push('', '--- Call stack ---', cleanStack(stack));
    lines.push('', '--- Engine output (last lines) ---', output.length ? output.join('\n') : '(nothing printed)');
    lines.push('', '--- Page events ---', events.join('\n'));
    lines.push('', '--- Environment ---',
      'Game files: ' + safe(filesInfo),
      'Browser: ' + navigator.userAgent,
      'WebGL: ' + safe(glInfo),
      'Canvas: ' + c.width + 'x' + c.height + ', pixel ratio ' + window.devicePixelRatio,
      'Memory: ' + safe(function () { return (Module.HEAP8.length / 1048576).toFixed(0) + ' MB'; }),
      'Audio: ' + safe(function () {
        var ctx = Module.SDL2 && Module.SDL2.audioContext;
        return ctx ? ctx.state + ' @' + ctx.sampleRate + ' Hz' : 'not started';
      }));
    if (!/-sASSERTIONS/.test(BUILD.flags)) {
      lines.push('', 'Tip: rebuild with DEBUG=1 web/build.sh for function names and runtime checks.');
    }
    return lines.join('\n');
  }

  function openReport(title, text) {
    $('report-title').textContent = title;
    $('log').textContent = text;
    $('report').hidden = false;
  }

  /* The first failure is the one that matters; later ones are its fallout. */
  function reportError(title, detail, stack) {
    note('ERROR ' + title);
    if (reported) { return; }
    reported = true;
    var text;
    try { text = buildReport(title, detail, stack); }
    catch (e) { text = 'Error: ' + title + '\n' + cleanStack(stack) + '\n(the full report failed: ' + (e && e.message || e) + ')'; }
    openReport('The game stopped unexpectedly', text);
  }

  function showLog(text) {
    note(text);
    openReport('Log', text);
  }

  function setBusy(busy) {
    ['pick-disc', 'pick-files'].forEach(function (id) { $(id).classList.toggle('disabled', busy); });
    $('play').disabled = busy;
    $('forget').disabled = busy;
  }

  $('report-close').addEventListener('click', function () { $('report').hidden = true; });
  $('report-copy').addEventListener('click', function () {
    var text = $('log').textContent;
    var btn = this;
    var done = function () { btn.textContent = 'Copied'; setTimeout(function () { btn.textContent = 'Copy report'; }, 1500); };
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(done, function () {});
    } else {
      var range = document.createRange();
      range.selectNodeContents($('log'));
      var sel = window.getSelection();
      sel.removeAllRanges();
      sel.addRange(range);
      try { document.execCommand('copy'); done(); } catch (_) { /* select only */ }
    }
  });
  $('build-info').textContent = 'Build ' + BUILD.id + ' (' + BUILD.commit + ')';

  /* ---- emscripten module ------------------------------------------- */

  window.Module = {
    canvas: $('canvas'),
    /* load the engine that belongs to this build, never a cached older one */
    locateFile: function (path, dir) { return dir + path + '?v=' + encodeURIComponent(BUILD.id); },
    print: function (text) { console.log(text); record('out', text); },
    printErr: function (text) { console.warn(text); record('err', text); },
    onAbort: function (what) {
      /* called from inside the failing call: the stack here shows where */
      var stack = '';
      try { throw new Error('abort'); } catch (e) { stack = e.stack; }
      reportError('abort: ' + (what || '(no message: the C/C++ code called abort())'), '', stack);
    },
    onRuntimeInitialized: function () {
      var FS = Module.FS;
      FS.mkdir(STREAMS);
      try {
        FS.mount(Module.IDBFS, {}, STREAMS);
      } catch (e) {
        console.warn('persistent storage unavailable', e);
      }
      FS.syncfs(true, function (err) {
        if (err) { console.warn('could not load stored files', err); }
        refresh();
      });
    }
  };

  window.addEventListener('error', function (e) {
    if (!started) { return; }
    var err = e.error;
    reportError((err && err.name ? err.name + ': ' : '') + (e.message || String(err)),
      (e.filename || '').replace(/^.*\//, '') + ':' + e.lineno, err && err.stack);
  });
  window.addEventListener('unhandledrejection', function (e) {
    if (!started) { return; }
    var r = e.reason;
    reportError((r && r.name ? r.name + ': ' : '') + (r && r.message || String(r)), 'unhandled promise rejection', r && r.stack);
  });

  /* ---- stored assets ----------------------------------------------- */

  function storedAssets() {
    return Module.FS.readdir(STREAMS).filter(function (n) { return ASSET_NAME.test(n); });
  }

  function refresh(message) {
    var names = storedAssets();
    var hasTitle = names.indexOf(TITLE_LEVEL + '.nsf') >= 0 && names.indexOf(TITLE_LEVEL + '.nsd') >= 0;
    var levels = names.filter(function (n) { return n.slice(-1) === 'f'; }).length;
    $('play').hidden = !hasTitle;
    $('forget').hidden = names.length === 0;
    if (message) {
      setStatus(message, true);
    } else if (hasTitle) {
      setStatus('Game files ready (' + levels + ' level files stored).');
    } else if (names.length) {
      setStatus('The title level (' + TITLE_LEVEL.toUpperCase() + '.NSF/.NSD) is missing. Add the rest of the files.', true);
    } else {
      setStatus('Choose your disc image or the .NSF/.NSD files to begin.');
    }
    setBusy(false);
  }

  function persist() {
    return new Promise(function (resolve) {
      Module.FS.syncfs(false, function (err) {
        if (err) { console.warn('could not store files', err); }
        resolve();
      });
    });
  }

  function store(name, data) {
    Module.FS.writeFile(STREAMS + '/' + name, data);
  }

  /* ---- import: loose files ----------------------------------------- */

  async function importFiles(fileList) {
    var count = 0;
    for (var i = 0; i < fileList.length; i++) {
      var name = fileList[i].name.toLowerCase().replace(/;1$/, '');
      if (!ASSET_NAME.test(name)) { continue; }
      setStatus('Reading ' + name + '…');
      store(name, new Uint8Array(await fileList[i].arrayBuffer()));
      count++;
    }
    if (!count) { throw new Error('None of the selected files look like S00000xx.NSF / .NSD game files.'); }
    await persist();
  }

  /* ---- import: disc image (ISO 9660, raw 2352 or cooked 2048) ------- */

  function Disc(file) {
    this.file = file;
    this.sectorSize = 2048;
    this.dataOffset = 0;
  }

  Disc.prototype.detect = async function () {
    var head = new Uint8Array(await this.file.slice(0, 16).arrayBuffer());
    var raw = head.length === 16 && head[0] === 0 && head[11] === 0;
    for (var i = 1; raw && i < 11; i++) { raw = head[i] === 0xFF; }
    if (raw) {
      this.sectorSize = 2352;
      this.dataOffset = head[15] === 2 ? 24 : 16; /* mode 2 (XA) / mode 1 */
    }
    var pvd = await this.read(16, 1);
    if (String.fromCharCode(pvd[1], pvd[2], pvd[3], pvd[4], pvd[5]) !== 'CD001') {
      throw new Error('This file is not a CD image this page can read. Use the .bin (or .iso) of the disc.');
    }
    return pvd;
  };

  /* returns the user data of `count` sectors starting at `lba` */
  Disc.prototype.read = async function (lba, count) {
    var start = lba * this.sectorSize;
    var buf = new Uint8Array(await this.file.slice(start, start + count * this.sectorSize).arrayBuffer());
    if (buf.length < count * this.sectorSize) { throw new Error('The disc image is incomplete.'); }
    if (this.sectorSize === 2048) { return buf; }
    var out = new Uint8Array(count * 2048);
    for (var i = 0; i < count; i++) {
      var src = i * this.sectorSize + this.dataOffset;
      out.set(buf.subarray(src, src + 2048), i * 2048);
    }
    return out;
  };

  Disc.prototype.readExtent = async function (lba, size) {
    var out = new Uint8Array(size);
    var chunk = 512; /* sectors per read */
    var total = Math.ceil(size / 2048);
    for (var done = 0; done < total; done += chunk) {
      var n = Math.min(chunk, total - done);
      var data = await this.read(lba + done, n);
      out.set(data.subarray(0, Math.min(data.length, size - done * 2048)), done * 2048);
    }
    return out;
  };

  function le32(buf, off) {
    return (buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16) | (buf[off + 3] << 24)) >>> 0;
  }

  Disc.prototype.list = async function (lba, size, depth, found) {
    var dir = await this.readExtent(lba, size);
    var off = 0;
    while (off < dir.length) {
      var len = dir[off];
      if (len === 0) { /* records do not cross sectors */
        off = (Math.floor(off / 2048) + 1) * 2048;
        continue;
      }
      var nameLen = dir[off + 32];
      var name = '';
      for (var i = 0; i < nameLen; i++) { name += String.fromCharCode(dir[off + 33 + i]); }
      var entry = { lba: le32(dir, off + 2), size: le32(dir, off + 10), isDir: !!(dir[off + 25] & 2) };
      off += len;
      if (name === '\u0000' || name === '\u0001') { continue; }
      if (entry.isDir) {
        if (depth < 4) { await this.list(entry.lba, entry.size, depth + 1, found); }
      } else {
        entry.name = name.toLowerCase().replace(/;\d+$/, '');
        if (ASSET_NAME.test(entry.name)) { found.push(entry); }
      }
    }
    return found;
  };

  async function importDisc(file) {
    var disc = new Disc(file);
    setStatus('Reading disc image…');
    var pvd = await disc.detect();
    var found = await disc.list(le32(pvd, 156 + 2), le32(pvd, 156 + 10), 0, []);
    if (!found.length) { throw new Error('No S00000xx.NSF / .NSD files were found on this disc.'); }
    for (var i = 0; i < found.length; i++) {
      setStatus('Copying game files… ' + (i + 1) + ' / ' + found.length);
      store(found[i].name, await disc.readExtent(found[i].lba, found[i].size));
    }
    await persist();
  }

  function runImport(promise) {
    setBusy(true);
    promise.then(function () { refresh(); }, function (err) {
      console.error(err);
      refresh(err.message || String(err));
    });
  }

  $('disc').addEventListener('change', function () {
    if (this.files.length) { runImport(importDisc(this.files[0])); }
    this.value = '';
  });
  $('files').addEventListener('change', function () {
    if (this.files.length) { runImport(importFiles(this.files)); }
    this.value = '';
  });
  $('forget').addEventListener('click', function () {
    if (!confirm('Remove the game files stored in this browser?')) { return; }
    storedAssets().forEach(function (n) { Module.FS.unlink(STREAMS + '/' + n); });
    runImport(persist());
  });

  /* ---- input: keyboard, gamepads and the touch overlay ---------------- */

  var held = {}; /* source:key -> bool, so sources do not release each other's keys */

  function setKey(source, key, down) {
    var id = source + ':' + key;
    if (!!held[id] === down) { return; }
    held[id] = down;
    var any = Object.keys(held).some(function (k) { return held[k] && k.split(':')[1] === key; });
    Module._WebSetKey(KEYS[key], any ? 1 : 0);
  }

  /* standard gamepad layout */
  var PAD_BUTTONS = {
    0: 'cross', 1: 'circle', 2: 'square', 3: 'triangle',
    4: 'l1', 5: 'r1', 6: 'l2', 7: 'r2', 8: 'select', 9: 'start',
    12: 'up', 13: 'down', 14: 'left', 15: 'right'
  };

  function pollGamepads() {
    var pads = navigator.getGamepads ? navigator.getGamepads() : [];
    var state = {};
    var any = false;
    for (var p = 0; p < pads.length; p++) {
      var pad = pads[p];
      if (!pad || !pad.connected) { continue; }
      for (var b in PAD_BUTTONS) {
        if (pad.buttons[b] && pad.buttons[b].pressed) { state[PAD_BUTTONS[b]] = true; any = true; }
      }
      if (pad.axes.length >= 2) {
        if (pad.axes[0] < -0.5) { state.left = true; any = true; }
        if (pad.axes[0] > 0.5) { state.right = true; any = true; }
        if (pad.axes[1] < -0.5) { state.up = true; any = true; }
        if (pad.axes[1] > 0.5) { state.down = true; any = true; }
      }
    }
    for (var key in KEYS) { setKey('pad', key, !!state[key]); }
    if (any) { physicalInput(); }
    requestAnimationFrame(pollGamepads);
  }

  /* ---- preferences ---------------------------------------------------- */

  var PREFS_KEY = 'c1web.prefs.v1';
  var prefs = { touch: 'auto' };
  try { Object.assign(prefs, JSON.parse(localStorage.getItem(PREFS_KEY) || '{}')); } catch (_) { /* defaults */ }
  function savePrefs() {
    try { localStorage.setItem(PREFS_KEY, JSON.stringify(prefs)); } catch (_) { /* ignore */ }
  }

  /* ---- display: canvas size, orientation, fullscreen ------------------ */
  /* (adapted from ReChan Web: input.js resizeCanvas, app.js orientation) */

  var isTouchDevice = ('ontouchstart' in window) || navigator.maxTouchPoints > 0;
  var isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent) ||
    (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  var playing = false;

  /* Match the canvas resolution to its size on screen. The engine reads the
     canvas size every frame and sets the GL viewport from it. */
  function resizeCanvas() {
    var canvas = $('canvas');
    if (!playing) { return; }
    var dpr = Math.min(window.devicePixelRatio || 1, 2);
    var rect = canvas.getBoundingClientRect();
    var w = Math.max(1, Math.round(rect.width * dpr));
    var h = Math.max(1, Math.round(rect.height * dpr));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
    }
  }

  /* Portrait on a phone/tablet: game in a 4:3 band at the top, touch controls
     underneath (handheld layout). Landscape keeps the full screen. */
  function updateOrientationLayout() {
    var portrait = window.matchMedia ? window.matchMedia('(orientation: portrait)').matches
      : window.innerHeight > window.innerWidth;
    var handheld = C1Touch.shown || C1Touch.editing;
    document.body.classList.toggle('portrait-play', portrait && playing && handheld);
    resizeCanvas();
  }

  /* iOS fires "resize" before a rotation has settled: run again a bit later. */
  function settle() {
    updateOrientationLayout();
    setTimeout(updateOrientationLayout, 250);
    setTimeout(updateOrientationLayout, 600);
  }

  function isFullscreen() {
    return !!(document.fullscreenElement || document.webkitFullscreenElement);
  }

  function toggleFullscreen() {
    var el = document.documentElement;
    try {
      if (isFullscreen()) {
        (document.exitFullscreen || document.webkitExitFullscreen).call(document);
      } else if (el.requestFullscreen) {
        var p = el.requestFullscreen({ navigationUI: 'hide' });
        if (p && p.catch) { p.catch(function () {}); }
      } else if (el.webkitRequestFullscreen) {
        el.webkitRequestFullscreen();
      } else {
        showLog('Fullscreen is not available here. On iPhone, add the page to the home screen instead.');
      }
    } catch (_) { /* not allowed right now */ }
  }

  /* ---- audio ---------------------------------------------------------- */

  /* Browsers start audio suspended until a user gesture, and iOS suspends it
     again whenever the page goes to the background. */
  function resumeAudio() {
    var ctx = Module.SDL2 && Module.SDL2.audioContext;
    if (ctx && ctx.state !== 'running' && ctx.resume) {
      var p = ctx.resume();
      if (p && p.catch) { p.catch(function () {}); }
    }
  }

  /* ---- touch overlay --------------------------------------------------- */

  var touchHiddenByPhysical = false;

  function updateTouchVisibility() {
    if (!playing || C1Touch.editing) { return; }
    var show;
    if (prefs.touch === 'on') { show = true; }
    else if (prefs.touch === 'off') { show = false; }
    else { show = isTouchDevice && !touchHiddenByPhysical; }
    C1Touch.show(show);
    updateOrientationLayout();
  }

  /* Hide the overlay when a real pad or keyboard is used; show it again on touch. */
  function physicalInput() {
    if (prefs.touch === 'auto' && !touchHiddenByPhysical && !C1Touch.editing) {
      touchHiddenByPhysical = true;
      updateTouchVisibility();
    }
  }

  function onTouchState(names) {
    for (var key in KEYS) { setKey('touch', key, names.indexOf(key) >= 0); }
  }

  function selectForEditor(c) {
    $('ed-selected').classList.toggle('disabled', !c);
    $('ed-name').textContent = c ? c.label : '—';
    if (c) { $('ed-size').value = c.size; }
  }

  function openTouchEditor() {
    closeMenu();
    $('touch-editor').hidden = false;
    document.body.classList.add('editing-touch');
    var s = C1Touch.settings;
    $('ed-opacity').value = s.opacity;
    $('ed-mode').value = s.mode;
    $('ed-haptics').checked = !!s.haptics;
    selectForEditor(null);
    C1Touch.setEditing(true, { onSelect: selectForEditor });
    updateOrientationLayout();
  }

  function closeTouchEditor() {
    C1Touch.setEditing(false);
    $('touch-editor').hidden = true;
    document.body.classList.remove('editing-touch');
    C1Touch.show(false);
    updateTouchVisibility();
  }

  function openMenu() {
    $('opt-touch').value = prefs.touch;
    $('opt-fullscreen').textContent = isFullscreen() ? 'Exit fullscreen' : 'Fullscreen';
    $('menu').hidden = false;
  }
  function closeMenu() { $('menu').hidden = true; }

  function wireGameUi() {
    C1Touch.init($('touch-layer'), onTouchState);

    $('menu-btn').addEventListener('click', function () { if ($('menu').hidden) { openMenu(); } else { closeMenu(); } this.blur(); });
    $('opt-close').addEventListener('click', closeMenu);
    $('opt-fullscreen').addEventListener('click', function () { toggleFullscreen(); closeMenu(); });
    $('opt-edit').addEventListener('click', openTouchEditor);
    $('opt-log').addEventListener('click', function () {
      closeMenu();
      openReport('Log', buildReport('(none: report opened from the menu)', '', '(not an error)'));
    });
    $('opt-touch').addEventListener('change', function () {
      prefs.touch = this.value;
      touchHiddenByPhysical = false;
      savePrefs();
      updateTouchVisibility();
    });

    $('ed-size').addEventListener('input', function () { C1Touch.updateSelected({ size: parseFloat(this.value) }); });
    $('ed-hide').addEventListener('click', function () { C1Touch.updateSelected({ visible: false }); selectForEditor(null); });
    $('ed-opacity').addEventListener('input', function () { C1Touch.setSetting('opacity', parseFloat(this.value)); });
    $('ed-mode').addEventListener('change', function () { C1Touch.setSetting('mode', this.value); });
    $('ed-haptics').addEventListener('change', function () { C1Touch.setSetting('haptics', this.checked); });
    $('ed-showall').addEventListener('click', function () { C1Touch.showAll(); });
    $('ed-reset').addEventListener('click', function () {
      C1Touch.reset();
      var s = C1Touch.settings;
      $('ed-opacity').value = s.opacity;
      $('ed-mode').value = s.mode;
      $('ed-haptics').checked = !!s.haptics;
      selectForEditor(null);
    });
    $('ed-done').addEventListener('click', closeTouchEditor);

    window.addEventListener('keydown', function (e) {
      if (e.isTrusted && !e.repeat) { physicalInput(); }
      resumeAudio();
    }, true);
    window.addEventListener('pointerdown', function (e) {
      if (e.pointerType === 'touch' && touchHiddenByPhysical) {
        touchHiddenByPhysical = false;
        updateTouchVisibility();
      }
      resumeAudio();
    }, true);
    document.addEventListener('visibilitychange', function () {
      if (document.visibilityState === 'visible') { resumeAudio(); }
    });

    if (typeof ResizeObserver !== 'undefined') { new ResizeObserver(resizeCanvas).observe($('canvas')); }
    window.addEventListener('resize', settle);
    window.addEventListener('orientationchange', settle);
    if (window.visualViewport) { window.visualViewport.addEventListener('resize', settle); }
    document.addEventListener('fullscreenchange', settle);
    document.addEventListener('webkitfullscreenchange', settle);
  }

  /* ---- start -------------------------------------------------------- */

  $('play').addEventListener('click', function () {
    if (started) { return; }
    started = true;
    playing = true;
    startedAt = performance.now();
    note('Play pressed');
    $('setup').hidden = true;
    $('canvas').hidden = false;
    $('menu-btn').hidden = false;
    document.body.classList.add('playing');
    wireGameUi();
    updateTouchVisibility();
    requestAnimationFrame(pollGamepads);
    /* Android: go fullscreen with the tap that starts the game (iOS has no
       fullscreen for pages; there the home-screen app is the way). */
    if (!isIOS && isTouchDevice && document.documentElement.requestFullscreen) {
      document.documentElement.requestFullscreen({ navigationUI: 'hide' }).catch(function () {});
    }
    try {
      Module.callMain([]);
      note('engine started (main is running)');
    } catch (e) {
      reportError((e && e.name ? e.name + ': ' : '') + (e && e.message || String(e)), 'thrown while starting', e && e.stack);
    }
    /* SDL sets its own canvas size when it creates the window */
    settle();
  });
})();
