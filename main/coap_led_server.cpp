#include "coap_led_server.hpp"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static constexpr const char *TAG            = "COAP_LED";
static constexpr uint16_t    COAP_UDP_PORT  = 5683; // standard CoAP port
static constexpr size_t      MAX_PACKET_LEN = 128;

static constexpr uint8_t COAP_TYPE_CON                = 0;
static constexpr uint8_t COAP_TYPE_NON                = 1;
static constexpr uint8_t COAP_TYPE_ACK                = 2;
static constexpr uint8_t COAP_CODE_GET                = 0x01; // 0.01
static constexpr uint8_t COAP_CODE_PUT                = 0x03; // 0.03
static constexpr uint8_t COAP_CODE_CONTENT            = 0x45; // 2.05
static constexpr uint8_t COAP_CODE_CHANGED            = 0x44; // 2.04
static constexpr uint8_t COAP_CODE_METHOD_NOT_ALLOWED = 0x85; // 4.05
static constexpr uint8_t COAP_PAYLOAD_MARKER          = 0xFF;

static int  g_led_gpio_pin = 2;
static bool g_led_is_on    = false;

static void apply_led_state(bool turn_on) {
    g_led_is_on = turn_on;
    gpio_set_level((gpio_num_t)g_led_gpio_pin, turn_on ? 1 : 0);
    ESP_LOGI(TAG, "LED %s", turn_on ? "ON" : "OFF");
}

struct ParsedCoapRequest {
    uint8_t        type;
    uint8_t        code;
    uint16_t       message_id;
    uint8_t        token[8];
    uint8_t        token_length;
    const uint8_t* payload;
    size_t         payload_length;
};

static bool parse_coap_request(const uint8_t* packet, size_t packet_length, ParsedCoapRequest* out) {
    if (packet_length < 4) return false;
    if (((packet[0] >> 6) & 0x03) != 1) return false; // CoAP version must be 1

    out->type         = (packet[0] >> 4) & 0x03;
    out->token_length = packet[0] & 0x0F;
    out->code         = packet[1];
    out->message_id   = (uint16_t)((packet[2] << 8) | packet[3]);

    if (out->token_length > 8 || packet_length < 4u + out->token_length) return false;
    memcpy(out->token, &packet[4], out->token_length);

    size_t cursor = 4 + out->token_length;
    while (cursor < packet_length) {
        uint8_t header = packet[cursor];
        if (header == COAP_PAYLOAD_MARKER) {
            cursor++;
            break;
        }
        cursor++;

        uint32_t option_delta  = (header >> 4) & 0x0F;
        uint32_t option_length = header & 0x0F;

        if (option_delta == 13) {
            if (cursor >= packet_length) return false;
            cursor++;
        } else if (option_delta == 14) {
            if (cursor + 1 >= packet_length) return false;
            cursor += 2;
        }

        if (option_length == 13) {
            if (cursor >= packet_length) return false;
            option_length = 13 + packet[cursor++];
        } else if (option_length == 14) {
            if (cursor + 1 >= packet_length) return false;
            option_length = 269 + ((packet[cursor] << 8) | packet[cursor + 1]);
            cursor += 2;
        }

        if (cursor + option_length > packet_length) return false;
        cursor += option_length; // option value itself is unused
    }

    out->payload        = &packet[cursor];
    out->payload_length = packet_length - cursor;
    return true;
}

static size_t build_coap_response(uint8_t* out_packet, const ParsedCoapRequest& request,
                                  uint8_t response_code, const char* state_text) {
    uint8_t response_type = (request.type == COAP_TYPE_CON) ? COAP_TYPE_ACK : COAP_TYPE_NON;

    out_packet[0] = (uint8_t)((1 << 6) | (response_type << 4) | request.token_length);
    out_packet[1] = response_code;
    out_packet[2] = (uint8_t)(request.message_id >> 8);
    out_packet[3] = (uint8_t)(request.message_id & 0xFF);

    size_t cursor = 4;
    memcpy(&out_packet[cursor], request.token, request.token_length);
    cursor += request.token_length;

    size_t state_text_length     = strlen(state_text);
    out_packet[cursor++]         = COAP_PAYLOAD_MARKER;
    memcpy(&out_packet[cursor], state_text, state_text_length);
    cursor += state_text_length;

    return cursor;
}

static void coap_server_task(void *) {
    int server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        vTaskDelete(nullptr);
        return;
    }

    sockaddr_in server_address = {};
    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port        = htons(COAP_UDP_PORT);

    if (bind(server_socket, (sockaddr *)&server_address, sizeof(server_address)) != 0) {
        ESP_LOGE(TAG, "bind() failed: errno %d", errno);
        close(server_socket);
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "CoAP server listening on UDP port %d, resource /led", COAP_UDP_PORT);

    static uint8_t request_buffer[MAX_PACKET_LEN];
    static uint8_t response_buffer[MAX_PACKET_LEN];

    while (true) {
        sockaddr_in client_address;
        socklen_t   client_address_length = sizeof(client_address);
        int received_length = recvfrom(server_socket, request_buffer, sizeof(request_buffer), 0,
                                       (sockaddr*)&client_address, &client_address_length);
        if (received_length <= 0) continue;

        ESP_LOGI(TAG, "Request from %s:%d, %d bytes",
                 inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), received_length);

        ParsedCoapRequest request;
        if (!parse_coap_request(request_buffer, (size_t)received_length, &request)) {
            ESP_LOGW(TAG, "Failed to parse incoming CoAP request, ignoring");
            continue;
        }

        uint8_t response_code;
        if (request.code == COAP_CODE_PUT) {
            bool requested_state = (request.payload_length >= 2 && memcmp(request.payload, "on", 2) == 0);
            apply_led_state(requested_state);
            response_code = COAP_CODE_CHANGED;
        } else if (request.code == COAP_CODE_GET) {
            response_code = COAP_CODE_CONTENT;
        } else {
            response_code = COAP_CODE_METHOD_NOT_ALLOWED;
        }

        const char* state_text      = g_led_is_on ? "on" : "off";
        size_t      response_length = build_coap_response(response_buffer, request, response_code, state_text);

        int sent_length = sendto(server_socket, response_buffer, response_length, 0,
                                  (sockaddr *)&client_address, client_address_length);
        if (sent_length < 0) {
            ESP_LOGE(TAG, "sendto() failed: errno %d", errno);
        } else {
            ESP_LOGI(TAG, "Replied %d bytes to %s:%d",
                     sent_length, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        }
    }
}

void start_led_coap_server(int led_gpio_pin) {
    g_led_gpio_pin = led_gpio_pin;

    gpio_config_t io_config = {};
    io_config.pin_bit_mask  = 1ULL << led_gpio_pin;
    io_config.mode          = GPIO_MODE_OUTPUT;
    gpio_config(&io_config);
    apply_led_state(false);

    xTaskCreate(coap_server_task, "coap_led_server", 4096, nullptr, 5, nullptr);
}
