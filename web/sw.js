// sw.js — service worker of the c1 web app: the page works offline once it
// has been opened, and a new build is used only when the player asks for it.
//
// - Install: every file of this build is fetched with cache: 'reload', so a
//   copy kept by the HTTP cache cannot mix builds, and stored in a cache
//   named after the build.
// - The new build waits until the page sends SKIP_WAITING (the "Update"
//   button); the page then reloads under it.
// - The game data parts (data/) are never cached here: they are installed
//   into the page's own storage, and caching them would double the space.
'use strict';

const BUILD = '__C1_BUILD__';
const CACHE = 'c1web-' + BUILD;
const FILES = __C1_FILES__;

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(CACHE).then((cache) =>
    cache.addAll(FILES.map((url) => new Request(url, { cache: 'reload' })))));
});

self.addEventListener('activate', (event) => {
  event.waitUntil(caches.keys().then((keys) => Promise.all(keys
    .filter((key) => key.startsWith('c1web-') && key !== CACHE)
    .map((key) => caches.delete(key)))).then(() => self.clients.claim()));
});

self.addEventListener('message', (event) => {
  if (event.data && event.data.type === 'SKIP_WAITING') { self.skipWaiting(); }
});

self.addEventListener('fetch', (event) => {
  const req = event.request;
  if (req.method !== 'GET') { return; }
  const url = new URL(req.url);
  if (url.origin !== self.location.origin) { return; }
  if (url.pathname.includes('/data/')) { return; } // game data: always the network
  if (req.mode === 'navigate') {
    // the page of this build, so that it never mixes with newer files
    event.respondWith(caches.open(CACHE).then((cache) =>
      cache.match('index.html').then((hit) => hit || fetch(req))));
    return;
  }
  event.respondWith(caches.open(CACHE).then((cache) =>
    cache.match(req).then((hit) => hit || fetch(req))));
});
