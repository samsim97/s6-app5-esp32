#include "wifi_manager.hpp"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"

static constexpr const char* TAG           = "WIFI";
static constexpr int         CONNECTED_BIT = BIT0;

static EventGroupHandle_t g_connection_events;

static void event_handler(void*, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* disconnected_event = (wifi_event_sta_disconnected_t*)data;
        ESP_LOGW(TAG, "Disconnected (reason=%d) — retrying", disconnected_event->reason);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* got_ip_event = (ip_event_got_ip_t*)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&got_ip_event->ip_info.ip));
        xEventGroupSetBits(g_connection_events, CONNECTED_BIT);
    }
}

void wifi_connect(const char* ssid, const char* password) {
    g_connection_events = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,     event_handler, nullptr);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP,  event_handler, nullptr);

    wifi_config_t station_config = {};
    strncpy((char *)station_config.sta.ssid,     ssid,     sizeof(station_config.sta.ssid)     - 1);
    strncpy((char *)station_config.sta.password, password, sizeof(station_config.sta.password) - 1);
    station_config.sta.pmf_cfg.capable  = true;  // interop with routers running WPA2/WPA3-transition mode
    station_config.sta.pmf_cfg.required = false;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &station_config);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE); // avoid modem-sleep beacon misses that cause repeated run→init drops
    esp_wifi_connect();

    ESP_LOGI(TAG, "Connecting to \"%s\"...", ssid);
    xEventGroupWaitBits(g_connection_events, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
