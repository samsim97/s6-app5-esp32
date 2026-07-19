const express = require('express');
const cors    = require('cors');
const mqtt    = require('mqtt');
const fs      = require('fs');
const path    = require('path');

const PORT       = process.env.PORT     || 3003;
const MQTT_URL   = process.env.MQTT_URL || 'mqtt://localhost:1883';
const TOPIC      = 'geoforce/events';
const CSV_FILE   = path.join(__dirname, 'events.csv');
const CSV_HEADER = 'timestamp,event,badge_id,station_id\n';

const app = express();
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, '..', 'frontend'))); // serves index.html

// Load persisted events on startup
let events = [];
if (fs.existsSync(CSV_FILE)) {
    try {
        const lines = fs.readFileSync(CSV_FILE, 'utf8').trim().split('\n').slice(1);
        events = lines.filter(Boolean).map(line => {
            const [timestamp, event, badge_id, station_id] = line.split(',');
            return { timestamp, event, badge_id, station_id };
        });
    } catch { events = []; }
} else {
    fs.writeFileSync(CSV_FILE, CSV_HEADER);
}

const mqttClient = mqtt.connect(MQTT_URL);
mqttClient.on('connect', () => {
    console.log(`[ARCHIVE] MQTT connected to ${MQTT_URL}`);
    mqttClient.subscribe(TOPIC);
});
mqttClient.on('error', err => console.error('[ARCHIVE] MQTT error:', err.message));

mqttClient.on('message', (_topic, message) => {
    const { event, badge_id, station_id } = JSON.parse(message.toString());
    const timestamp = new Date().toISOString();
    const entry = { timestamp, event, badge_id, station_id };
    events.push(entry);
    fs.appendFileSync(CSV_FILE, `${timestamp},${event},${badge_id},${station_id}\n`);
    console.log(`[ARCHIVE] Saved ${event} — ${badge_id}`);
});

// GET /events — consumed by the frontend
app.get('/events', (_req, res) => res.json(events));

app.listen(PORT, () =>
    console.log(`[ARCHIVE] Listening on port ${PORT} — open http://localhost:${PORT}`)
);
