#include "nvs_flash.h"
#include "ble_scanner.hpp"
#include "wifi_manager.hpp"
#include "event_poster.hpp"
#include "coap_led_server.hpp"

#define WIFI_SSID        "TP-Link_8880"
#define WIFI_PASSWORD    "05867455"
#define RELAY_EVENTS_URL "http://192.168.1.138:3001/events"
#define LED_GPIO         2

static void on_beacon_event(BeaconEvent event, const BeaconId &id) {
    post_beacon_event(event, id);
}

extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_connect(WIFI_SSID, WIFI_PASSWORD);
    event_poster_init(RELAY_EVENTS_URL);
    start_led_coap_server(LED_GPIO);

    ble_scanner_set_callback(on_beacon_event);
    start_ble_scanner();
}
