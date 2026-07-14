#pragma once
#include "ble_scanner.hpp"

// Call once after WiFi is up. Starts the background HTTP poster task.
void event_poster_init(const char *relay_url);

// Thread-safe: queues the event for async HTTP POST to the relay.
void post_beacon_event(BeaconEvent event, const BeaconId &id);
