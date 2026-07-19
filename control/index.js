const express = require('express');
const cors    = require('cors');
const axios   = require('axios');

const PORT       = process.env.PORT       || 3002;
const SERVER_URL = process.env.SERVER_URL || 'http://localhost:3000';

const app = express();
app.use(cors());
app.use(express.json());

// GET /led — proxied to Serveur Web
app.get('/led', async (_req, res) => {
    try {
        const { data } = await axios.get(`${SERVER_URL}/led`);
        res.json(data);
    } catch (err) {
        console.error('[CONTROL] Serveur Web unreachable:', err.message);
        res.status(502).json({ error: 'Serveur Web unreachable' });
    }
});

// POST /led — proxied to Serveur Web, which forwards to ESP32
app.post('/led', async (req, res) => {
    const { state } = req.body;
    if (typeof state !== 'boolean')
        return res.status(400).json({ error: '"state" must be a boolean' });

    try {
        const { data } = await axios.post(`${SERVER_URL}/led`, { state });
        console.log(`[CONTROL] LED → ${state ? 'ON' : 'OFF'}`);
        res.json(data);
    } catch (err) {
        console.error('[CONTROL] Serveur Web unreachable:', err.message);
        res.status(502).json({ error: 'Serveur Web unreachable' });
    }
});

app.listen(PORT, () =>
    console.log(`[CONTROL] Listening on port ${PORT} | Serveur Web=${SERVER_URL}`)
);
