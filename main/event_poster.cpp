#include "event_poster.hpp"
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_http_client.h"

static constexpr const char *TAG        = "POSTER";
static constexpr size_t      QUEUE_SIZE = 10;

static const char    *g_relay_url = nullptr;
static char           g_station_id[18]; // "AABBCCDDEEFF"
static QueueHandle_t  g_queue;

struct QueueItem {
    BeaconEvent event;
    BeaconId    id;
};

static void format_uuid(const uint8_t uuid[16], char *out, size_t out_len)
{
    snprintf(out, out_len,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0],  uuid[1],  uuid[2],  uuid[3],
        uuid[4],  uuid[5],  uuid[6],  uuid[7],
        uuid[8],  uuid[9],  uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);
}

static void send_event_to_relay(BeaconEvent event, const BeaconId &id)
{
    char uuid_str[37];
    format_uuid(id.uuid, uuid_str, sizeof(uuid_str));

    char body[256];
    snprintf(body, sizeof(body),
        "{\"event\":\"%s\",\"badge_id\":\"%s:%u:%u\",\"station_id\":\"%s\"}",
        event == BeaconEvent::ARRIVAL ? "arrival" : "departure",
        uuid_str, id.major, id.minor,
        g_station_id);

    esp_http_client_config_t client_config = {};
    client_config.url    = g_relay_url;
    client_config.method = HTTP_METHOD_POST;

    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, (int)strlen(body));

    esp_err_t result = esp_http_client_perform(client);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "%s — badge %s:%u:%u",
            event == BeaconEvent::ARRIVAL ? "ARRIVAL" : "DEPARTURE",
            uuid_str, id.major, id.minor);
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(result));
    }
    esp_http_client_cleanup(client);
}

static void poster_task(void *)
{
    QueueItem item;
    while (true) {
        if (xQueueReceive(g_queue, &item, portMAX_DELAY))
            send_event_to_relay(item.event, item.id);
    }
}

void event_poster_init(const char *relay_url)
{
    g_relay_url = relay_url;

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(g_station_id, sizeof(g_station_id),
        "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    g_queue = xQueueCreate(QUEUE_SIZE, sizeof(QueueItem));
    xTaskCreate(poster_task, "http_poster", 8192, nullptr, 4, nullptr);
}

void post_beacon_event(BeaconEvent event, const BeaconId &id)
{
    if (!g_relay_url) return;
    QueueItem item = {event, id};
    if (xQueueSend(g_queue, &item, 0) != pdTRUE)
        ESP_LOGW(TAG, "Event queue full — event dropped");
}
