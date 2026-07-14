#include <cstdint>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_client.hpp"

static constexpr const char *LOG_TAG            = "CLIENT";
static constexpr const char *SERVER_DEVICE_NAME = "ESP32-Server";

static const ble_uuid16_t service_uuid            = BLE_UUID16_INIT(0x1234);
static const ble_uuid16_t characteristic_uuid     = BLE_UUID16_INIT(0x5678);
static const ble_uuid16_t client_char_config_uuid = BLE_UUID16_INIT(0x2902);

static uint16_t g_server_connection_handle     = BLE_HS_CONN_HANDLE_NONE;
static uint16_t g_service_end_attribute_handle = 0;

static int on_gap_event(ble_gap_event *gap_event, void *);

static int on_descriptor_discovered(uint16_t connection_handle,
                                     const ble_gatt_error *error,
                                     [[maybe_unused]] uint16_t characteristic_value_handle,
                                     const ble_gatt_dsc *descriptor,
                                     void *)
{
    if (error->status == BLE_HS_EDONE) return 0;
    if (error->status != 0) {
        ESP_LOGE(LOG_TAG, "Descriptor discovery error: %d", error->status);
        return 0;
    }

    if (ble_uuid_cmp(&descriptor->uuid.u, &client_char_config_uuid.u) == 0) {
        uint16_t enable_notifications = 0x0001;
        ble_gattc_write_flat(connection_handle, descriptor->handle,
                             &enable_notifications, sizeof(enable_notifications), nullptr, nullptr);
        ESP_LOGI(LOG_TAG, "Subscribed to notifications (CCCD handle=%d)", descriptor->handle);
    }
    return 0;
}

static int on_characteristic_discovered(uint16_t connection_handle,
                                         const ble_gatt_error *error,
                                         const ble_gatt_chr *characteristic,
                                         void *)
{
    if (error->status == BLE_HS_EDONE) return 0;
    if (error->status != 0) {
        ESP_LOGE(LOG_TAG, "Characteristic discovery error: %d", error->status);
        return 0;
    }

    if (ble_uuid_cmp(&characteristic->uuid.u, &characteristic_uuid.u) == 0) {
        ESP_LOGI(LOG_TAG, "Found characteristic, val_handle=%d", characteristic->val_handle);
        ble_gattc_disc_all_dscs(connection_handle,
                                characteristic->val_handle,
                                g_service_end_attribute_handle,
                                on_descriptor_discovered, nullptr);
    }
    return 0;
}

static int on_service_discovered(uint16_t connection_handle,
                                  const ble_gatt_error *error,
                                  const ble_gatt_svc *service,
                                  void *)
{
    if (error->status == BLE_HS_EDONE) return 0;
    if (error->status != 0) {
        ESP_LOGE(LOG_TAG, "Service discovery error: %d", error->status);
        return 0;
    }

    if (ble_uuid_cmp(&service->uuid.u, &service_uuid.u) == 0) {
        g_service_end_attribute_handle = service->end_handle;
        ESP_LOGI(LOG_TAG, "Found service (end_handle=%d), discovering characteristics...",
                 g_service_end_attribute_handle);
        ble_gattc_disc_chrs_by_uuid(connection_handle,
                                    service->start_handle, service->end_handle,
                                    &characteristic_uuid.u, on_characteristic_discovered, nullptr);
    }
    return 0;
}

static void start_ble_scan()
{
    ble_gap_disc_params scan_params = {};
    scan_params.passive           = 0;
    scan_params.itvl              = 0x0050;
    scan_params.window            = 0x0030;
    scan_params.filter_duplicates = 1;
    ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &scan_params, on_gap_event, nullptr);
    ESP_LOGI(LOG_TAG, "Scanning for \"%s\"...", SERVER_DEVICE_NAME);
}

static int on_gap_event(ble_gap_event *gap_event, void *)
{
    switch (gap_event->type) {
    case BLE_GAP_EVENT_DISC: {
        ble_hs_adv_fields advertisement_fields;
        if (ble_hs_adv_parse_fields(&advertisement_fields,
                                    gap_event->disc.data,
                                    gap_event->disc.length_data) != 0)
            break;

        if (advertisement_fields.name != nullptr &&
            advertisement_fields.name_len == static_cast<uint8_t>(strlen(SERVER_DEVICE_NAME)) &&
            memcmp(advertisement_fields.name, SERVER_DEVICE_NAME,
                   advertisement_fields.name_len) == 0) {
            ESP_LOGI(LOG_TAG, "Found \"%s\", connecting...", SERVER_DEVICE_NAME);
            ble_gap_disc_cancel();
            ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &gap_event->disc.addr,
                            5000, nullptr, on_gap_event, nullptr);
        }
        break;
    }
    case BLE_GAP_EVENT_CONNECT:
        if (gap_event->connect.status == 0) {
            ESP_LOGI(LOG_TAG, "Connected, discovering services...");
            g_server_connection_handle = gap_event->connect.conn_handle;
            ble_gattc_disc_svc_by_uuid(g_server_connection_handle,
                                       &service_uuid.u, on_service_discovered, nullptr);
        } else {
            ESP_LOGE(LOG_TAG, "Connect failed (%d), retrying scan", gap_event->connect.status);
            start_ble_scan();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(LOG_TAG, "Disconnected, retrying scan");
        g_server_connection_handle = BLE_HS_CONN_HANDLE_NONE;
        start_ble_scan();
        break;
    case BLE_GAP_EVENT_NOTIFY_RX: {
        uint8_t received_counter = 0;
        if (OS_MBUF_PKTLEN(gap_event->notify_rx.om) >= 1) {
            os_mbuf_copydata(gap_event->notify_rx.om, 0, 1, &received_counter);
            ESP_LOGI(LOG_TAG, "Notification received: counter=%d", received_counter);
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

static void on_bluetooth_sync()
{
    ble_hs_util_ensure_addr(0);
    start_ble_scan();
}

static void nimble_host_task(void *)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void start_ble_client()
{
    nimble_port_init();
    ble_svc_gap_init();
    ble_hs_cfg.sync_cb = on_bluetooth_sync;
    nimble_port_freertos_init(nimble_host_task);
}
