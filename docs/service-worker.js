const CACHE = "door-opener-v1";

self.addEventListener("install", event => {
  event.waitUntil( // don't continue until fully installed
    caches.open(CACHE).then(cache => {
      return cache.addAll([ // cache files to work offline
        "./",
        "./index.html",
        "./manifest.json",
        "./ble.js"
      ]);
    })
  );
});

self.addEventListener("fetch", event => {
  event.respondWith(
    fetch(event.request).catch(() => caches.match(event.request))
  );
});