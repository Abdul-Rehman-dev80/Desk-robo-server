#pragma once

#include <driver/gpio.h>
#include <driver/ledc.h>

// Two hobby servos (yaw + pitch) on LEDC at 50 Hz. Targets are clamped to the
// configured travel limits and reached by a speed-limited ramp.
class DualServo {
public:
    enum Axis { kYaw = 0, kPitch = 1, kAxisCount };

    DualServo();

    void SetTarget(Axis axis, float degrees);
    void MoveBy(Axis axis, float degrees) { SetTarget(axis, GetTarget(axis) + degrees); }
    float GetTarget(Axis axis) const { return servos_[axis].target; }
    float GetPosition(Axis axis) const { return servos_[axis].position; }

private:
    struct Servo {
        gpio_num_t gpio;
        ledc_channel_t channel;
        float min_deg;
        float max_deg;
        float position;
        volatile float target;
    };

    Servo servos_[kAxisCount];

    void WriteAngle(const Servo& servo, float degrees);
    static void Task(void* arg);
};
