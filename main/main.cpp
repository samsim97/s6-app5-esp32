#include "nvs_flash.h"
#include "esp_log.h"
#include "ble_scanner.hpp"
// #include "wifi_manager.hpp"
// #include "event_poster.hpp"
// #include "led_server.hpp"

// ── Configuration ────────────────────────────────────────────────────────────
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
#define RELAY_URL      "http://192.168.1.100:3000/events"
#define LED_GPIO       2     // built-in LED on most ESP32 devkits
#define LED_HTTP_PORT  8080
// ─────────────────────────────────────────────────────────────────────────────

static void on_beacon_event(BeaconEvent event, const BeaconId &id)
{
    // post_beacon_event(event, id);
    const char *ev = (event == BeaconEvent::ARRIVAL) ? "ARRIVAL" : "DEPARTURE";
    ESP_LOGI("MAIN", "%s  uuid=%02X%02X%02X%02X-...  major=0x%04X  minor=0x%04X",
             ev,
             id.uuid[0], id.uuid[1], id.uuid[2], id.uuid[3],
             id.major, id.minor);
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // wifi_connect(WIFI_SSID, WIFI_PASSWORD);
    // event_poster_init(RELAY_URL);
    // start_led_server(LED_GPIO, LED_HTTP_PORT);

    ble_scanner_set_callback(on_beacon_event);
    start_ble_scanner();
}
