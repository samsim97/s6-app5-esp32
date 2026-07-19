#pragma once
#include <cstdint>

// Starts a CoAP server on the standard CoAP UDP port (5683).
//   GET /led  -> "on" or "off"
//   PUT /led  -> body "on" or "off"  ->  echoes the new state back
void start_led_coap_server(int led_gpio_pin);
