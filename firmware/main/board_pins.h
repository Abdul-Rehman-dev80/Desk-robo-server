// Pin map for the ESP32-S3-CAM (N16R8) desk robot.
// Camera, octal PSRAM (35-37) and SD card (38-40) pins are fixed by the board.
#pragma once

// OV2640 camera
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_D7      16  // Y9
#define CAM_PIN_D6      17  // Y8
#define CAM_PIN_D5      18  // Y7
#define CAM_PIN_D4      12  // Y6
#define CAM_PIN_D3      10  // Y5
#define CAM_PIN_D2      8   // Y4
#define CAM_PIN_D1      9   // Y3
#define CAM_PIN_D0      11  // Y2
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

// SSD1306 OLED (I2C)
#define OLED_PIN_SDA    41
#define OLED_PIN_SCL    42

// Shared full-duplex I2S bus: INMP441 mic + MAX98357A amp
#define I2S_PIN_BCLK    47
#define I2S_PIN_WS      21
#define I2S_PIN_MIC_SD  14  // mic data in
#define I2S_PIN_AMP_DIN 46  // amp data out

// SG90 servos
#define SERVO_PIN_PAN   2
#define SERVO_PIN_TILT  3

// Battery sense: 220k (battery +) / 100k (GND) divider on ADC1_CH0
#define BATT_PIN_SENSE  1

// On-board peripherals
#define BUTTON_PIN      0   // BOOT button, push-to-talk
#define RGB_LED_PIN     48  // WS2812 status LED
