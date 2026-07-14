#include "led_server.hpp"
#include <cstring>
#include <cstdio>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_http_server.h"

static constexpr const char *TAG = "LED";

static int  g_pin       = 2;
static bool g_led_state = false;

static void apply_led(bool on)
{
    g_led_state = on;
    gpio_set_level((gpio_num_t)g_pin, on ? 1 : 0);
    ESP_LOGI(TAG, "LED %s", on ? "ON" : "OFF");
}

static esp_err_t handle_get(httpd_req_t *req)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"state\":%s}", g_led_state ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, (ssize_t)strlen(buf));
    return ESP_OK;
}

static esp_err_t handle_post(httpd_req_t *req)
{
    char body[64] = {};
    int  received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    // Minimal JSON: look for "true" or "false" anywhere in the body
    bool on = (strstr(body, "true") != nullptr);
    apply_led(on);

    httpd_resp_set_type(req, "application/json");
    const char *resp = "{\"ok\":true}";
    httpd_resp_send(req, resp, (ssize_t)strlen(resp));
    return ESP_OK;
}

void start_led_server(int gpio_pin, uint16_t port)
{
    g_pin = gpio_pin;

    gpio_config_t io = {};
    io.pin_bit_mask  = 1ULL << gpio_pin;
    io.mode          = GPIO_MODE_OUTPUT;
    gpio_config(&io);
    apply_led(false);

    httpd_config_t cfg  = HTTPD_DEFAULT_CONFIG();
    cfg.server_port     = port;

    httpd_handle_t server = nullptr;
    httpd_start(&server, &cfg);

    httpd_uri_t get_uri  = { "/led", HTTP_GET,  handle_get,  nullptr };
    httpd_uri_t post_uri = { "/led", HTTP_POST, handle_post, nullptr };
    httpd_register_uri_handler(server, &get_uri);
    httpd_register_uri_handler(server, &post_uri);

    ESP_LOGI(TAG, "LED server listening on port %d, GPIO %d", port, gpio_pin);
}
