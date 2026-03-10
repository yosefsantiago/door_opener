const CACHE = "door-opener-1.0.1";

// Cache Files for Offline
self.addEventListener("install", event => {
  event.waitUntil( // don't continue until fully installed
    caches.open(CACHE).then(cache => {
      return cache.addAll([
        "./",
        "./index.html",
        "./manifest.json",
        "./ble.js"
      ]);
    })
  );
});

// Online vs Offline Behavior (cache first, network fallback)
self.addEventListener("fetch", event => {
  event.respondWith(
    caches.match(event.request).then(response => {
      return response || fetch(event.request);
    })
  );
});

// Remove Old Versions from Cache
self.addEventListener("activate", event => {
  event.waitUntil(
    caches.keys().then(keys => {
      return Promise.all(
        keys.map(key => {
          if (key !== CACHE) {
            return caches.delete(key);
          }
        })
      );
    })
  );
});