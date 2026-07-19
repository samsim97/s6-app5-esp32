#pragma once
#include <cstdint>

struct BeaconId {
    uint8_t  uuid[16];
    uint16_t major;
    uint16_t minor;
};

enum class BeaconEvent : uint8_t { ARRIVAL, DEPARTURE };

using BeaconEventCallback = void (*)(BeaconEvent, const BeaconId &);

void ble_scanner_set_callback(BeaconEventCallback cb);
void start_ble_scanner();
