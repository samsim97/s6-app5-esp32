const express = require('express');
const mqtt    = require('mqtt');

const PORT     = process.env.PORT     || 3001;
const MQTT_URL = process.env.MQTT_URL || 'mqtt://localhost:1883';
const TOPIC    = 'geoforce/events';

const app = express();
app.use(express.json());

const mqttClient = mqtt.connect(MQTT_URL);
mqttClient.on('connect', () => console.log(`[RELAY] MQTT connected to ${MQTT_URL}`));
mqttClient.on('error',   err => console.error('[RELAY] MQTT error:', err.message));

// POST /events — webhook called by Serveur Web when ESP32 sends an event
app.post('/events', (req, res) => {
    const event = req.body;
    if (!event.event || !event.badge_id || !event.station_id)
        return res.status(400).json({ error: 'Missing required fields' });

    mqttClient.publish(TOPIC, JSON.stringify(event), err => {
        if (err) return res.status(500).json({ error: 'MQTT publish failed' });
        console.log(`[RELAY] Published ${event.event} — badge=${event.badge_id}`);
        res.json({ ok: true });
    });
});

app.listen(PORT, () => console.log(`[RELAY] Listening on port ${PORT}`));
