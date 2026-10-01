#pragma once
#include "esp_err.h"

// Port 80: control page, /capture (one JPEG), /servo?pan=&tilt=, /status.
// Port 81: /stream (MJPEG). It runs on its own server because a stream
// occupies its connection for as long as the viewer is open.
esp_err_t web_start(void);
