idf_component_register(
    SRCS
        "main.cpp"
        "ble_scanner.cpp"
        "wifi_manager.cpp"
        "event_poster.cpp"
        "coap_led_server.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        bt
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        driver
        lwip
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
