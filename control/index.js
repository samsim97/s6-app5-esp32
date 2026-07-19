const express = require('express');
const cors    = require('cors');
const coap    = require('coap');

const PORT                    = process.env.PORT            || 3002;
const ESP32_HOST              = process.env.ESP32_HOST      || '192.168.1.166';
const ESP32_COAP_PORT         = process.env.ESP32_COAP_PORT || 5683;
const COAP_REQUEST_TIMEOUT_MS = 10000;

const app = express();
app.use(cors());
app.use(express.json());

function sendLedCoapRequest(method, payloadText) {
    return new Promise((resolve, reject) => {
        let settled = false;
        const settleOnce = (fn, value) => {
            if (settled) return;
            settled = true;
            clearTimeout(timeoutHandle);
            fn(value);
        };

        const timeoutHandle = setTimeout(
            () => settleOnce(reject, new Error('CoAP request to ESP32 timed out')),
            COAP_REQUEST_TIMEOUT_MS
        );

        const request = coap.request({
            host:     ESP32_HOST,
            port:     ESP32_COAP_PORT,
            method,
            pathname: '/led',
        });

        request.on('response', response => settleOnce(resolve, response.payload.toString()));
        request.on('error', err => settleOnce(reject, err));

        if (payloadText !== undefined) request.write(payloadText);
        request.end();
    });
}

// GET /led — CoAP GET to the ESP32, returns its current LED state
app.get('/led', async (_req, res) => {
    try {
        const stateText = await sendLedCoapRequest('GET');
        res.json({ state: stateText === 'on' });
    } catch (err) {
        console.error('[CONTROL] ESP32 unreachable:', err.message);
        res.status(502).json({ error: 'ESP32 unreachable' });
    }
});

// POST /led — CoAP PUT to the ESP32 to change the LED state
app.post('/led', async (req, res) => {
    const { state } = req.body;
    if (typeof state !== 'boolean')
        return res.status(400).json({ error: '"state" must be a boolean' });

    try {
        const stateText = await sendLedCoapRequest('PUT', state ? 'on' : 'off');
        console.log(`[CONTROL] LED → ${stateText.toUpperCase()}`);
        res.json({ ok: true, state: stateText === 'on' });
    } catch (err) {
        console.error('[CONTROL] ESP32 unreachable:', err.message);
        res.status(502).json({ error: 'ESP32 unreachable' });
    }
});

app.listen(PORT, () =>
    console.log(`[CONTROL] Listening on port ${PORT} | ESP32=coap://${ESP32_HOST}:${ESP32_COAP_PORT}/led`)
);
