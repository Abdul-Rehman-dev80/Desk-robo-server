#include "dual_servo.h"
#include "config.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "DualServo"

#define SERVO_FREQ_HZ     50
#define SERVO_PERIOD_US   20000
#define SERVO_UPDATE_MS   20

static float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

DualServo::DualServo() {
    servos_[kYaw] = {SERVO_YAW_GPIO, SERVO_YAW_LEDC_CHANNEL, SERVO_YAW_MIN_DEG, SERVO_YAW_MAX_DEG, 0, 0};
    servos_[kPitch] = {SERVO_PITCH_GPIO, SERVO_PITCH_LEDC_CHANNEL, SERVO_PITCH_MIN_DEG, SERVO_PITCH_MAX_DEG, 0, 0};

    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_14_BIT;
    timer.timer_num = SERVO_LEDC_TIMER;
    timer.freq_hz = SERVO_FREQ_HZ;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    for (auto& servo : servos_) {
        servo.position = Clamp(SERVO_CENTER_DEG, servo.min_deg, servo.max_deg);
        servo.target = servo.position;

        ledc_channel_config_t channel = {};
        channel.gpio_num = servo.gpio;
        channel.speed_mode = LEDC_LOW_SPEED_MODE;
        channel.channel = servo.channel;
        channel.timer_sel = SERVO_LEDC_TIMER;
        channel.duty = 0;
        channel.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&channel));
        WriteAngle(servo, servo.position);
    }

    xTaskCreate(Task, "servo", 3072, this, 5, nullptr);
    ESP_LOGI(TAG, "yaw on GPIO%d, pitch on GPIO%d, centered", SERVO_YAW_GPIO, SERVO_PITCH_GPIO);
}

void DualServo::SetTarget(Axis axis, float degrees) {
    if (axis >= kAxisCount) {
        return;
    }
    Servo& servo = servos_[axis];
    servo.target = Clamp(degrees, servo.min_deg, servo.max_deg);
}

void DualServo::WriteAngle(const Servo& servo, float degrees) {
    float pulse_us = SERVO_MIN_PULSE_US + (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * degrees / 180.0f;
    uint32_t duty = static_cast<uint32_t>(pulse_us * (1 << 14) / SERVO_PERIOD_US);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, servo.channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, servo.channel);
}

void DualServo::Task(void* arg) {
    auto* self = static_cast<DualServo*>(arg);
    const float max_step = SERVO_MAX_SPEED_DEG_S * SERVO_UPDATE_MS / 1000.0f;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        for (auto& servo : self->servos_) {
            float delta = servo.target - servo.position;
            if (delta != 0.0f) {
                servo.position += Clamp(delta, -max_step, max_step);
                self->WriteAngle(servo, servo.position);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SERVO_UPDATE_MS));
    }
}
