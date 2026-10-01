idf_component_register(
    SRCS "main.c" "camera.c" "servo.c" "wifi.c" "web.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_http_server esp_driver_ledc esp_psram esp_timer spi_flash nvs_flash esp_netif esp_event
)
