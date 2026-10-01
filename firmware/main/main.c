#include "camera.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "servo.h"
#include "web.h"
#include "wifi.h"

static const char *TAG = "main";

static void log_board_info(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "ESP32-S3 rev %d, %d cores, flash %u MB, PSRAM %u MB",
             chip.revision, chip.cores,
             (unsigned)(flash_size / (1024 * 1024)),
             (unsigned)(esp_psram_get_size() / (1024 * 1024)));
    if (esp_reset_reason() == ESP_RST_BROWNOUT) {
        ESP_LOGW(TAG, "last reset was a BROWNOUT: the 5 V supply is sagging, check power wiring");
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    log_board_info();
    ESP_ERROR_CHECK(servo_init());
    ESP_ERROR_CHECK(camera_init());
    ESP_ERROR_CHECK(wifi_connect());
    ESP_ERROR_CHECK(web_start());
}
