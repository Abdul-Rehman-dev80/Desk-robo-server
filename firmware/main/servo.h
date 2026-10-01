#pragma once
#include "esp_err.h"

typedef enum {
    SERVO_PAN = 0,
    SERVO_TILT,
    SERVO_COUNT,
} servo_id_t;

// Starts the PWM outputs and the ramping task, centred.
esp_err_t servo_init(void);

// Sets a target angle in degrees. Out-of-range values are clamped to the
// configured limits, and the servo ramps there at the configured speed.
void servo_set_target(servo_id_t id, float deg);

float servo_get_position(servo_id_t id);
