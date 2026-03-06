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
    fetch(event.request)
      .then(response => {
        // SUCCESS: Put a copy of the fresh file into the cache
        return caches.open(CACHE).then(cache => {
          cache.put(event.request, response.clone());
          return response;
        });
      })
      .catch(() => {
        // OFFLINE: Return the cached version if network fails
        return caches.match(event.request);
      })
  );
});