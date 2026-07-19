#include "nvs_flash.h"
#include "ble_scanner.hpp"
#include "wifi_manager.hpp"
#include "event_poster.hpp"
#include "led_server.hpp"

// ── Configuration ────────────────────────────────────────────────────────────
#define WIFI_SSID      "TP-Link_8880"
#define WIFI_PASSWORD  "05867455"
#define SERVER_URL     "http://192.168.1.138:3000/events"
#define LED_GPIO       2
#define LED_HTTP_PORT  8080
// ─────────────────────────────────────────────────────────────────────────────

static void on_beacon_event(BeaconEvent event, const BeaconId &id)
{
    post_beacon_event(event, id);
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_connect(WIFI_SSID, WIFI_PASSWORD);
    event_poster_init(SERVER_URL);
    start_led_server(LED_GPIO, LED_HTTP_PORT);

    ble_scanner_set_callback(on_beacon_event);
    start_ble_scanner();
}
