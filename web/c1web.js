/* Front end of the c1 web build: asset import, storage, input helpers. */
'use strict';

(function () {
  var $ = function (id) { return document.getElementById(id); };
  /* Extra entry of the game's own title menu, below OPTIONS: choosing it
     opens this address in a new tab. `label` is drawn with the game's font:
     up to 16 characters, capital letters, digits and spaces. An empty url
     removes the entry. */
  var MENU_LINK = { label: 'MORE GAMES', url: 'https://x.com/bardock_dev' };

  /* shown at the bottom right of the game's title menu */
  var CREDIT = 'Crash Bandicoot web port by Bardock';

  var STREAMS = '/streams';
  var SAVES = '/save';
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
    debugLine('page', text);
  }
  function record(kind, text) {
    output.push('[' + stamp() + '] ' + kind + ' ' + text);
    if (output.length > 80) { output.shift(); }
    debugLine(kind, text);
  }

  /* ---- debug panel: log ------------------------------------------------ */
  /* kinds: out/err = printed by the engine, page = what the page did,
     game = engine state changes noticed by the page, warn, error */

  var DEBUG_MAX = 400;
  var debugLines = [];

  function debugLine(kind, text) {
    var line = { t: stamp(), kind: kind, text: String(text) };
    debugLines.push(line);
    if (debugLines.length > DEBUG_MAX) { debugLines.shift(); }
    var box = $('debug-log');
    if (!box || $('debug').hidden) { return; }
    appendDebugLine(box, line);
    while (box.childNodes.length > DEBUG_MAX) { box.removeChild(box.firstChild); }
  }

  function appendDebugLine(box, line) {
    /* keep following the end unless the reader scrolled up */
    var follow = box.scrollTop + box.clientHeight >= box.scrollHeight - 30;
    var row = document.createElement('div');
    row.className = 'k-' + line.kind;
    var t = document.createElement('span');
    t.className = 't';
    t.textContent = line.t + ' ';
    row.appendChild(t);
    row.appendChild(document.createTextNode(line.text));
    box.appendChild(row);
    if (follow) { box.scrollTop = box.scrollHeight; }
  }

  function debugText() {
    return $('debug-stats').textContent + '\n\n' + debugLines.map(function (l) {
      return '[' + l.t + '] ' + l.kind + ' ' + l.text;
    }).join('\n');
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

  /* ---- source map: code offsets of an error -> file and line --------------
     Debug builds come with c1.wasm.map. A stack frame such as
     "c1.wasm:wasm-function[499]:0x18ca0" carries the offset in the module;
     the map gives the source line of the code at that offset, including
     code that the compiler inlined into another function. */

  var sourceMap = null;

  function loadSourceMap() {
    if (sourceMap) { return sourceMap; }
    sourceMap = fetch('c1.wasm.map?v=' + encodeURIComponent(BUILD.id)).then(function (res) {
      if (!res.ok) { throw new Error('HTTP ' + res.status); }
      return res.json();
    }).then(function (map) {
      var B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
      var offsets = [], entries = [];
      var col = 0, src = 0, line = 0, scol = 0;
      map.mappings.split(',').forEach(function (seg) {
        if (!seg) { return; }
        var vals = [], value = 0, shift = 0;
        for (var i = 0; i < seg.length; i++) {
          var digit = B64.indexOf(seg[i]);
          value += (digit & 31) << shift;
          if (digit & 32) { shift += 5; continue; }
          vals.push(value & 1 ? -(value >>> 1) : value >>> 1);
          value = 0; shift = 0;
        }
        col += vals[0];
        if (vals.length >= 4) {
          src += vals[1]; line += vals[2]; scol += vals[3];
          offsets.push(col);
          entries.push([src, line + 1]);
        }
      });
      return { sources: map.sources, offsets: offsets, entries: entries };
    });
    return sourceMap;
  }

  /* "file:line" for each wasm frame of a stack, as text lines */
  function resolveStack(stack) {
    var frames = [];
    String(stack || '').split('\n').forEach(function (l) {
      var m = /c1\.wasm[^:]*:wasm-function\[(\d+)\]:0x([0-9a-f]+)/.exec(l);
      if (m) { frames.push({ name: (/at (?:c1\.wasm\.)?([^ (]+)/.exec(l) || [])[1] || 'function ' + m[1], offset: parseInt(m[2], 16) }); }
    });
    if (!frames.length) { return Promise.resolve(''); }
    return loadSourceMap().then(function (map) {
      return frames.map(function (f) {
        var lo = 0, hi = map.offsets.length - 1, best = -1;
        while (lo <= hi) {
          var mid = (lo + hi) >> 1;
          if (map.offsets[mid] <= f.offset) { best = mid; lo = mid + 1; } else { hi = mid - 1; }
        }
        if (best < 0) { return f.name + ': (no line)'; }
        var e = map.entries[best];
        var file = String(map.sources[e[0]] || '?').replace(/^.*?\/src\//, 'src/').replace(/^(\.\.\/)+/, '');
        return f.name + '  ->  ' + file + ':' + e[1];
      }).join('\n');
    }, function (e) {
      return '(no source map in this build: ' + (e && e.message || e) + '. Build with DEBUG=1.)';
    });
  }

  function openReport(title, text) {
    $('report-title').textContent = title;
    $('log').textContent = text;
    $('report').hidden = false;
  }

  /* The first failure is the one that matters; later ones are its fallout. */
  function reportError(title, detail, stack) {
    events.push('[' + stamp() + '] ERROR ' + title);
    debugLine('error', title);
    if (reported) { return; }
    reported = true;
    var text;
    try { text = buildReport(title, detail, stack); }
    catch (e) { text = 'Error: ' + title + '\n' + cleanStack(stack) + '\n(the full report failed: ' + (e && e.message || e) + ')'; }
    openReport('The game stopped unexpectedly', text);
    /* add the source lines when the map has been read */
    resolveStack(stack).then(function (lines) {
      if (!lines) { return; }
      $('log').textContent = $('log').textContent.replace('\n\n--- Engine output', '\n\n--- Source lines ---\n' + lines + '\n\n--- Engine output');
    });
  }

  /* progress bar under the status line: 0..1, or a negative value to hide it */
  function setProgress(fraction) {
    $('progress').hidden = fraction < 0;
    $('progress-bar').style.width = Math.round(Math.max(0, Math.min(1, fraction)) * 100) + '%';
  }

  function showLog(text) {
    note(text);
    openReport('Log', text);
  }

  function setBusy(busy) {
    ['pick-disc', 'pick-zip', 'pick-files'].forEach(function (id) { $(id).classList.toggle('disabled', busy); });
    $('play').disabled = busy;
    $('net-play').disabled = busy;
    if (!busy) { setProgress(-1); }
    $('forget').disabled = busy;
  }

  $('link-close').addEventListener('click', function () { $('link-bar').hidden = true; });
  $('link-open').addEventListener('click', function () { $('link-bar').hidden = true; });
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

  /* ---- app (PWA): service worker, updates, install --------------------- */

  /* The service worker keeps the page working offline. Browsers only allow it
     on https and on localhost; elsewhere (a phone opening the computer's
     local address) the page works as before, online. */
  var standalone = (window.matchMedia && window.matchMedia('(display-mode: standalone)').matches) ||
    (window.matchMedia && window.matchMedia('(display-mode: fullscreen)').matches) || navigator.standalone === true;
  if (standalone) { $('install-help').hidden = true; }
  if (!window.isSecureContext && !standalone) { $('install-local-note').hidden = false; }

  function offerUpdate(worker) {
    $('update-bar').hidden = false;
    $('update-now').onclick = function () {
      $('update-now').disabled = true;
      worker.postMessage({ type: 'SKIP_WAITING' });
    };
  }
  $('update-later').addEventListener('click', function () { $('update-bar').hidden = true; });

  if ('serviceWorker' in navigator && window.isSecureContext) {
    var reloading = false;
    var hadController = !!navigator.serviceWorker.controller;
    navigator.serviceWorker.addEventListener('controllerchange', function () {
      /* the first install takes control silently; a later one is an update
         the player asked for: load the page of the new build */
      if (!hadController) { hadController = true; return; }
      if (reloading) { return; }
      reloading = true;
      location.reload();
    });
    window.addEventListener('load', function () {
      navigator.serviceWorker.register('sw.js').then(function (reg) {
        if (reg.waiting && navigator.serviceWorker.controller) { offerUpdate(reg.waiting); }
        reg.addEventListener('updatefound', function () {
          var w = reg.installing;
          if (!w) { return; }
          w.addEventListener('statechange', function () {
            if (w.state === 'installed' && navigator.serviceWorker.controller) { offerUpdate(w); }
          });
        });
        /* look for a new build now and then while the page stays open */
        setInterval(function () { reg.update().catch(function () {}); }, 30 * 60 * 1000);
      }, function (e) { note('service worker not registered: ' + (e && e.message || e)); });
    });
  }

  /* Chrome and Edge offer their own install prompt; the button uses it */
  var installPrompt = null;
  window.addEventListener('beforeinstallprompt', function (e) {
    e.preventDefault();
    installPrompt = e;
    $('install-app').hidden = false;
  });
  $('install-app').addEventListener('click', function () {
    if (!installPrompt) { return; }
    installPrompt.prompt();
    installPrompt.userChoice.then(function () { installPrompt = null; $('install-app').hidden = true; });
  });
  window.addEventListener('appinstalled', function () { $('install-app').hidden = true; note('installed as an app'); });

  /* ---- hidden diagnostics ------------------------------------------------
     Five taps on the build line, or ?diag in the address. Everything a phone
     cannot show in a console: what the browser supports, what is stored and
     whether it is intact, and whether sound comes out at all. */

  function hexBytes(bytes) {
    return Array.prototype.map.call(bytes, function (b) { return ('0' + b.toString(16)).slice(-2); }).join(' ');
  }

  function fmtMb(n) { return (n / 1048576).toFixed(1) + ' MB'; }

  async function gatherDiagnostics(verifyParts) {
    var out = [];
    var line = function (t) { out.push(t); };
    line('=== c1 web diagnostics ===');
    line('Build: ' + BUILD.id + ' (' + BUILD.commit + ') ' + BUILD.flags);
    line('Page: ' + location.origin + location.pathname);
    line('Browser: ' + navigator.userAgent);
    line('');
    line('--- Browser support ---');
    line('Secure context: ' + (window.isSecureContext ? 'yes' : 'NO (plain http from another machine)'));
    line('SHA-256: ' + (C1Sha256.native ? 'crypto.subtle' : 'JavaScript fallback (no crypto.subtle here)'));
    line('DecompressionStream (compressed packs): ' + (typeof DecompressionStream !== 'undefined' ? 'yes' : 'NO'));
    line('IndexedDB: ' + (window.indexedDB ? 'yes' : 'NO'));
    line('Service worker: ' + (!('serviceWorker' in navigator) ? 'not supported' : !window.isSecureContext ? 'not allowed (not a secure context)' : navigator.serviceWorker.controller ? 'active' : 'not active yet'));
    if (window.caches) { try { line('Caches: ' + ((await caches.keys()).join(', ') || 'none')); } catch (_) { /* n/a */ } }
    line('Installed as an app: ' + (standalone ? 'yes' : 'no'));
    if (navigator.storage && navigator.storage.estimate) {
      try {
        var est = await navigator.storage.estimate();
        line('Storage: ' + fmtMb(est.usage || 0) + ' used of ' + fmtMb(est.quota || 0));
      } catch (e) { line('Storage: estimate failed (' + e.name + ')'); }
      try { line('Storage persisted: ' + (await navigator.storage.persisted() ? 'yes' : 'no (the browser may evict it)')); } catch (_) { /* n/a */ }
    } else {
      line('Storage: estimate not available here');
    }
    line('');
    line('--- Download (data/pack.json) ---');
    if (netPack) {
      line('Pack: ' + netPack.file + ', ' + fmtMb(netPack.size) + ', ' + netPack.parts.length + ' parts, sha ' + netPack.sha256.slice(0, 16) + '…');
    } else {
      line('No pack next to the page (the download option is off).');
    }
    var kept = await C1NetPack.stored(verifyParts);
    if (kept.error) { line('Parts kept: ' + kept.error); }
    else if (!kept.parts.length) { line('Parts kept in this browser: none'); }
    else {
      line('Parts kept in this browser: ' + kept.parts.length);
      kept.parts.forEach(function (p) {
        line('  ' + p.name + '  ' + fmtMb(p.size) + (verifyParts ? (p.ok ? '  checksum ok' : '  CHECKSUM WRONG' + (p.error ? ' (' + p.error + ')' : '')) : ''));
      });
      if (!verifyParts) { line('  (press "Verify stored parts" to check their contents)'); }
    }
    line('');
    line('--- Installed game ---');
    var m = readManifest();
    line('Manifest: ' + (m ? JSON.stringify(m) : 'none'));
    var names = storedAssets().sort();
    var nsf = names.filter(function (n) { return n.slice(-1) === 'f'; });
    var nsd = names.filter(function (n) { return n.slice(-1) === 'd'; });
    var total = 0;
    names.forEach(function (n) { try { total += Module.FS.stat(STREAMS + '/' + n).size; } catch (_) { /* gone */ } });
    line('Files: ' + nsf.length + ' .NSF, ' + nsd.length + ' .NSD, ' + fmtMb(total));
    var unpaired = names.filter(function (n) { return names.indexOf(n.slice(0, -1) + (n.slice(-1) === 'f' ? 'd' : 'f')) < 0; });
    line('Without partner: ' + (unpaired.length ? unpaired.join(' ') : 'none'));
    var badNsf = nsf.filter(function (n) { return !nsfLooksValid(n); });
    line('.NSF files with a wrong start: ' + (badNsf.length ? badNsf.join(' ') : 'none'));
    [TITLE_LEVEL + '.nsf', TITLE_LEVEL + '.nsd', nsf[0], nsf[nsf.length - 1]].filter(function (n, i, all) {
      return n && all.indexOf(n) === i;
    }).forEach(function (n) {
      var b = firstBytes(n, 16);
      var isNsf = n.slice(-1) === 'f';
      line('  ' + n + ': ' + (b.length ? hexBytes(b) : 'MISSING') +
        (isNsf && b.length ? (nsfLooksValid(n) ? '  (page magic ok)' : '  (NOT a page magic)') : ''));
    });
    line('Saved games: ' + Math.max(0, cardSaveCount(readCard())));
    line('');
    line('--- Sound ---');
    var AC = window.AudioContext || window.webkitAudioContext;
    var OAC = window.OfflineAudioContext || window.webkitOfflineAudioContext;
    line('AudioContext: ' + (AC ? 'yes' : 'NO') + '   OfflineAudioContext: ' + (OAC ? 'yes' : 'NO'));
    var sdl = Module.SDL2 && Module.SDL2.audioContext;
    line('Game audio: ' + (sdl ? sdl.state + ' at ' + sdl.sampleRate + ' Hz' : 'not started (it starts with the game)'));
    var probe = document.createElement('audio');
    line('<audio> can play: wav ' + (probe.canPlayType('audio/wav') || 'no') + ', m4a ' + (probe.canPlayType('audio/mp4') || 'no') + ', ogg ' + (probe.canPlayType('audio/ogg') || 'no'));
    if (OAC) {
      /* renders a tone without playing it: a peak of 0 means the audio
         pipeline itself produces silence */
      for (var r = 0; r < 2; r++) {
        var rate = [44100, 22050][r];
        try {
          var off = new OAC(1, rate / 10, rate);
          var osc = off.createOscillator();
          osc.frequency.value = 440;
          osc.connect(off.destination);
          osc.start(0);
          var t0 = performance.now();
          var buf = await new Promise(function (resolve, reject) {
            off.oncomplete = function (e) { resolve(e.renderedBuffer); };
            var p = off.startRendering();
            if (p && p.then) { p.then(resolve, reject); }
          });
          var data = buf.getChannelData(0), peak = 0;
          for (var i = 0; i < data.length; i++) { var a = Math.abs(data[i]); if (a > peak) { peak = a; } }
          line('Offline render at ' + rate + ' Hz: peak ' + peak.toFixed(3) + (peak === 0 ? '  (SILENT!)' : '') + ', ' + Math.round(performance.now() - t0) + ' ms');
        } catch (e) { line('Offline render at ' + rate + ' Hz: failed (' + (e.name || e) + ')'); }
      }
    }
    line('');
    line('--- Graphics ---');
    line('WebGL: ' + safe(glInfo));
    line('Screen: ' + window.innerWidth + 'x' + window.innerHeight + ' @' + window.devicePixelRatio);
    line('');
    line('--- Last install error ---');
    line(lastInstallError || '(none)');
    line('');
    line('--- Page events ---');
    line(events.length ? events.join('\n') : '(none)');
    line('');
    line('--- Engine output ---');
    line(output.length ? output.join('\n') : '(none)');
    return out.join('\n');
  }

  function openDiagnostics(verifyParts) {
    $('diag').hidden = false;
    $('diag-text').textContent = 'Collecting…';
    gatherDiagnostics(verifyParts).then(function (text) { $('diag-text').textContent = text; },
      function (e) { $('diag-text').textContent = 'Diagnostics failed: ' + (e && e.stack || e); });
  }

  /* a short beep, to tell "no sound at all" from "the game is silent" */
  function testToneWebAudio() {
    var AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) { return; }
    var ctx = new AC();
    var osc = ctx.createOscillator(), gain = ctx.createGain();
    osc.frequency.value = 660;
    gain.gain.value = 0.2;
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start();
    osc.stop(ctx.currentTime + 0.4);
    osc.onended = function () { ctx.close(); };
    $('diag-text').textContent += '\n[' + new Date().toLocaleTimeString() + '] Web Audio beep requested, context ' + ctx.state + ' at ' + ctx.sampleRate + ' Hz';
  }
  function testToneElement() {
    var rate = 22050, n = rate * 0.4, wav = new Uint8Array(44 + n * 2), dv = new DataView(wav.buffer);
    var put = function (o, t) { for (var i = 0; i < t.length; i++) { wav[o + i] = t.charCodeAt(i); } };
    put(0, 'RIFF'); dv.setUint32(4, 36 + n * 2, true); put(8, 'WAVEfmt '); dv.setUint32(16, 16, true);
    dv.setUint16(20, 1, true); dv.setUint16(22, 1, true); dv.setUint32(24, rate, true); dv.setUint32(28, rate * 2, true);
    dv.setUint16(32, 2, true); dv.setUint16(34, 16, true); put(36, 'data'); dv.setUint32(40, n * 2, true);
    for (var i = 0; i < n; i++) { dv.setInt16(44 + i * 2, Math.sin(i * 2 * Math.PI * 660 / rate) * 6000, true); }
    var url = URL.createObjectURL(new Blob([wav], { type: 'audio/wav' }));
    var el = new Audio(url);
    var say = function (t) { $('diag-text').textContent += '\n[' + new Date().toLocaleTimeString() + '] <audio> beep: ' + t; };
    el.onended = function () { say('played to the end'); URL.revokeObjectURL(url); };
    el.onerror = function () { say('ERROR ' + (el.error && el.error.code)); };
    var p = el.play();
    if (p && p.catch) { p.catch(function (e) { say('refused (' + e.name + ')'); }); }
  }

  var buildTaps = [];
  $('build-info').addEventListener('click', function () {
    var now = Date.now();
    buildTaps = buildTaps.filter(function (t) { return now - t < 3000; });
    buildTaps.push(now);
    if (buildTaps.length >= 5) { buildTaps = []; openDiagnostics(false); }
  });
  $('diag-close').addEventListener('click', function () { $('diag').hidden = true; });
  $('diag-parts').addEventListener('click', function () { openDiagnostics(true); });
  $('diag-tone').addEventListener('click', testToneWebAudio);
  $('diag-audio').addEventListener('click', testToneElement);
  $('diag-copy').addEventListener('click', function () {
    var btn = this, text = $('diag-text').textContent;
    var done = function () { btn.textContent = 'Copied'; setTimeout(function () { btn.textContent = 'Copy'; }, 1500); };
    if (navigator.clipboard && navigator.clipboard.writeText) { navigator.clipboard.writeText(text).then(done, function () {}); }
    else {
      var range = document.createRange();
      range.selectNodeContents($('diag-text'));
      var sel = window.getSelection();
      sel.removeAllRanges();
      sel.addRange(range);
      try { document.execCommand('copy'); done(); } catch (_) { /* selected: copy by hand */ }
    }
  });

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
    /* called by the engine after it wrote the memory card file */
    c1SaveChanged: function () {
      note('saved games written');
      Module.FS.syncfs(false, function (err) {
        if (err) { debugLine('error', 'could not store the saved games: ' + err); }
      });
    },
    /* read by the engine (web/menulink.c): text of the extra menu entry */
    c1MenuLabel: MENU_LINK.url ? String(MENU_LINK.label || 'WEBSITE').slice(0, 16) : '',
    /* called by the engine when the extra menu entry is chosen */
    c1OpenMenuLink: function () {
      note('menu link chosen');
      var win = null;
      try { win = window.open(MENU_LINK.url, '_blank'); } catch (_) { /* blocked */ }
      if (win) { try { win.opener = null; } catch (_) { /* cross origin */ } return; }
      /* Blocked (a gamepad press does not count as a click for the browser):
         offer the link to click instead. */
      $('link-open').href = MENU_LINK.url;
      $('link-open').textContent = 'Open ' + MENU_LINK.url.replace(/^https?:\/\//, '') + ' ↗';
      $('link-bar').hidden = false;
    },
    onRuntimeInitialized: function () {
      var FS = Module.FS;
      FS.mkdir(STREAMS);
      FS.mkdir(SAVES); /* saved games: the engine's virtual memory card */
      try {
        FS.mount(Module.IDBFS, {}, STREAMS);
        FS.mount(Module.IDBFS, {}, SAVES);
      } catch (e) {
        console.warn('persistent storage unavailable', e);
      }
      FS.syncfs(true, function (err) {
        if (err) { console.warn('could not load stored files', err); }
        var problem = validateInstall();
        refresh(problem);
        checkNetPack().then(function (pack) {
          refresh(problem);
          /* set by "Delete everything and download again" before reloading */
          var auto = false;
          try { auto = sessionStorage.getItem(AUTO_DOWNLOAD) === '1'; sessionStorage.removeItem(AUTO_DOWNLOAD); } catch (_) { /* private mode */ }
          if (auto && pack) { runInstall('net', true, importNet); }
          if (/[?&]diag\b/.test(location.search)) { openDiagnostics(false); }
        });
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

  /* Level to boot: the title sequence unless another one is chosen in the
     Debug section or with ?level=<hex id> in the address. */
  var bootLevel = '';
  try { bootLevel = (new URLSearchParams(location.search).get('level') || '').toLowerCase().replace(/^0x/, ''); } catch (_) { /* none */ }

  var requestedLevel = bootLevel;

  function levelFiles(hexId) {
    var base = 's' + ('0000000' + hexId).slice(-7);
    return [base + '.nsd', base + '.nsf'];
  }

  /* hex ids of the levels that have both of their files stored */
  function storedLevels(names) {
    return names.filter(function (n) { return n.slice(-1) === 'd' && names.indexOf(n.slice(0, -1) + 'f') >= 0; })
      .map(function (n) { return n.slice(1, 8).replace(/^0+(?=.)/, ''); }).sort();
  }

  /* ---- saved games (the engine's virtual memory card, src/pc/card.c) ---- */

  var CARD_FILE = SAVES + '/card.dat';
  var CARD_SIZE = 8 + 15 * 132;
  var CARD_MAGIC = 0x31433143;

  /* number of saves in a card file, or -1 if it is not one */
  function cardSaveCount(data) {
    if (!data || data.length !== CARD_SIZE) { return -1; }
    var dv = new DataView(data.buffer, data.byteOffset, data.length);
    var count = dv.getUint32(4, true);
    return dv.getUint32(0, true) === CARD_MAGIC && count <= 15 ? count : -1;
  }

  function readCard() {
    try { return Module.FS.readFile(CARD_FILE); } catch (_) { return null; }
  }

  function refreshSaves(message) {
    var count = cardSaveCount(readCard());
    $('saves-status').textContent = message ||
      (count > 0 ? count + (count === 1 ? ' saved game' : ' saved games') + ' on the memory card.' : 'No saved games yet.');
    $('saves-export').hidden = $('saves-delete').hidden = !(count > 0);
  }

  $('saves-export').addEventListener('click', function () {
    var data = readCard();
    if (!data) { return; }
    var url = URL.createObjectURL(new Blob([data], { type: 'application/octet-stream' }));
    var a = document.createElement('a');
    a.href = url;
    a.download = 'c1-saves.dat';
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(function () { URL.revokeObjectURL(url); }, 10000);
  });
  $('saves-import').addEventListener('change', function () {
    var picked = this.files[0];
    this.value = '';
    if (!picked) { return; }
    picked.arrayBuffer().then(function (buf) {
      var data = new Uint8Array(buf);
      if (cardSaveCount(data) < 0) { refreshSaves('That file is not a c1 saved games backup.'); return; }
      if (cardSaveCount(readCard()) > 0 && !confirm('Replace the saved games in this browser with the backup?')) { return; }
      Module.FS.writeFile(CARD_FILE, data);
      persist().then(function () { refreshSaves(); });
    });
  });
  $('saves-delete').addEventListener('click', function () {
    if (!confirm('Delete all saved games? This cannot be undone.')) { return; }
    try { Module.FS.unlink(CARD_FILE); } catch (_) { /* nothing to delete */ }
    persist().then(function () { refreshSaves(); });
  });

  var sourcesOpen = false;
  $('sources-toggle').addEventListener('click', function () { sourcesOpen = true; refresh(); });

  function refresh(message) {
    refreshSaves();
    var names = storedAssets();
    var levelsStored = storedLevels(names);
    var titleId = TITLE_LEVEL.slice(1).replace(/^0+/, '');
    /* a level asked for in the address applies as soon as its files are there */
    if (requestedLevel && levelsStored.indexOf(requestedLevel) >= 0) { bootLevel = requestedLevel; requestedLevel = ''; }
    if (levelsStored.indexOf(bootLevel) < 0) { bootLevel = levelsStored.indexOf(titleId) >= 0 || !levelsStored.length ? '' : levelsStored[0]; }
    var bootPick = $('boot-level');
    bootPick.innerHTML = '';
    levelsStored.forEach(function (id) {
      var opt = document.createElement('option');
      opt.value = id === titleId ? '' : id;
      opt.textContent = id === titleId ? 'Title screen (normal start)' : 'Level 0x' + id.toUpperCase();
      bootPick.appendChild(opt);
    });
    bootPick.value = bootLevel;
    $('boot-row').hidden = levelsStored.length < 2 && !bootLevel;
    var hasTitle = bootLevel ? true : levelsStored.indexOf(titleId) >= 0;
    var levels = names.filter(function (n) { return n.slice(-1) === 'f'; }).length;
    $('ready').hidden = !hasTitle;
    /* with the game installed the ways to load it step back behind a button */
    if (!hasTitle) { sourcesOpen = false; }
    $('sources-toggle').hidden = !hasTitle || sourcesOpen;
    $('sources').hidden = hasTitle && !sourcesOpen;
    /* The download stays on offer with a game installed, as an update, unless
       that very pack is what is installed. */
    var manifest = readManifest();
    var samePack = !!(netPack && manifest && manifest.packSha === netPack.sha256);
    $('card-net').hidden = !netPack || (hasTitle && samePack);
    var isUpdate = hasTitle && !!(manifest && manifest.packSha);
    $('net-title').textContent = isUpdate ? 'Update the game' : 'Download the game';
    $('net-play').textContent = isUpdate ? 'Download the update' : 'Download and play';
    $('step-net').hidden = hasTitle;
    $('step-disc').textContent = netPack ? '2' : '1';
    $('step-zip').textContent = netPack ? '3' : '2';
    $('opt-reset').hidden = !netPack;
    $('forget').hidden = names.length === 0;
    $('export-row').hidden = $('export-note').hidden = names.length === 0;
    var pick = $('export-file');
    pick.innerHTML = '';
    names.sort().forEach(function (n) {
      var opt = document.createElement('option');
      opt.value = n;
      opt.textContent = n.toUpperCase() + ' (' + (Module.FS.stat(STREAMS + '/' + n).size / 1048576).toFixed(1) + ' MB)';
      pick.appendChild(opt);
    });
    if (message) {
      setStatus(message, true);
    } else if (hasTitle) {
      setStatus(bootLevel ? 'The game is ready, starting at level 0x' + bootLevel.toUpperCase() + '.' : 'The game is ready to play.');
    } else if (names.length) {
      setStatus('Some game files are missing (the title screen). Load the game again from one of the options.', true);
    } else {
      setStatus('Choose how to load the game.');
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

  /* like persist, but a failure (no space left, broken database) is an error */
  function persistStrict(what) {
    return new Promise(function (resolve, reject) {
      Module.FS.syncfs(false, function (err) {
        if (!err) { resolve(); return; }
        var e = err.target && err.target.error ? err.target.error : err;
        var name = (e && e.name) || 'Error';
        var msg = (e && e.message) || String(e);
        var full = name === 'QuotaExceededError' || /quota|no space/i.test(msg);
        var out = new Error((full ? 'There is not enough free storage in this browser. ' : '') +
          'IndexedDB (' + what + '): ' + name + (msg && msg !== name ? ' – ' + msg : ''));
        out.kind = full ? 'storage' : 'other';
        reject(out);
      });
    });
  }

  /* ---- installation: manifest, validation, cleaning -------------------- */

  /* Written last, and only when an install went through. A game without it
     was interrupted half way. */
  var MANIFEST = STREAMS + '/install.json';
  var IMPORT_VERSION = 1; /* raise when the way files are stored changes */
  var install = null;     /* the install in progress: { source, full, names } */

  function readManifest() {
    try { return JSON.parse(Module.FS.readFile(MANIFEST, { encoding: 'utf8' })); } catch (_) { return null; }
  }
  function removeManifest() {
    try { Module.FS.unlink(MANIFEST); } catch (_) { /* none */ }
  }
  function wipeAssets() {
    storedAssets().forEach(function (n) { try { Module.FS.unlink(STREAMS + '/' + n); } catch (_) { /* gone */ } });
    removeManifest();
  }

  function firstBytes(name, count) {
    var buf = new Uint8Array(count);
    try {
      var st = Module.FS.open(STREAMS + '/' + name, 'r');
      var got = Module.FS.read(st, buf, 0, count, 0);
      Module.FS.close(st);
      return buf.subarray(0, got);
    } catch (_) { return new Uint8Array(0); }
  }
  /* an .NSF starts with a page: magic 0x1234 (plain) or 0x1235 (compressed) */
  function nsfLooksValid(name) {
    var b = firstBytes(name, 2);
    return b.length === 2 && b[1] === 0x12 && (b[0] === 0x34 || b[0] === 0x35);
  }

  function store(name, data) {
    /* a full install replaces the previous one: its manifest goes first, so
       that a game left half written is recognised as such */
    if (install && install.full && !install.started) { install.started = true; removeManifest(); }
    Module.FS.writeFile(STREAMS + '/' + name, data);
    if (install) { install.names[name] = true; }
  }

  function writeManifest(source, extra) {
    var names = storedAssets();
    var m = {
      format: 'c1-web-install',
      complete: true,
      importVersion: IMPORT_VERSION,
      source: source,
      /* the files really stored, not the entries the source claimed to have */
      files: install && install.full ? Object.keys(install.names).length : names.length,
      packSha: (extra && extra.packSha) || null,
      installed: new Date().toISOString()
    };
    Module.FS.writeFile(MANIFEST, JSON.stringify(m));
    return m;
  }

  /* Checks the stored game when the page starts. Returns '' when it is fine
     (or absent), otherwise the reason, after deleting what was there. */
  function validateInstall() {
    var names = storedAssets();
    var m = readManifest();
    if (!names.length) { if (m) { removeManifest(); } return ''; }
    var titleId = TITLE_LEVEL.slice(1).replace(/^0+/, '');
    var levels = storedLevels(names);
    var nsfs = names.filter(function (n) { return n.slice(-1) === 'f'; }).sort();
    var problem = '';
    if (!m) {
      /* Installs made before the manifest existed: accepted when they look
         whole (every file has its partner and every .NSF starts as one). */
      if (levels.length && names.length === levels.length * 2 && nsfs.every(nsfLooksValid)) {
        install = null;
        var legacy = writeManifest('legacy');
        note('older install accepted: ' + legacy.files + ' files');
        persist();
        return '';
      }
      problem = 'it was not completed';
    } else if (!m.complete) {
      problem = 'it was not completed';
    } else if (names.length < m.files) {
      problem = names.length + ' of ' + m.files + ' files are there';
    } else if (m.source !== 'files' && levels.indexOf(titleId) < 0) {
      problem = 'the title screen is missing';
    } else {
      var probe = [TITLE_LEVEL + '.nsf', nsfs[0], nsfs[nsfs.length - 1]].filter(function (n, i, all) {
        return n && names.indexOf(n) >= 0 && all.indexOf(n) === i;
      });
      var bad = probe.filter(function (n) { return !nsfLooksValid(n); });
      if (bad.length) { problem = bad[0].toUpperCase() + ' is damaged'; }
    }
    if (!problem) { return ''; }
    note('stored game removed: ' + problem);
    wipeAssets();
    persist();
    return 'The game stored in this browser was incomplete or damaged (' + problem + '). Install it again.';
  }

  /* Runs one way of installing the game. `full` installs replace the game;
     the loose files option only adds to it. */
  function runInstall(source, full, work) {
    setBusy(true);
    hideInstallError();
    install = { source: source, full: full, names: {}, started: false };
    lastInstall = { source: source, work: work, full: full };
    return Promise.resolve().then(work).then(function (extra) {
      writeManifest(source, extra);
      return persistStrict('saving the game');
    }).then(function () {
      install = null;
      note('installed from ' + source);
      refresh();
    }, function (err) {
      console.error(err);
      /* nothing usable is left of a full install that had started writing */
      var wrote = install && install.full && install.started;
      install = null;
      if (wrote) { wipeAssets(); }
      note('install from ' + source + ' failed: ' + (err && err.message || err));
      return persist().then(function () { refresh(); showInstallError(err, source); });
    });
  }

  /* ---- install error panel --------------------------------------------- */

  var lastInstall = null;
  var lastInstallError = '';

  function hideInstallError() { $('install-error').hidden = true; }

  function showInstallError(err, source) {
    var kind = (err && err.kind) || 'other';
    lastInstallError = (err && err.message) || String(err);
    $('install-error-text').textContent = lastInstallError;
    /* An interrupted download carries on from the parts it has: retry first.
       Anything else is safer started clean, when there is a download. */
    var canRetry = !!lastInstall && source === 'net';
    $('install-retry').hidden = !canRetry;
    $('install-reset').hidden = !netPack;
    $('install-retry').classList.toggle('primary', kind === 'network' || !netPack);
    $('install-reset').classList.toggle('primary', kind !== 'network' && !!netPack);
    $('install-error').hidden = false;
    setStatus('');
  }

  $('install-dismiss').addEventListener('click', hideInstallError);
  $('install-retry').addEventListener('click', function () {
    if (lastInstall) { runInstall(lastInstall.source, lastInstall.full, lastInstall.work); }
  });

  function deleteDatabase(name) {
    return new Promise(function (resolve) {
      var r;
      try { r = indexedDB.deleteDatabase(name); } catch (_) { resolve(); return; }
      /* a broken or still open database must not hold this up */
      r.onsuccess = r.onerror = r.onblocked = function () { resolve(); };
      setTimeout(resolve, 3000);
    });
  }

  /* Removes the game, the downloaded parts and every cache, keeps the saved
     games and the options, then reloads and starts the download by itself. */
  var AUTO_DOWNLOAD = 'c1web.autoDownload';
  function resetAndDownload() {
    setBusy(true);
    setStatus('Deleting the stored game…');
    try { wipeAssets(); } catch (_) { /* the file system may be the broken part */ }
    persist().then(function () {
      return Promise.all([deleteDatabase(STREAMS), deleteDatabase(C1NetPack.DB_NAME)]);
    }).then(function () {
      if (!window.caches || !caches.keys) { return null; }
      return caches.keys().then(function (keys) {
        return Promise.all(keys.map(function (k) { return caches.delete(k); }));
      }).catch(function () {});
    }).then(function () {
      try { sessionStorage.setItem(AUTO_DOWNLOAD, '1'); } catch (_) { /* private mode */ }
      location.reload();
    });
  }
  $('install-reset').addEventListener('click', resetAndDownload);
  $('opt-reset').addEventListener('click', function () {
    if (confirm('Delete the game stored in this browser and download it again? Saved games are kept.')) { resetAndDownload(); }
  });

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
      setStatus('Copying the game from the disc… ' + Math.floor((i + 1) * 100 / found.length) + '%');
      setProgress((i + 1) / found.length);
      store(found[i].name, await disc.readExtent(found[i].lba, found[i].size));
    }
    setStatus('Storing the game…');
  }

  /* ---- import: download in parts (tools/pack/split_pack.py) ------------- */

  var netPack = null;

  /* The option only exists when the build was given a split pack (data/). */
  function checkNetPack() {
    return C1NetPack.info().then(function (pack) {
      netPack = pack;
      if (pack) { $('net-size').textContent = '(' + Math.round(pack.size / 1048576) + ' MB)'; }
      return pack;
    });
  }

  async function importNet() {
    setStatus('Starting the download…');
    setProgress(0);
    var pack = netPack;
    await C1NetPack.downloadAndImport(pack, store, function (p) {
      var fraction = p.done / Math.max(p.total, 1);
      if (p.phase === 'download') {
        setStatus('Downloading the game… part ' + p.part + ' of ' + p.parts);
        setProgress(fraction * 0.8);
      } else {
        setStatus('Installing the game… ' + Math.floor(fraction * 100) + '%');
        setProgress(0.8 + fraction * 0.2);
      }
    });
    setStatus('Storing the game…');
    return { packSha: pack.sha256 };
  }

  $('net-play').addEventListener('click', function () {
    if (netPack) { runInstall('net', true, importNet); }
  });

  /* ---- import: game ZIP (tools/pack/make_c1_pack.py) ------------------- */

  async function importZip(file) {
    setStatus('Reading game ZIP…');
    var result = await C1ZipPack.importPack(file, store, function (p) {
      setStatus('Installing the game… ' + Math.floor(p.done * 100 / Math.max(p.total, 1)) + '%');
      setProgress(p.done / Math.max(p.total, 1));
    });
    if (result.serial && result.serial !== 'SCUS-94900') {
      console.warn('pack made from ' + result.serial + '; c1 expects SCUS-94900');
    }
    setStatus('Storing the game…');
  }

  $('disc').addEventListener('change', function () {
    var picked = this.files[0];
    if (picked) { runInstall('disc', true, function () { return importDisc(picked); }); }
    this.value = '';
  });
  $('zip').addEventListener('change', function () {
    var picked = this.files[0];
    if (picked) { runInstall('zip', true, function () { return importZip(picked); }); }
    this.value = '';
  });
  $('files').addEventListener('change', function () {
    /* copy the list: clearing the input below empties the live FileList */
    var list = Array.prototype.slice.call(this.files);
    if (list.length) { runInstall('files', false, function () { return importFiles(list); }); }
    this.value = '';
  });
  $('boot-level').addEventListener('change', function () { bootLevel = this.value; refresh(); });
  $('export-save').addEventListener('click', function () {
    var name = $('export-file').value;
    if (!name) { return; }
    var url = URL.createObjectURL(new Blob([Module.FS.readFile(STREAMS + '/' + name)], { type: 'application/octet-stream' }));
    var a = document.createElement('a');
    a.href = url;
    a.download = name.toUpperCase();
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(function () { URL.revokeObjectURL(url); }, 10000);
  });
  $('forget').addEventListener('click', function () {
    if (!confirm('Remove the game from this browser? Saved games are kept.')) { return; }
    wipeAssets();
    C1NetPack.reset();
    hideInstallError();
    setBusy(true);
    persist().then(function () { refresh(); });
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

  /* ---- keyboard bindings ---------------------------------------------- */
  /* The page reads the keyboard itself and presses the engine's pad keys, so
     any key can be bound to any pad button. Bindings are KeyboardEvent.code
     values (physical keys, independent of the keyboard language). */

  var BINDS_KEY = 'c1web.keys.v1';
  var BUTTON_LABELS = [
    ['up', 'D-pad up'], ['down', 'D-pad down'], ['left', 'D-pad left'], ['right', 'D-pad right'],
    ['cross', '✕ Jump'], ['square', '□ Spin'], ['circle', '○ Spin'], ['triangle', '△'],
    ['start', 'Start'], ['select', 'Select'],
    ['l1', 'L1'], ['r1', 'R1'], ['l2', 'L2'], ['r2', 'R2']
  ];
  var DEFAULT_BINDS = {
    up: 'ArrowUp', down: 'ArrowDown', left: 'ArrowLeft', right: 'ArrowRight',
    cross: 'KeyZ', square: 'KeyX', circle: 'KeyC', triangle: 'KeyS',
    start: 'Enter', select: 'Space',
    l1: 'KeyA', r1: 'KeyD', l2: 'KeyQ', r2: 'KeyE'
  };
  var RESERVED_CODES = ['Escape', 'F2']; /* cancel a binding / toggle the debug panel */
  var binds = Object.assign({}, DEFAULT_BINDS);
  try { Object.assign(binds, JSON.parse(localStorage.getItem(BINDS_KEY) || '{}')); } catch (_) { /* defaults */ }
  var waitingFor = null; /* pad button whose key is being chosen */

  function saveBinds() {
    try { localStorage.setItem(BINDS_KEY, JSON.stringify(binds)); } catch (_) { /* ignore */ }
  }

  function keyName(code) {
    if (!code) { return 'not set'; }
    var arrows = { ArrowUp: '↑', ArrowDown: '↓', ArrowLeft: '←', ArrowRight: '→' };
    if (arrows[code]) { return arrows[code]; }
    var m = /^Key([A-Z])$/.exec(code) || /^Digit(\d)$/.exec(code);
    if (m) { return m[1]; }
    m = /^Numpad(.+)$/.exec(code);
    if (m) { return 'Num ' + m[1]; }
    return code.replace(/(Left|Right)$/, ' $1').replace(/([a-z])([A-Z])/g, '$1 $2');
  }

  function buttonForCode(code) {
    for (var btn in binds) { if (binds[btn] === code) { return btn; } }
    return null;
  }

  function renderBinds() {
    var box = $('keys');
    box.innerHTML = '';
    BUTTON_LABELS.forEach(function (pair) {
      var row = document.createElement('div');
      row.className = 'keyrow';
      var name = document.createElement('span');
      name.textContent = pair[1];
      var key = document.createElement('button');
      key.className = 'keybind' + (waitingFor === pair[0] ? ' waiting' : '') + (binds[pair[0]] ? '' : ' unset');
      key.textContent = waitingFor === pair[0] ? 'Press a key…' : keyName(binds[pair[0]]);
      key.setAttribute('aria-label', pair[1] + ': ' + keyName(binds[pair[0]]) + '. Change');
      key.addEventListener('click', function () {
        waitingFor = waitingFor === pair[0] ? null : pair[0];
        renderBinds();
        this.blur();
      });
      row.appendChild(name);
      row.appendChild(key);
      box.appendChild(row);
    });
  }

  function assignBind(btn, code) {
    /* a key drives one button: taking it from another leaves that one unset */
    for (var other in binds) { if (other !== btn && binds[other] === code) { binds[other] = ''; } }
    binds[btn] = code;
    saveBinds();
  }

  function releaseKeyboard() {
    for (var key in KEYS) { setKey('kbd', key, false); }
  }

  function isTypingTarget(el) {
    return el && (el.tagName === 'INPUT' || el.tagName === 'SELECT' || el.tagName === 'TEXTAREA');
  }

  /* Capture phase and registered before the engine starts, so these run before
     SDL's own keyboard handlers, which must not see the keys (the engine's
     built-in key layout would act on top of the chosen one). */
  function onKey(e) {
    var down = e.type === 'keydown';
    if (waitingFor) {
      e.preventDefault();
      e.stopImmediatePropagation();
      if (!down) { return; }
      if (e.code !== 'Escape' && RESERVED_CODES.indexOf(e.code) < 0 && e.code) { assignBind(waitingFor, e.code); }
      waitingFor = null;
      renderBinds();
      return;
    }
    if (!playing) { return; }
    e.stopImmediatePropagation();
    if (down && e.code === 'F2') { e.preventDefault(); toggleDebug(); return; }
    if (isTypingTarget(e.target) || e.metaKey || e.ctrlKey || e.altKey) { return; }
    var btn = buttonForCode(e.code);
    if (!btn) { return; }
    e.preventDefault();
    if (down && !e.repeat) { physicalInput(); resumeAudio(); }
    setKey('kbd', btn, down);
  }
  window.addEventListener('keydown', onKey, true);
  window.addEventListener('keyup', onKey, true);
  /* keys held while the window loses focus never get their keyup */
  window.addEventListener('blur', function () { if (playing) { releaseKeyboard(); } });

  $('keys-reset').addEventListener('click', function () {
    binds = Object.assign({}, DEFAULT_BINDS);
    waitingFor = null;
    saveBinds();
    renderBinds();
  });
  renderBinds();

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
  var prefs = { touch: 'auto', debug: false, volume: 100, aspect: 'fill', res: 0, aa: 1 };
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

  /* ---- debug panel: live state ------------------------------------------ */

  var debugTimer = 0;
  var last = { frames: 0, at: 0, lid: -1, paused: -1, crash: -1, audio: '', canvas: '', slowAt: 0 };
  var workMax = 0;

  function hex(n) { return '0x' + (n >>> 0).toString(16).toUpperCase(); }

  function heldNames() {
    var names = {};
    Object.keys(held).forEach(function (k) { if (held[k]) { names[k.split(':')[1]] = true; } });
    return Object.keys(names).join(' ') || '-';
  }

  /* Runs while playing, panel open or not, so the log also records what
     happened before the panel was opened. */
  function pollDebug() {
    var d;
    try {
      var ptr = Module._WebDebugInfo();
      d = Module.HEAP32.subarray(ptr >> 2, (ptr >> 2) + 24);
    } catch (e) { return; }
    var now = performance.now();
    var fps = last.at ? (d[0] - last.frames) * 1000 / (now - last.at) : 0;
    last.frames = d[0];
    last.at = now;
    var work = d[2] / 1000;
    if (work > workMax) { workMax = work; }

    if (d[3] !== last.lid) { debugLine('game', 'level ' + hex(d[3]) + (last.lid >= 0 ? ' (was ' + hex(last.lid) + ')' : '')); last.lid = d[3]; }
    if (d[5] !== last.paused) { if (last.paused >= 0) { debugLine('game', d[5] ? 'paused' : 'resumed'); } last.paused = d[5]; }
    if (d[10] !== last.crash) { if (last.crash >= 0) { debugLine('game', d[10] ? 'player object created' : 'player object removed'); } last.crash = d[10]; }
    /* a frame has 33 ms: report work that does not fit, at most every 2 s */
    if (work > 33 && now - last.slowAt > 2000) { debugLine('warn', 'slow frame: ' + work.toFixed(1) + ' ms of work'); last.slowAt = now; }
    var ctx = Module.SDL2 && Module.SDL2.audioContext;
    var audio = ctx ? ctx.state + ' ' + ctx.sampleRate + ' Hz' : 'not started';
    if (audio !== last.audio) { debugLine('page', 'audio ' + audio); last.audio = audio; }
    var c = $('canvas');
    var canvas = c.width + 'x' + c.height;
    if (canvas !== last.canvas) { debugLine('page', 'canvas ' + canvas + ' @' + window.devicePixelRatio); last.canvas = canvas; }

    updateCredit(d[16] === 5);
    if ($('debug').hidden) { return; }
    $('debug-stats').textContent = [
      'FPS ' + fps.toFixed(1) + '   work ' + work.toFixed(1) + ' ms (max ' + workMax.toFixed(1) + ')',
      'Level ' + hex(d[3]) + '   frame ' + d[0] + '   draw ' + d[4] + (d[5] ? '   PAUSED' : ''),
      'Tris ' + d[1] + '   ticks/frame ' + d[6] + '   peak bounds ' + d[17] + '/96 results ' + d[18] + '/512',
      'Lives ' + d[7] + '   Fruit ' + d[8] + '   Boxes ' + d[9],
      'Crash ' + (d[10] ? d[11] + ', ' + d[12] + ', ' + d[13] : '(no player object)'),
      'Pad ' + heldNames(),
      'Render ' + d[14] + 'x' + d[15] + '   Canvas ' + canvas + ' @' + window.devicePixelRatio,
      'Mem ' + (Module.HEAP8.length / 1048576).toFixed(0) + ' MB',
      'Audio ' + audio,
      'Build ' + BUILD.id + ' ' + BUILD.commit
    ].join('\n');
  }

  /* The credit sits over the bottom right corner of the picture while the
     game's title menu is on screen. */
  function updateCredit(onMenu) {
    var el = $('credit');
    if (!onMenu || !CREDIT) { el.hidden = true; return; }
    var r = $('canvas').getBoundingClientRect();
    /* the picture ends 12 of its 240 rows above the bottom of the canvas */
    el.textContent = CREDIT;
    el.style.fontSize = Math.max(9, Math.round(r.height * 0.026)) + 'px';
    el.style.right = Math.round(window.innerWidth - r.right + r.width * 0.02) + 'px';
    el.style.bottom = Math.round(window.innerHeight - r.bottom + r.height * 0.075) + 'px';
    el.hidden = false;
  }

  function applyDebug() {
    var on = !!prefs.debug && playing;
    document.body.classList.toggle('debug-on', on);
    $('debug').hidden = !on;
    if (on) {
      var box = $('debug-log');
      box.innerHTML = '';
      debugLines.forEach(function (l) { appendDebugLine(box, l); });
      box.scrollTop = box.scrollHeight;
      workMax = 0;
    }
    settle();
  }

  function toggleDebug() {
    prefs.debug = !prefs.debug;
    savePrefs();
    $('opt-debug-start').checked = prefs.debug;
    applyDebug();
  }

  $('opt-debug-start').checked = !!prefs.debug;
  $('opt-debug-start').addEventListener('change', function () { prefs.debug = this.checked; savePrefs(); });
  $('debug-close').addEventListener('click', toggleDebug);
  $('debug-clear').addEventListener('click', function () { debugLines = []; $('debug-log').innerHTML = ''; workMax = 0; });
  $('debug-copy').addEventListener('click', function () {
    var btn = this;
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(debugText()).then(function () {
        btn.textContent = 'Copied';
        setTimeout(function () { btn.textContent = 'Copy'; }, 1500);
      }, function () {});
    }
  });

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

  /* master volume, in percent of the game's own mix (up to 500%) */
  function applyVolume() {
    var v = Math.max(0, Math.min(500, +prefs.volume || 0));
    $('opt-volume').value = v;
    $('opt-volume-value').textContent = v + '%';
    try { Module._WebSetVolume(v); } catch (_) { /* engine not ready */ }
  }

  /* Picture modes:
     fill    - the canvas takes the whole window and the engine shows more of
               the scene at the sides (nothing is stretched);
     stretch - the canvas takes the whole window and the 4:3 picture is
               stretched over it;
     4:3     - the original picture, as large as fits. */
  function applyAspect() {
    var mode = ['fill', 'stretch', '4:3'].indexOf(prefs.aspect) >= 0 ? prefs.aspect : 'fill';
    $('opt-aspect').value = mode;
    document.body.classList.toggle('wide', mode !== '4:3' && playing);
    try { Module._WebSetWide(mode === 'fill' ? 1 : 0); } catch (_) { /* engine not ready */ }
    settle();
  }

  /* internal resolution (a multiple of the original 240 rows, 0 = the size
     of the window) and antialiasing (supersampling) */
  function applyRender() {
    var res = [0, 1, 2, 3, 4].indexOf(+prefs.res) >= 0 ? +prefs.res : 0;
    var aa = prefs.aa === 0 || prefs.aa === '0' ? 0 : 1;
    $('opt-res').value = String(res);
    $('opt-aa').value = String(aa);
    try { Module._WebSetRender(res, aa); } catch (_) { /* engine not ready */ }
  }

  function openMenu() {
    applyVolume();
    applyRender();
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
    $('opt-volume').addEventListener('input', function () {
      prefs.volume = +this.value;
      savePrefs();
      applyVolume();
    });
    $('opt-aspect').addEventListener('change', function () {
      prefs.aspect = this.value;
      savePrefs();
      applyAspect();
    });
    $('opt-res').addEventListener('change', function () { prefs.res = +this.value; savePrefs(); applyRender(); });
    $('opt-aa').addEventListener('change', function () { prefs.aa = +this.value; savePrefs(); applyRender(); });
    $('opt-edit').addEventListener('click', openTouchEditor);
    $('opt-debug').addEventListener('click', function () { closeMenu(); toggleDebug(); });
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

    window.addEventListener('pointerdown', function (e) {
      if (e.pointerType === 'touch' && touchHiddenByPhysical) {
        touchHiddenByPhysical = false;
        updateTouchVisibility();
      }
      resumeAudio();
    }, true);
    document.addEventListener('visibilitychange', function () {
      note('tab ' + document.visibilityState);
      if (document.visibilityState === 'visible') { resumeAudio(); } else { releaseKeyboard(); }
    });
    window.addEventListener('gamepadconnected', function (e) { note('gamepad connected: ' + e.gamepad.id); });
    window.addEventListener('gamepaddisconnected', function (e) { note('gamepad disconnected: ' + e.gamepad.id); });
    $('canvas').addEventListener('webglcontextlost', function () { debugLine('error', 'WebGL context lost'); });
    $('canvas').addEventListener('webglcontextrestored', function () { debugLine('warn', 'WebGL context restored'); });

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
    waitingFor = null;
    wireGameUi();
    applyVolume();
    applyAspect();
    applyRender();
    applyDebug();
    debugTimer = setInterval(pollDebug, 250);
    updateTouchVisibility();
    requestAnimationFrame(pollGamepads);
    /* Android: go fullscreen with the tap that starts the game (iOS has no
       fullscreen for pages; there the home-screen app is the way). */
    if (!isIOS && isTouchDevice && document.documentElement.requestFullscreen) {
      document.documentElement.requestFullscreen({ navigationUI: 'hide' }).catch(function () {});
    }
    try {
      if (bootLevel) { note('booting level 0x' + bootLevel); }
      Module.callMain(bootLevel ? [bootLevel] : []);
      note('engine started (main is running)');
    } catch (e) {
      reportError((e && e.name ? e.name + ': ' : '') + (e && e.message || String(e)), 'thrown while starting', e && e.stack);
    }
    /* SDL sets its own canvas size when it creates the window */
    settle();
  });
})();
