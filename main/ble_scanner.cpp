#include "ble_scanner.hpp"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

static constexpr const char *TAG               = "SCANNER";
static constexpr uint32_t    DEPARTURE_MS      = 20000; // must exceed typical adv-packet gaps under WiFi/BT coexistence
static constexpr size_t      MAX_BEACONS       = 20;

// iBeacon manufacturer-specific data layout (after AD type byte):
//   [0-1]  : Company ID 0x004C (Apple, little-endian)
//   [2]    : type   0x02
//   [3]    : length 0x15 (21 payload bytes)
//   [4-19] : UUID (16 bytes)
//   [20-21]: Major (big-endian)
//   [22-23]: Minor (big-endian)
//   [24]   : TX Power
static constexpr size_t IBEACON_MFG_LEN = 25;

static BeaconEventCb g_callback = nullptr;

struct BeaconEntry {
    BeaconId id;
    uint64_t last_seen_ms;
    bool     active;
};

static BeaconEntry      g_beacons[MAX_BEACONS];
static SemaphoreHandle_t g_mutex;

static bool is_ibeacon(const uint8_t *mfg, uint8_t len, BeaconId *out)
{
    if (len < IBEACON_MFG_LEN)      return false;
    if (mfg[0] != 0x4C || mfg[1] != 0x00) return false; // Apple
    if (mfg[2] != 0x02 || mfg[3] != 0x15) return false; // iBeacon type + length
    memcpy(out->uuid, &mfg[4], 16);
    out->major = (uint16_t)((mfg[20] << 8) | mfg[21]); // big-endian per iBeacon spec
    out->minor = (uint16_t)((mfg[22] << 8) | mfg[23]);
    return true;
}

static bool ids_equal(const BeaconId &a, const BeaconId &b)
{
    return memcmp(a.uuid, b.uuid, 16) == 0 && a.major == b.major && a.minor == b.minor;
}

static void on_beacon_seen(const BeaconId &id)
{
    uint64_t now = (uint64_t)(esp_timer_get_time() / 1000);

    xSemaphoreTake(g_mutex, portMAX_DELAY);

    for (size_t i = 0; i < MAX_BEACONS; i++) {
        if (g_beacons[i].active && ids_equal(g_beacons[i].id, id)) {
            g_beacons[i].last_seen_ms = now;
            xSemaphoreGive(g_mutex);
            return;
        }
    }

    for (size_t i = 0; i < MAX_BEACONS; i++) {
        if (!g_beacons[i].active) {
            g_beacons[i] = {id, now, true};
            xSemaphoreGive(g_mutex);
            if (g_callback) g_callback(BeaconEvent::ARRIVAL, id);
            return;
        }
    }

    xSemaphoreGive(g_mutex);
    ESP_LOGW(TAG, "Beacon table full — ignoring new beacon");
}

static void presence_task(void *)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint64_t now = (uint64_t)(esp_timer_get_time() / 1000);

        xSemaphoreTake(g_mutex, portMAX_DELAY);
        for (size_t i = 0; i < MAX_BEACONS; i++) {
            if (g_beacons[i].active && (now - g_beacons[i].last_seen_ms) > DEPARTURE_MS) {
                BeaconId gone = g_beacons[i].id;
                g_beacons[i].active = false;
                xSemaphoreGive(g_mutex);
                if (g_callback) g_callback(BeaconEvent::DEPARTURE, gone);
                xSemaphoreTake(g_mutex, portMAX_DELAY);
            }
        }
        xSemaphoreGive(g_mutex);
    }
}

static int on_gap_event(ble_gap_event *event, void *)
{
    if (event->type != BLE_GAP_EVENT_DISC) return 0;

    ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0)
        return 0;

    if (!fields.mfg_data) return 0;

    BeaconId id;
    if (is_ibeacon(fields.mfg_data, fields.mfg_data_len, &id))
        on_beacon_seen(id);

    return 0;
}

static void start_scan()
{
    ble_gap_disc_params params = {};
    params.passive           = 1; // passive — no scan requests sent
    params.itvl              = 0x0040; // 40ms interval
    params.window            = 0x0040; // 40ms window → 100% duty cycle
    params.filter_duplicates = 0; // must see repeats to refresh timestamps
    ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, on_gap_event, nullptr);
    ESP_LOGI(TAG, "Passive iBeacon scan started");
}

static void on_sync()
{
    ble_hs_util_ensure_addr(0);
    start_scan();
}

static void nimble_task(void *)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_scanner_set_callback(BeaconEventCb cb) { g_callback = cb; }

void start_ble_scanner()
{
    g_mutex = xSemaphoreCreateMutex();
    memset(g_beacons, 0, sizeof(g_beacons));

    nimble_port_init();
    ble_svc_gap_init();
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(nimble_task);

    xTaskCreate(presence_task, "presence", 4096, nullptr, 5, nullptr);
}
