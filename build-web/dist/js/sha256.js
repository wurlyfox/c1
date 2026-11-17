// sha256.js — SHA-256 of an ArrayBuffer, as a hex string.
//
// Uses the browser's crypto.subtle when it exists. It does not exist on
// pages opened over plain http from another machine (for example a phone
// opening http://192.168.x.x:8000 served by a computer on the same network),
// because browsers only expose it in "secure contexts" (https or localhost).
// Those pages get the plain JavaScript implementation below, so the game can
// still be downloaded and verified there.
'use strict';

const C1Sha256 = (() => {
  const K = new Uint32Array([
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2]);

  function plain(buffer) {
    const data = new Uint8Array(buffer);
    const len = data.length;
    const total = ((len + 9 + 63) >> 6) << 6; // padded to a multiple of 64 bytes
    const h = new Uint32Array([0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]);
    const w = new Uint32Array(64);
    const tail = new Uint8Array(total - (len & ~63));
    tail.set(data.subarray(len & ~63));
    tail[len & 63] = 0x80;
    const bits = len * 8;
    const tv = new DataView(tail.buffer);
    tv.setUint32(tail.length - 8, Math.floor(bits / 4294967296));
    tv.setUint32(tail.length - 4, bits >>> 0);
    const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const full = len & ~63;
    for (let off = 0; off < total; off += 64) {
      const view = off < full ? dv : tv;
      const base = off < full ? off : off - full;
      for (let i = 0; i < 16; i++) w[i] = view.getUint32(base + i * 4);
      for (let i = 16; i < 64; i++) {
        const a = w[i - 15], b = w[i - 2];
        const s0 = ((a >>> 7) | (a << 25)) ^ ((a >>> 18) | (a << 14)) ^ (a >>> 3);
        const s1 = ((b >>> 17) | (b << 15)) ^ ((b >>> 19) | (b << 13)) ^ (b >>> 10);
        w[i] = (w[i - 16] + s0 + w[i - 7] + s1) | 0;
      }
      let a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
      for (let i = 0; i < 64; i++) {
        const S1 = ((e >>> 6) | (e << 26)) ^ ((e >>> 11) | (e << 21)) ^ ((e >>> 25) | (e << 7));
        const t1 = (hh + S1 + ((e & f) ^ (~e & g)) + K[i] + w[i]) | 0;
        const S0 = ((a >>> 2) | (a << 30)) ^ ((a >>> 13) | (a << 19)) ^ ((a >>> 22) | (a << 10));
        const t2 = (S0 + ((a & b) ^ (a & c) ^ (b & c))) | 0;
        hh = g; g = f; f = e; e = (d + t1) | 0; d = c; c = b; b = a; a = (t1 + t2) | 0;
      }
      h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    return Array.from(h, (x) => x.toString(16).padStart(8, '0')).join('');
  }

  const native = !!(window.crypto && window.crypto.subtle);

  async function hex(buffer) {
    if (native) {
      const d = new Uint8Array(await crypto.subtle.digest('SHA-256', buffer));
      return Array.from(d, (b) => b.toString(16).padStart(2, '0')).join('');
    }
    return plain(buffer);
  }

  return { hex, plain, native };
})();
