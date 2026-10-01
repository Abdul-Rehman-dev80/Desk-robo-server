#include "servo.h"
#include "board_pins.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "servo";

#define SERVO_LEDC_TIMER    LEDC_TIMER_1  // timer 0 is the camera XCLK
#define SERVO_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define SERVO_FREQ_HZ       50
#define SERVO_RESOLUTION    LEDC_TIMER_14_BIT
#define SERVO_PERIOD_US     20000
#define SERVO_MIN_PULSE_US  500   // 0 degrees
#define SERVO_MAX_PULSE_US  2500  // 180 degrees
#define SERVO_UPDATE_MS     20

typedef struct {
    int pin;
    ledc_channel_t channel;
    float min_deg;
    float max_deg;
    float position;
    volatile float target;
} servo_t;

static servo_t s_servos[SERVO_COUNT] = {
    [SERVO_PAN] = {
        .pin = SERVO_PIN_PAN,
        .channel = LEDC_CHANNEL_1,
        .min_deg = CONFIG_ROBO_PAN_MIN_DEG,
        .max_deg = CONFIG_ROBO_PAN_MAX_DEG,
    },
    [SERVO_TILT] = {
        .pin = SERVO_PIN_TILT,
        .channel = LEDC_CHANNEL_2,
        .min_deg = CONFIG_ROBO_TILT_MIN_DEG,
        .max_deg = CONFIG_ROBO_TILT_MAX_DEG,
    },
};

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void write_angle(servo_t *s, float deg)
{
    float pulse_us = SERVO_MIN_PULSE_US + (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * deg / 180.0f;
    uint32_t duty = (uint32_t)(pulse_us * (1 << 14) / SERVO_PERIOD_US);
    ledc_set_duty(SERVO_LEDC_MODE, s->channel, duty);
    ledc_update_duty(SERVO_LEDC_MODE, s->channel);
}

static void servo_task(void *arg)
{
    const float max_step = CONFIG_ROBO_SERVO_SPEED_DEG_S * SERVO_UPDATE_MS / 1000.0f;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            servo_t *s = &s_servos[i];
            float delta = s->target - s->position;
            if (delta == 0.0f) {
                continue;
            }
            s->position += clampf(delta, -max_step, max_step);
            write_angle(s, s->position);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SERVO_UPDATE_MS));
    }
}

esp_err_t servo_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = SERVO_LEDC_MODE,
        .timer_num = SERVO_LEDC_TIMER,
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    for (int i = 0; i < SERVO_COUNT; i++) {
        servo_t *s = &s_servos[i];
        s->position = (s->min_deg + s->max_deg) / 2.0f;
        s->target = s->position;

        ledc_channel_config_t ch = {
            .gpio_num = s->pin,
            .speed_mode = SERVO_LEDC_MODE,
            .channel = s->channel,
            .timer_sel = SERVO_LEDC_TIMER,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch));
        write_angle(s, s->position);
    }

    BaseType_t ok = xTaskCreatePinnedToCore(servo_task, "servo", 3072, NULL, 5, NULL, 1);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "pan on GPIO%d, tilt on GPIO%d, centred", SERVO_PIN_PAN, SERVO_PIN_TILT);
    return ESP_OK;
}

void servo_set_target(servo_id_t id, float deg)
{
    if (id >= SERVO_COUNT) {
        return;
    }
    servo_t *s = &s_servos[id];
    s->target = clampf(deg, s->min_deg, s->max_deg);
}

float servo_get_position(servo_id_t id)
{
    return id < SERVO_COUNT ? s_servos[id].position : 0.0f;
}
