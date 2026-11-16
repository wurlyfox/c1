// netpack.js — "Download the game": fetches the c1 pack placed next to the
// page (data/pack.json + data/c1-pack.partNNN, see tools/pack/split_pack.py),
// verifies every part with SHA-256, keeps the parts in their own IndexedDB
// so an interrupted download resumes, then joins them and imports the result
// exactly like a ZIP chosen by hand.
//
// Adapted from ReChan Web (https://github.com/gabom88/ReChan,
// web/shell/js/netpack.js).
'use strict';

const C1NetPack = (() => {
  const BASE = 'data/';
  const DB_NAME = 'c1-download';
  const STORE = 'parts';

  class NetError extends Error {}

  const MESSAGES = {
    storageFull: 'There is not enough free storage in this browser for the game.',
    interrupted: 'The download was interrupted. Press the button again to carry on where it stopped.',
    badPart: 'A downloaded part is damaged. Try again.',
    noCrypto: 'This browser cannot verify the download (the page must be opened from localhost or https).',
  };

  // Human-readable cause: the real error name/message, never a generic
  // "network" (storage full, memory, a broken browser database... are not
  // connection problems).
  function describe(e, where) {
    if (e instanceof NetError) return e.message;
    const name = (e && e.name) || 'Error';
    const msg = (e && e.message) || String(e);
    if (name === 'QuotaExceededError' || /quota|space/i.test(msg)) return MESSAGES.storageFull;
    if (name === 'TypeError' || /fetch|network|http \d/i.test(msg)) return MESSAGES.interrupted;
    return (where ? where + ': ' : '') + name + (msg && msg !== name ? ' – ' + msg : '');
  }

  async function info() {
    try {
      const res = await fetch(BASE + 'pack.json', { cache: 'no-store' });
      if (!res.ok) return null;
      const j = await res.json();
      if (!j || j.format !== 'c1-pack-parts' || !Array.isArray(j.parts) || !j.parts.length) return null;
      return j;
    } catch (_) {
      return null;
    }
  }

  function openDb() {
    return new Promise((resolve, reject) => {
      const r = indexedDB.open(DB_NAME, 1);
      r.onupgradeneeded = () => r.result.createObjectStore(STORE);
      r.onsuccess = () => resolve(r.result);
      r.onerror = () => reject(r.error);
    });
  }
  function tx(db, mode, fn) {
    return new Promise((resolve, reject) => {
      const t = db.transaction(STORE, mode);
      const s = t.objectStore(STORE);
      let out;
      Promise.resolve(fn(s)).then((v) => { out = v; });
      t.oncomplete = () => resolve(out);
      t.onerror = () => reject(t.error);
      t.onabort = () => reject(t.error);
    });
  }
  const req = (r) => new Promise((resolve, reject) => { r.onsuccess = () => resolve(r.result); r.onerror = () => reject(r.error); });

  async function sha256Hex(buf) {
    const d = new Uint8Array(await crypto.subtle.digest('SHA-256', buf));
    return Array.from(d, (b) => b.toString(16).padStart(2, '0')).join('');
  }

  // Downloads one part with byte progress; returns its ArrayBuffer.
  async function fetchPart(part, onBytes) {
    const res = await fetch(BASE + part.name, { cache: 'no-store' });
    if (!res.ok) throw new Error('http ' + res.status);
    if (!res.body || !res.body.getReader) {
      const buf = await res.arrayBuffer();
      onBytes(buf.byteLength);
      return buf;
    }
    const reader = res.body.getReader();
    const chunks = [];
    let got = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      chunks.push(value);
      got += value.byteLength;
      onBytes(value.byteLength);
    }
    const out = new Uint8Array(got);
    let o = 0;
    for (const c of chunks) { out.set(c, o); o += c.byteLength; }
    return out.buffer;
  }

  // onProgress({ phase: 'download'|'import', done, total, part, parts })
  // store(name, bytes) receives each level file, as in C1ZipPack.importPack.
  async function downloadAndImport(pack, store, onProgress) {
    if (!crypto || !crypto.subtle) throw new NetError(MESSAGES.noCrypto);
    // The parts plus the unpacked game need a bit over twice the pack size.
    if (navigator.storage && navigator.storage.estimate) {
      try {
        const est = await navigator.storage.estimate();
        if (est.quota && est.quota - est.usage < pack.size * 2.6) throw new NetError(MESSAGES.storageFull);
      } catch (e) { if (e instanceof NetError) throw e; }
    }
    let db;
    try { db = await openDb(); } catch (e) { throw new NetError(describe(e, 'IndexedDB')); }
    try {
      // Parts kept from an earlier, interrupted download (same SHA-256 only).
      const kept = await tx(db, 'readonly', (s) => req(s.getAll()).then((rows) => rows));
      const have = new Map((kept || []).map((r) => [r.name, r]));
      const total = pack.parts.reduce((a, p) => a + p.size, 0);
      let done = 0;
      const blobs = [];
      for (let i = 0; i < pack.parts.length; i++) {
        const part = pack.parts[i];
        const old = have.get(part.name);
        // Re-read and re-hash parts kept from an interrupted download: a part
        // stored by a page that was killed can be unreadable or truncated
        // (WebKit), and reusing it would make every import fail.
        let keep = false;
        if (old && old.sha256 === part.sha256 && old.blob && old.blob.size === part.size) {
          try { keep = await sha256Hex(await old.blob.arrayBuffer()) === part.sha256; } catch (_) { keep = false; }
        }
        if (keep) {
          blobs.push(old.blob);
          done += part.size;
          onProgress && onProgress({ phase: 'download', done, total, part: i + 1, parts: pack.parts.length });
          continue;
        }
        let buf = null;
        for (let attempt = 1; attempt <= 3 && !buf; attempt++) {
          const before = done;
          try {
            const b = await fetchPart(part, (n) => {
              done += n;
              onProgress && onProgress({ phase: 'download', done, total, part: i + 1, parts: pack.parts.length });
            });
            if (b.byteLength !== part.size || await sha256Hex(b) !== part.sha256) throw new NetError(MESSAGES.badPart);
            buf = b;
          } catch (e) {
            done = before;
            if (attempt === 3) throw new NetError(describe(e, part.name));
            await new Promise((r) => setTimeout(r, 1500 * attempt));
          }
        }
        const blob = new Blob([buf]);
        try {
          await tx(db, 'readwrite', (s) => s.put({ name: part.name, sha256: part.sha256, blob }, part.name));
        } catch (e) {
          throw new NetError(describe(e, 'IndexedDB'));
        }
        blobs.push(blob);
      }

      // Joined pack (disk-backed blobs) -> the ZIP importer.
      const file = new Blob(blobs, { type: 'application/zip' });
      if (file.size !== pack.size) throw new NetError(MESSAGES.badPart);
      let result;
      try {
        result = await C1ZipPack.importPack(file, store, (p) => {
          onProgress && onProgress({ phase: 'import', done: p.done, total: p.total, current: p.current });
        });
      } catch (e) {
        // A failed import leaves nothing usable from these parts: drop them
        // so the next attempt starts clean.
        await tx(db, 'readwrite', (s) => s.clear()).catch(() => {});
        throw e;
      }
      // Installed: the parts are no longer needed.
      await tx(db, 'readwrite', (s) => s.clear());
      return result;
    } finally {
      db.close();
    }
  }

  // Forget any downloaded parts (used when an install is found incomplete).
  async function reset() {
    try {
      const db = await openDb();
      try { await tx(db, 'readwrite', (s) => s.clear()); } finally { db.close(); }
    } catch (_) { /* nothing stored */ }
  }

  // Deletes the whole download database (also when it is broken and cannot
  // even be opened).
  function deleteAll() {
    return new Promise((resolve) => {
      let r;
      try { r = indexedDB.deleteDatabase(DB_NAME); } catch (_) { resolve(); return; }
      r.onsuccess = r.onerror = r.onblocked = () => resolve();
    });
  }

  return { info, downloadAndImport, reset, deleteAll, NetError };
})();
