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

  function showLog(text) {
    var el = $('log');
    el.hidden = false;
    el.textContent += text + '\n';
    el.scrollTop = el.scrollHeight;
  }

  function setBusy(busy) {
    ['pick-disc', 'pick-files'].forEach(function (id) { $(id).classList.toggle('disabled', busy); });
    $('play').disabled = busy;
    $('forget').disabled = busy;
  }

  /* ---- emscripten module ------------------------------------------- */

  window.Module = {
    canvas: $('canvas'),
    print: function (text) { console.log(text); },
    printErr: function (text) { console.warn(text); },
    onAbort: function (what) {
      showLog('The game stopped unexpectedly: ' + what);
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
    if (started) { showLog('Error: ' + (e.message || e)); }
  });
  window.addEventListener('unhandledrejection', function (e) {
    if (started) { showLog('Error: ' + (e.reason && e.reason.message || e.reason)); }
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

  /* ---- input: touch and gamepad ------------------------------------ */

  var held = {}; /* source:key -> bool, so sources do not release each other's keys */

  function setKey(source, key, down) {
    var id = source + ':' + key;
    if (!!held[id] === down) { return; }
    held[id] = down;
    var any = Object.keys(held).some(function (k) { return held[k] && k.split(':')[1] === key; });
    Module._WebSetKey(KEYS[key], any ? 1 : 0);
  }

  function setupTouch() {
    if (!('ontouchstart' in window) && !(navigator.maxTouchPoints > 0)) { return; }
    $('touch').hidden = false;
    Array.prototype.forEach.call(document.querySelectorAll('#touch button'), function (btn) {
      var key = btn.dataset.key;
      var press = function (down) {
        return function (e) {
          e.preventDefault();
          btn.classList.toggle('on', down);
          setKey('touch', key, down);
        };
      };
      btn.addEventListener('pointerdown', press(true));
      btn.addEventListener('pointerup', press(false));
      btn.addEventListener('pointercancel', press(false));
      btn.addEventListener('pointerleave', press(false));
      btn.addEventListener('contextmenu', function (e) { e.preventDefault(); });
    });
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
    for (var p = 0; p < pads.length; p++) {
      var pad = pads[p];
      if (!pad) { continue; }
      for (var b in PAD_BUTTONS) {
        if (pad.buttons[b] && pad.buttons[b].pressed) { state[PAD_BUTTONS[b]] = true; }
      }
      if (pad.axes.length >= 2) {
        if (pad.axes[0] < -0.5) { state.left = true; }
        if (pad.axes[0] > 0.5) { state.right = true; }
        if (pad.axes[1] < -0.5) { state.up = true; }
        if (pad.axes[1] > 0.5) { state.down = true; }
      }
    }
    for (var key in KEYS) { setKey('pad', key, !!state[key]); }
    requestAnimationFrame(pollGamepads);
  }

  /* ---- start -------------------------------------------------------- */

  $('play').addEventListener('click', function () {
    if (started) { return; }
    started = true;
    $('setup').hidden = true;
    $('game').hidden = false;
    setupTouch();
    requestAnimationFrame(pollGamepads);
    Module.callMain([]);
  });

  $('fullscreen').addEventListener('click', function () {
    var el = $('game');
    if (document.fullscreenElement) { document.exitFullscreen(); }
    else if (el.requestFullscreen) { el.requestFullscreen(); }
    this.blur();
  });
})();
