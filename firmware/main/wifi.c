#include "wifi.h"
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "sdkconfig.h"

static const char *TAG = "wifi";

#define CONNECTED_BIT BIT0

static EventGroupHandle_t s_events;

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events, CONNECTED_BIT);
        ESP_LOGW(TAG, "disconnected, retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_events, CONNECTED_BIT);
    }
}

static void start_mdns(void)
{
    if (mdns_init() != ESP_OK) {
        ESP_LOGW(TAG, "mDNS unavailable");
        return;
    }
    mdns_hostname_set(CONFIG_ROBO_HOSTNAME);
    mdns_instance_name_set("Desk Robo");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "reachable at http://%s.local", CONFIG_ROBO_HOSTNAME);
}

esp_err_t wifi_connect(void)
{
    if (strlen(CONFIG_ROBO_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "WiFi SSID not set, run `idf.py menuconfig` > Desk Robo");
        return ESP_ERR_INVALID_STATE;
    }

    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(netif, CONFIG_ROBO_HOSTNAME);

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, CONFIG_ROBO_WIFI_SSID, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, CONFIG_ROBO_WIFI_PASSWORD, sizeof(config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    // Power save adds latency to streaming; the robot is on a power rail anyway.
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "connecting to \"%s\"", CONFIG_ROBO_WIFI_SSID);
    xEventGroupWaitBits(s_events, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    start_mdns();
    return ESP_OK;
}
