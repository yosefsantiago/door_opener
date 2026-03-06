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
    // try network, if it fails use cached files
    fetch(event.request) 
      .then(response => response)
      .catch(() => caches.match(event.request))
  );
});