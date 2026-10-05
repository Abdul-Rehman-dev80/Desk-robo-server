#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Digilog desk robot: ESP32-S3-CAM (N16R8) + 0.96" SSD1306 OLED + INMP441 mic
// + MAX98357A amp + 2x SG90 (yaw/pitch). Pins follow the robot's real wiring.

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Separate I2S buses for the mic and the amp
#define AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_1
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_14
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_39
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_40
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_21

#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0

// OLED (I2C). esp32-camera's SCCB driver takes I2C port 1 on the S3, so the OLED uses port 0.
#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42
#define DISPLAY_I2C_PORT 0
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

// Servos. LEDC timer 0 / channel 0 belong to the camera clock.
#define SERVO_YAW_GPIO          GPIO_NUM_47
#define SERVO_PITCH_GPIO        GPIO_NUM_45
#define SERVO_LEDC_TIMER        LEDC_TIMER_1
#define SERVO_YAW_LEDC_CHANNEL  LEDC_CHANNEL_2
#define SERVO_PITCH_LEDC_CHANNEL LEDC_CHANNEL_3
#define SERVO_MIN_PULSE_US      500     // 0 degrees
#define SERVO_MAX_PULSE_US      2500    // 180 degrees
#define SERVO_CENTER_DEG        90
// Safe travel limits, so a bracket never gets pushed past its stops
#define SERVO_YAW_MIN_DEG       10
#define SERVO_YAW_MAX_DEG       170
#define SERVO_PITCH_MIN_DEG     45
#define SERVO_PITCH_MAX_DEG     135
// Moves are ramped to this speed to keep current spikes low
#define SERVO_MAX_SPEED_DEG_S   180

// OV2640 camera (ESP32-S3-CAM board)
#define CAMERA_PIN_D0 GPIO_NUM_11
#define CAMERA_PIN_D1 GPIO_NUM_9
#define CAMERA_PIN_D2 GPIO_NUM_8
#define CAMERA_PIN_D3 GPIO_NUM_10
#define CAMERA_PIN_D4 GPIO_NUM_12
#define CAMERA_PIN_D5 GPIO_NUM_18
#define CAMERA_PIN_D6 GPIO_NUM_17
#define CAMERA_PIN_D7 GPIO_NUM_16
#define CAMERA_PIN_XCLK GPIO_NUM_15
#define CAMERA_PIN_PCLK GPIO_NUM_13
#define CAMERA_PIN_VSYNC GPIO_NUM_6
#define CAMERA_PIN_HREF GPIO_NUM_7
#define CAMERA_PIN_SIOC GPIO_NUM_5
#define CAMERA_PIN_SIOD GPIO_NUM_4
#define CAMERA_PIN_PWDN GPIO_NUM_NC
#define CAMERA_PIN_RESET GPIO_NUM_NC
#define XCLK_FREQ_HZ 20000000

#endif // _BOARD_CONFIG_H_
