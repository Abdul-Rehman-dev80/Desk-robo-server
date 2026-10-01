#pragma once
#include "esp_err.h"

// Connects to the configured WiFi network and blocks until an IP is assigned.
// Also announces the robot over mDNS as <CONFIG_ROBO_HOSTNAME>.local.
esp_err_t wifi_connect(void);
