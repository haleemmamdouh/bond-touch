// ============================================================
// BOND TOUCH — SERVICE WORKER v3
// Background MQTT listener + Push Notifications
// Works even when the tab is hidden / minimised
// ============================================================

const CACHE_NAME = 'bondtouch-v4';
const CACHE_FILES = ['./index.html', './manifest.json', './icon-192.png', './icon-512.png'];

const MQTT_BROKER = 'wss://broker.emqx.io:8084/mqtt';

let mqttWs       = null;   // raw WebSocket (MQTT protocol)
let bgTopic      = null;   // set by main page via postMessage
let bgStatusTopic = null;
let myBgName     = null;
let reconnectTimer = null;

// ────────────────────────────────────────────────────────────
// INSTALL / ACTIVATE — cache shell
// ────────────────────────────────────────────────────────────
self.addEventListener('install',  e => { e.waitUntil(caches.open(CACHE_NAME).then(c => c.addAll(CACHE_FILES))); self.skipWaiting(); });
self.addEventListener('activate', e => { e.waitUntil(self.clients.claim()); });

// ────────────────────────────────────────────────────────────
// FETCH — serve from cache, fall back to network
// ────────────────────────────────────────────────────────────
self.addEventListener('fetch', e => {
  e.respondWith(caches.match(e.request).then(r => r || fetch(e.request)));
});

// ────────────────────────────────────────────────────────────
// MESSAGE from main page: { type, topic, statusTopic, name }
// ────────────────────────────────────────────────────────────
self.addEventListener('message', e => {
  const d = e.data;
  if (!d) return;

  if (d.type === 'INIT') {
    bgTopic       = d.topic;
    bgStatusTopic = d.statusTopic;
    myBgName      = d.name;
    startMQTT();
  }

  if (d.type === 'STOP') {
    stopMQTT();
  }
});

// ────────────────────────────────────────────────────────────
// NOTIFICATION click — open / focus the app
// ────────────────────────────────────────────────────────────
self.addEventListener('notificationclick', e => {
  e.notification.close();
  e.waitUntil(
    clients.matchAll({ type: 'window', includeUncontrolled: true }).then(list => {
      for (const c of list) {
        if (c.url.includes('index.html') && 'focus' in c) return c.focus();
      }
      if (clients.openWindow) return clients.openWindow('./index.html');
    })
  );
});

// ────────────────────────────────────────────────────────────
// MINIMAL MQTT-OVER-WEBSOCKET ENGINE
// Uses raw WebSocket + MQTT packet serialisation (no lib needed)
// ────────────────────────────────────────────────────────────
function startMQTT() {
  if (mqttWs && mqttWs.readyState <= 1) return; // already connecting / open
  clearTimeout(reconnectTimer);

  const clientId = 'bt_sw_' + Math.random().toString(16).slice(2, 10);

  mqttWs = new WebSocket(MQTT_BROKER, ['mqtt']);
  mqttWs.binaryType = 'arraybuffer';

  mqttWs.onopen = () => {
    // Send MQTT CONNECT packet
    const cid = strToBytes(clientId);
    const packet = [
      0x10,                    // CONNECT
      12 + cid.length,         // remaining length
      0x00, 0x04, 0x4d, 0x51, 0x54, 0x54, // "MQTT"
      0x04,                    // protocol level 4
      0x02,                    // clean session
      0x00, 0x3c,              // keepalive 60s
      0x00, cid.length, ...cid
    ];
    mqttWs.send(new Uint8Array(packet));
  };

  mqttWs.onmessage = e => {
    const buf = new Uint8Array(e.data);
    const pType = (buf[0] & 0xf0) >> 4;

    if (pType === 2) {          // CONNACK
      subscribe(bgTopic);
      if (bgStatusTopic) subscribe(bgStatusTopic);
    }

    if (pType === 3) {          // PUBLISH
      try {
        let i = 1;
        // Skip remaining-length bytes
        let mul = 1, rl = 0, digit;
        do { digit = buf[i++]; rl += (digit & 0x7f) * mul; mul *= 128; } while (digit & 0x80);

        const topicLen = (buf[i] << 8) | buf[i + 1]; i += 2;
        const topic    = bytesToStr(buf.slice(i, i + topicLen)); i += topicLen;
        const payload  = bytesToStr(buf.slice(i));

        handleMessage(topic, payload);
      } catch (_) {}
    }

    if (pType === 13) { // PINGRESP — ignore
    }
  };

  mqttWs.onerror = () => {};

  mqttWs.onclose = () => {
    reconnectTimer = setTimeout(startMQTT, 5000);
  };

  // Keepalive PINGREQ every 50 s
  const ping = setInterval(() => {
    if (mqttWs.readyState === 1) mqttWs.send(new Uint8Array([0xc0, 0x00]));
    else clearInterval(ping);
  }, 50000);
}

function stopMQTT() {
  clearTimeout(reconnectTimer);
  if (mqttWs) { try { mqttWs.close(); } catch(_){} mqttWs = null; }
}

function subscribe(topic) {
  if (!mqttWs || mqttWs.readyState !== 1 || !topic) return;
  const tb = strToBytes(topic);
  const packet = [
    0x82,                      // SUBSCRIBE
    2 + 2 + tb.length + 1,    // remaining length
    0x00, 0x01,                // packet id
    0x00, tb.length, ...tb,
    0x00                       // QoS 0
  ];
  mqttWs.send(new Uint8Array(packet));
}

function handleMessage(topic, payload) {
  if (topic !== bgTopic && topic !== bgStatusTopic) return;
  try {
    const p = JSON.parse(payload);
    if (!p.sender || (myBgName && p.sender.toLowerCase() === myBgName.toLowerCase())) return;

    // Relay message to any open windows first
    self.clients.matchAll({ includeUncontrolled: true }).then(list => {
      list.forEach(c => c.postMessage({ type: 'MQTT_MSG', topic, payload }));
    });

    // Only show notification when all windows are hidden
    self.clients.matchAll({ type: 'window', includeUncontrolled: true }).then(list => {
      const allHidden = list.every(c => !c.focused);
      if (!allHidden) return;   // app is visible — don't spam notifications

      if (p.type === 'waveform' || (!p.type && p.sender)) {
        showNotif(`💓 Touch from ${p.sender}`, 'Your partner just sent you a touch!', '❤️');
      } else if (p.type === 'ble_connect') {
        showNotif(`🟢 ${p.sender} Connected`, "Your partner's wearable is now online.", '🟢');
      }
    });
  } catch(_) {}
}

function showNotif(title, body, icon) {
  self.registration.showNotification(title, {
    body,
    icon: './icon-192.png',
    badge: './icon-192.png',
    tag: 'bondtouch',
    renotify: true,
    vibrate: [200, 100, 200],
    data: { url: './index.html' }
  });
}

// ────────────────────────────────────────────────────────────
// HELPERS
// ────────────────────────────────────────────────────────────
function strToBytes(s) { return Array.from(new TextEncoder().encode(s)); }
function bytesToStr(a) { return new TextDecoder().decode(a); }
