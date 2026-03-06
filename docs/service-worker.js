const CACHE = "door-opener-v1";

self.addEventListener("install", event => {
  event.waitUntil( // don't continue until fully installed
    caches.open(CACHE).then(cache => {
      return cache.addAll([ // cache files to work offline
        "./",
        "./index.html",
        "./manifest.json"
      ]);
    })
  );
});

self.addEventListener("fetch", event => {
  event.respondWith(
    caches.match(event.request).then(response => {
      return response || fetch(event.request); // cache first, network fallback
    })
  );
});