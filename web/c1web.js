/* Front end of the c1 web build: asset import, storage, input helpers. */
'use strict';

(function () {
  var $ = function (id) { return document.getElementById(id); };
  /* Extra entry of the game's own title menu, below OPTIONS: choosing it
     opens this address in a new tab. `label` is drawn with the game's font:
     up to 16 characters, capital letters, digits and spaces. An empty url
     removes the entry. */
  var MENU_LINK = { label: 'WEBSITE', url: 'https://github.com/gabom88/crash1' };

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
  }

  function showLog(text) {
    note(text);
    openReport('Log', text);
  }

  function setBusy(busy) {
    ['pick-disc', 'pick-zip', 'pick-files'].forEach(function (id) { $(id).classList.toggle('disabled', busy); });
    $('play').disabled = busy;
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
    $('play').hidden = !hasTitle;
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
      setStatus('Game files ready (' + levels + ' level files stored)' + (bootLevel ? ', starting at level 0x' + bootLevel.toUpperCase() + '.' : '.'));
    } else if (names.length) {
      setStatus('The title level (' + TITLE_LEVEL.toUpperCase() + '.NSF/.NSD) is missing. Add the rest of the files.', true);
    } else {
      setStatus('Choose your disc image, a game ZIP or the .NSF/.NSD files to begin.');
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

  /* ---- import: game ZIP (tools/pack/make_c1_pack.py) ------------------- */

  async function importZip(file) {
    setStatus('Reading game ZIP…');
    var result = await C1ZipPack.importPack(file, store, function (p) {
      if (p.current) {
        setStatus('Unpacking game files… ' + Math.floor(p.done * 100 / Math.max(p.total, 1)) + '%  (' + p.current.toUpperCase() + ')');
      }
    });
    if (result.serial && result.serial !== 'SCUS-94900') {
      console.warn('pack made from ' + result.serial + '; c1 expects SCUS-94900');
    }
    setStatus('Storing game files…');
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
  $('zip').addEventListener('change', function () {
    if (this.files.length) { runImport(importZip(this.files[0])); }
    this.value = '';
  });
  $('files').addEventListener('change', function () {
    /* copy the list: clearing the input below empties the live FileList */
    if (this.files.length) { runImport(importFiles(Array.prototype.slice.call(this.files))); }
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
      d = Module.HEAP32.subarray(ptr >> 2, (ptr >> 2) + 16);
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

    if ($('debug').hidden) { return; }
    $('debug-stats').textContent = [
      'FPS ' + fps.toFixed(1) + '   work ' + work.toFixed(1) + ' ms (max ' + workMax.toFixed(1) + ')',
      'Level ' + hex(d[3]) + '   frame ' + d[0] + '   draw ' + d[4] + (d[5] ? '   PAUSED' : ''),
      'Tris ' + d[1] + '   ticks/frame ' + d[6],
      'Lives ' + d[7] + '   Fruit ' + d[8] + '   Boxes ' + d[9],
      'Crash ' + (d[10] ? d[11] + ', ' + d[12] + ', ' + d[13] : '(no player object)'),
      'Pad ' + heldNames(),
      'Render ' + d[14] + 'x' + d[15] + '   Canvas ' + canvas + ' @' + window.devicePixelRatio,
      'Mem ' + (Module.HEAP8.length / 1048576).toFixed(0) + ' MB',
      'Audio ' + audio,
      'Build ' + BUILD.id + ' ' + BUILD.commit
    ].join('\n');
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
