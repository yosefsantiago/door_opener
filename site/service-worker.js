const CACHE = "door-opener-1.0.5";

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

// Online vs Offline Behavior
self.addEventListener("fetch", event => {
  event.respondWith(
    caches.match(event.request).then(cachedResponse => {
      // 1. If we found a match in the cache, return it!
      if (cachedResponse) {
        return cachedResponse;
      }
      // 2. Otherwise, try the network
      return fetch(event.request).catch(() => {
        // 3. NETWORK FAILED (User is offline or server is down)
        // If they were trying to load a page, give them the cached index.html
        if (event.request.mode === 'navigate') {
          return caches.match('./index.html');
        }
      });
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