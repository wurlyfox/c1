// zippack.js — reads a c1 web pack made by tools/pack/make_c1_pack.py.
//
// Adapted from ReChan Web (https://github.com/gabom88/ReChan,
// web/shell/js/zippack.js). The ZIP is read in place with File.slice(), one
// entry at a time, so the whole pack never has to fit in memory; compressed
// entries are inflated with the browser's DecompressionStream.
'use strict';

const C1ZipPack = (() => {
  const FORMAT = 'c1-web-pack';
  const ASSET_NAME = /^s[0-9a-f]{7}\.ns[fd]$/;

  class PackError extends Error {}

  async function bytes(file, start, end) {
    return new Uint8Array(await file.slice(start, end).arrayBuffer());
  }
  const u16 = (b, o) => b[o] | (b[o + 1] << 8);
  const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;

  async function readDirectory(file) {
    const tailSize = Math.min(file.size, 65536 + 22);
    const tail = await bytes(file, file.size - tailSize, file.size);
    let eocd = -1;
    for (let i = tail.length - 22; i >= 0; i--) {
      if (u32(tail, i) === 0x06054b50) { eocd = i; break; }
    }
    if (eocd < 0) throw new PackError('This file is not a ZIP.');
    const count = u16(tail, eocd + 10);
    const cdSize = u32(tail, eocd + 12);
    const cdOffset = u32(tail, eocd + 16);
    if (count === 0xFFFF || cdOffset === 0xFFFFFFFF) throw new PackError('ZIP64 files are not supported.');
    const cd = await bytes(file, cdOffset, cdOffset + cdSize);
    const entries = [];
    const dec = new TextDecoder();
    let p = 0;
    for (let i = 0; i < count; i++) {
      if (u32(cd, p) !== 0x02014b50) throw new PackError('The ZIP is damaged.');
      const nameLen = u16(cd, p + 28);
      entries.push({
        name: dec.decode(cd.subarray(p + 46, p + 46 + nameLen)),
        method: u16(cd, p + 10),
        compSize: u32(cd, p + 20),
        size: u32(cd, p + 24),
        localOffset: u32(cd, p + 42),
      });
      p += 46 + nameLen + u16(cd, p + 30) + u16(cd, p + 32);
    }
    return entries;
  }

  // Reads the pack through a 4 MB window: the 30 byte header of every entry
  // and the whole of the small entries then cost a handful of reads instead
  // of two each. Every read of a big Blob is slow on iPhone, where it lives
  // on disk (in IndexedDB).
  const WINDOW = 4 * 1024 * 1024;
  function windowReader(file) {
    let wStart = 0, wData = null;
    return async (start, end) => {
      if (!wData || start < wStart || end > wStart + wData.length) {
        wStart = start;
        wData = await bytes(file, start, Math.min(file.size, Math.max(end, start + WINDOW)));
      }
      return wData.subarray(start - wStart, end - wStart);
    };
  }

  // Entries up to this size are taken from the window.
  const SMALL = 256 * 1024;

  // Always returns its own copy of the bytes, never a slice of the pack:
  // Safari on iOS stores a slice of a disk-backed Blob (the downloaded pack
  // is one, joined from the parts kept in IndexedDB) with the right size but
  // the wrong data.
  async function entryBytes(file, e, read) {
    const lh = read ? await read(e.localOffset, e.localOffset + 30) : await bytes(file, e.localOffset, e.localOffset + 30);
    if (u32(lh, 0) !== 0x04034b50) throw new PackError('The ZIP is damaged.');
    const start = e.localOffset + 30 + u16(lh, 26) + u16(lh, 28);
    let data;
    if (e.method === 0) {
      data = (read && e.compSize <= SMALL)
        ? (await read(start, start + e.compSize)).slice()
        : new Uint8Array(await file.slice(start, start + e.compSize).arrayBuffer());
    } else if (e.method === 8) {
      if (typeof DecompressionStream === 'undefined') {
        throw new PackError('This browser cannot open compressed packs. Update it, or make the pack again with --store.');
      }
      const raw = (read && e.compSize <= SMALL)
        ? new Blob([(await read(start, start + e.compSize)).slice()])
        : file.slice(start, start + e.compSize);
      const stream = raw.stream().pipeThrough(new DecompressionStream('deflate-raw'));
      data = new Uint8Array(await new Response(stream).arrayBuffer());
    } else {
      throw new PackError('The ZIP uses a compression method this page cannot read.');
    }
    if (data.length !== e.size) throw new PackError('The ZIP is damaged (' + e.name + ').');
    return data;
  }

  /**
   * Reads every level file of the pack and hands it to store(name, bytes).
   * onProgress({done, total, current}). Returns {files, bytes, serial}.
   *
   * A ZIP without a manifest is accepted too as long as it holds level files
   * (someone zipped the S0..S3 folders by hand).
   */
  async function importPack(file, store, onProgress) {
    const entries = await readDirectory(file);
    let manifest = null;
    const manifestEntry = entries.find((e) => e.name === 'manifest.json');
    if (manifestEntry) {
      try { manifest = JSON.parse(new TextDecoder().decode(await entryBytes(file, manifestEntry))); }
      catch (_) { manifest = null; }
      if (manifest && manifest.format !== FORMAT) manifest = null;
      if (manifest && manifest.version > 1) throw new PackError('This pack was made by a newer converter. Update the page.');
    }
    const wanted = [];
    for (const e of entries) {
      const name = e.name.split('/').pop().toLowerCase();
      if (ASSET_NAME.test(name)) wanted.push({ entry: e, name });
    }
    if (!wanted.length) throw new PackError('This ZIP has no S00000xx.NSF / .NSD level files. Make it with make_c1_pack.py.');
    wanted.sort((a, b) => a.entry.localOffset - b.entry.localOffset); // sequential reads
    const total = wanted.reduce((sum, w) => sum + w.entry.size, 0);
    let done = 0;
    const read = windowReader(file);
    for (const w of wanted) {
      if (onProgress) onProgress({ done, total, current: w.name });
      store(w.name, await entryBytes(file, w.entry, read));
      done += w.entry.size;
    }
    if (onProgress) onProgress({ done: total, total, current: '' });
    return { files: wanted.length, bytes: total, serial: manifest ? manifest.serial : null, names: wanted.map((w) => w.name) };
  }

  return { importPack, PackError };
})();
