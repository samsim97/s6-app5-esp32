# GeoForce — Contact-Tracing Station Prototype

This document explains what the project does, the concepts behind it, and how
its five pieces talk to each other. It's meant to be enough on its own to
present and defend the solution.

## 1. The problem being solved

GeoForce wants hospitals to know **when** an employee was in a given room,
without continuously tracking *where* everyone is. If someone tests positive
for a contagious illness, the hospital needs to reconstruct who shared a room
with them and when — nothing more. Continuous real-time location would be
overkill and a privacy liability; a log of "badge X entered room Y at time T,
left at time T'" is exactly enough to do contact tracing.

The chosen approach: fixed **stations** (an ESP32 per room) passively listen
for **Bluetooth Low Energy (BLE) beacons** broadcast by employees' phones (or,
later, physical keyfobs). A station doesn't need to pair or "know" the phone —
it just needs to notice "this badge is nearby" and "this badge is no longer
nearby", and report those two events upward.

## 2. The five components

| # | Component | Role | Technology |
|---|-----------|------|------------|
| 1 | **ESP32 station** (`main/`) | Scans for BLE beacons, detects arrival/departure, reports events, and exposes a CoAP endpoint to switch its LED | ESP-IDF (C++20), NimBLE, `esp_http_client`, a small hand-written CoAP server over raw UDP sockets |
| 2 | **Relay** (`relay/`) | Receives events from stations over HTTP and republishes them on MQTT | Node.js, Express, `mqtt` |
| 3 | **Mosquitto** | The MQTT broker — decouples publishers (Relay) from subscribers (Archive, and any future consumer) | Eclipse Mosquitto |
| 4 | **Archive** (`archive/`) | Subscribes to MQTT, persists events to a CSV file, and exposes them over HTTP for the UI | Node.js, Express, `mqtt` |
| 5 | **Control** (`control/`) | Exposes an HTTP API for the UI and speaks CoAP directly to a station to read/set its LED | Node.js, Express, `coap` |

Plus a **frontend** (`frontend/index.html`) — a single static page that calls
Archive's and Control's HTTP APIs directly. No build step, no framework: just
`fetch()`.

### Why a Relay at all?

The station could, in principle, publish straight to MQTT. The assignment
deliberately forbids that: **stations are never exposed to the event-sharing
mechanism directly.** Two reasons this matters in practice:

- **Blast radius.** If a station is compromised or misconfigured, it can only
  reach the Relay's `/events` endpoint — never the broker, never other
  stations' topics.
- **Room to grow.** GeoForce plans to add features to the stations later
  (screens, sensors, etc.). Those features go through **Control**, over CoAP,
  completely separate from the event pipeline. The two capabilities (reporting
  presence vs. being commanded) are deliberately on different transports with
  different trust boundaries, so evolving one never risks the other.

## 3. Concepts

### 3.1 BLE advertising & iBeacon

BLE devices can periodically broadcast small unencrypted "advertising"
packets without ever connecting to anything — this is how fitness trackers,
tags, and phones announce their presence. **iBeacon** is Apple's convention
for structuring the advertising payload so it carries an identity: a 16-byte
UUID plus two 16-bit numbers (**major**, **minor**) that the badge app or
keyfob is configured with. A station listening passively (no pairing, no
connection) can read this payload straight out of the air.

The relevant bytes inside the advertisement's *manufacturer-specific data*
field (see `main/ble_scanner.cpp`):

```
[0-1]  Company ID 0x004C (Apple, little-endian)
[2]    Type   0x02  (iBeacon)
[3]    Length 0x15
[4-19] UUID (16 bytes)
[20-21] Major (big-endian)
[22-23] Minor (big-endian)
[24]   TX power
```

`uuid:major:minor` together identify one badge — that triplet is what we use
as the `badge_id`.

### 3.2 Arrival / departure detection

The station has no "disconnect" event to rely on (there's no connection to
begin with). Instead it keeps a small table of badges it has recently seen,
each with a last-seen timestamp:

- First time a badge's advertisement is seen → **arrival** event, badge added
  to the table.
- Every subsequent advertisement from the same badge just refreshes its
  timestamp.
- A background task runs every second and evicts any badge whose timestamp is
  more than 20 seconds old → **departure** event.

20 seconds is comfortably larger than the gaps you see between advertisement
packets when BLE is competing with WiFi on the same radio (coexistence), so a
badge isn't falsely marked "departed" just because a packet or two was missed.

### 3.3 MQTT (publish/subscribe)

MQTT is a lightweight publish/subscribe protocol built for this kind of
fan-out: one or more publishers send messages tagged with a **topic**
(here, `geoforce/events`); the **broker** (Mosquitto) delivers a copy to every
subscriber, without either side needing to know about the other directly.
Relay is the only publisher; Archive is the only subscriber today, but any
number of other services (analytics, alerting, a second archive) could
subscribe to the same topic with zero changes to Relay.

### 3.4 CoAP (Constrained Application Protocol)

CoAP is HTTP's lightweight cousin, designed for small embedded devices: it
runs over UDP instead of TCP, uses a compact binary header, and mirrors HTTP's
verbs (`GET`, `PUT`, `POST`, `DELETE`) and response codes. The ESP32 runs a
CoAP **server** exposing one resource, `/led`:

- `GET /led` → responds with the current state as plain text, `"on"` or
  `"off"`.
- `PUT /led` with body `"on"` or `"off"` → applies it to the GPIO pin and
  echoes back the new state.

The server side (`main/coap_led_server.cpp`) is a small hand-written UDP
responder rather than a full CoAP library: it parses just enough of the
RFC 7252 header/options to read the message id, token, and payload, and
replies with a piggybacked ACK. Given the single resource and tiny payload,
that's a deliberately narrower (and easier to reason about) implementation
than a general-purpose CoAP stack — no block-wise transfer, no Observe, no
retransmission bookkeeping to get subtly wrong.

Control is the CoAP **client**: it's the only piece of software allowed to
send commands to a station.

**A real-world wrinkle worth knowing:** BLE and WiFi share one radio on the
ESP32. Continuous BLE scanning competes with WiFi for airtime, and that
contention can occasionally delay or drop ARP traffic — the underlying
"who has this IP" resolution that has to succeed before *any* UDP packet
(including a CoAP reply) can actually be delivered. If the ESP32 hasn't
talked to a given peer recently, its ARP cache entry for that peer can go
stale, and the *first* request after a quiet period can be lost while ARP
re-resolves; everything after that succeeds normally. Two mitigations are in
place for this: the BLE scan uses a ~37% duty cycle instead of continuously
scanning (`main/ble_scanner.cpp`), leaving WiFi more airtime, and the
frontend polls the LED state every 5 seconds instead of fetching it once
(`frontend/index.html`) — that keeps the Control↔ESP32 path continuously
exercised so it never goes idle long enough to go cold before a real user
action needs it.

### 3.5 REST/HTTP APIs

Archive and Control both expose small JSON HTTP APIs — this is the boundary
the UI is built against, and it's deliberately framework-agnostic (no
Express requirement, no Angular requirement): any client that can do
`fetch()` or `curl` can use them.

## 4. End-to-end flow: an arrival event

```
Phone (BLE badge app)
   │  broadcasts iBeacon advertisement (UUID, major, minor)
   ▼
ESP32 station (main/ble_scanner.cpp)
   │  detects a *new* badge → BeaconEvent::ARRIVAL
   ▼
main/event_poster.cpp
   │  HTTP POST http://<relay-host>:3001/events
   │  body: {"event":"arrival","badge_id":"<uuid>:<major>:<minor>","station_id":"<esp32 mac>"}
   ▼
Relay (relay/index.js)
   │  validates the payload, publishes it unchanged to MQTT topic "geoforce/events"
   ▼
Mosquitto broker
   │  delivers the message to every subscriber of "geoforce/events"
   ▼
Archive (archive/index.js)
   │  appends a row to archive/events.csv: timestamp,event,badge_id,station_id
   │  keeps the same row in memory for GET /events
   ▼
Frontend (frontend/index.html)
   │  polls GET http://localhost:3003/events every 5s, renders the table
```

Departure follows the exact same path — only `event` differs (`"departure"`),
triggered by the ESP32's 20-second timeout instead of a new advertisement.

## 5. End-to-end flow: toggling the LED

```
Frontend (frontend/index.html)
   │  user clicks the button → POST http://localhost:3002/led  { "state": true }
   ▼
Control (control/index.js)
   │  CoAP PUT coap://<esp32-ip>:5683/led   payload: "on"
   ▼
ESP32 station (main/coap_led_server.cpp)
   │  sets the GPIO pin, responds 2.04 Changed, payload: "on"
   ▼
Control
   │  HTTP response  { "ok": true, "state": true }
   ▼
Frontend
   │  updates the button/label
```

This path never touches Relay, Mosquitto, or Archive — LED control and event
reporting are fully independent pipelines, on purpose (see §2).

## 6. Message formats at a glance

| Hop | Format |
|---|---|
| BLE advertisement | iBeacon manufacturer data (see §3.1) |
| ESP32 → Relay (HTTP) | `{"event":"arrival\|departure","badge_id":"<uuid>:<major>:<minor>","station_id":"<mac>"}` |
| Relay → MQTT | same JSON body, unmodified, topic `geoforce/events` |
| Archive CSV row | `timestamp,event,badge_id,station_id` |
| Archive → UI (HTTP) | `[{ "timestamp", "event", "badge_id", "station_id" }, …]` |
| UI → Control (HTTP) | `GET /led` → `{ "state": bool }` · `POST /led { "state": bool }` → `{ "ok": true, "state": bool }` |
| Control → ESP32 (CoAP) | `GET /led` → `"on"`/`"off"` · `PUT /led "on"`/`"off"` → `"on"`/`"off"` |

## 7. Running the full demo (Windows / PowerShell)

The quickest path: run `.\start-all.ps1` from the repo root. It starts
Mosquitto (if it finds it), Relay, Archive, and Control each in their own
window, and opens the frontend in your browser. You still need to flash and
power the ESP32 separately. The manual steps below are what that script
automates, useful if you want to run/restart pieces individually.

1. **Install and start Mosquitto** (default port 1883). With the standard
   Windows installer, it registers as a service and starts automatically; to
   run it in a visible console instead:
   ```powershell
   & "C:\Program Files\mosquitto\mosquitto.exe" -v
   ```
2. **Start Relay, Archive, Control** — each in its own terminal:
   ```powershell
   cd relay;   npm install; npm start    # http://localhost:3001
   cd archive; npm install; npm start    # http://localhost:3003
   cd control; npm install; npm start    # http://localhost:3002
   ```
   Control needs to know the station's address if it isn't the default:
   ```powershell
   $env:ESP32_HOST = "192.168.1.166"; npm start
   ```
3. **Flash the ESP32** — update the WiFi credentials and `RELAY_EVENTS_URL`
   in `main/main.cpp` to match your network (the Relay machine's IP, port
   3001), then build/flash via the VS Code ESP-IDF tasks described in
   `README.md`.
4. **Open the UI** — `frontend/index.html` directly in a browser (or serve it
   from any static file server). It talks to Archive on `:3003` and Control
   on `:3002`.
5. **Simulate a badge** — use any iBeacon-broadcasting app (iOS/Android) on a
   phone near the station; watch arrival/departure rows appear in the UI
   within a few seconds.

## 8. Where things are configured

| Setting | Where |
|---|---|
| WiFi SSID/password | `main/main.cpp` |
| Relay URL the ESP32 posts events to | `main/main.cpp` (`RELAY_EVENTS_URL`) |
| LED GPIO pin | `main/main.cpp` (`LED_GPIO`) |
| CoAP port (station) | fixed at the standard `5683` (`main/coap_led_server.cpp`) |
| MQTT broker URL | `relay/index.js`, `archive/index.js` (`MQTT_URL` env var, default `mqtt://localhost:1883`) |
| MQTT topic | `relay/index.js`, `archive/index.js` (`geoforce/events`) |
| Station the Control app targets | `control/index.js` (`ESP32_HOST` / `ESP32_COAP_PORT` env vars) |
| Ports for Relay / Archive / Control | each app's `PORT` env var (defaults 3001 / 3003 / 3002) |
| Archive/Control URLs the frontend calls | `frontend/index.html` (`ARCHIVE`, `CONTROL` constants) |
