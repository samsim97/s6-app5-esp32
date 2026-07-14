#pragma once
#include <cstdint>

// Starts an HTTP server on `port`.
//   GET  /led  → {"state": true|false}
//   POST /led  → body {"state": true|false}  →  {"ok": true}
void start_led_server(int gpio_pin, uint16_t port);
