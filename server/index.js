const express = require('express');
const axios   = require('axios');

const PORT      = process.env.PORT      || 3000;
const ESP32_URL = process.env.ESP32_URL || 'http://192.168.1.166:8080';
const RELAY_URL = process.env.RELAY_URL || 'http://localhost:3001';

const app = express();
app.use(express.json());

let ledState = false;

// POST /events — called by ESP32 bornes
app.post('/events', (req, res) => {
    const event = req.body;
    if (!event.event || !event.badge_id || !event.station_id)
        return res.status(400).json({ error: 'Missing required fields' });

    console.log(`[SERVER] ${event.event} — badge=${event.badge_id} station=${event.station_id}`);
    res.json({ ok: true }); // ack ESP32 immediately

    // Push event to Relay asynchronously (webhook)
    axios.post(`${RELAY_URL}/events`, event)
        .catch(err => console.error('[SERVER] Could not reach Relay:', err.message));
});

// GET /led — called by Control to read cached LED state
app.get('/led', (_req, res) => res.json({ state: ledState }));

// POST /led — called by Control; forwarded to ESP32
app.post('/led', async (req, res) => {
    const { state } = req.body;
    if (typeof state !== 'boolean')
        return res.status(400).json({ error: '"state" must be a boolean' });

    try {
        await axios.post(`${ESP32_URL}/led`, { state });
        ledState = state;
        console.log(`[SERVER] LED → ${state ? 'ON' : 'OFF'}`);
        res.json({ ok: true, state: ledState });
    } catch (err) {
        console.error('[SERVER] Could not reach ESP32:', err.message);
        res.status(502).json({ error: 'Could not reach ESP32' });
    }
});

app.listen(PORT, () =>
    console.log(`[SERVER] Listening on port ${PORT} | ESP32=${ESP32_URL} | Relay=${RELAY_URL}`)
);
