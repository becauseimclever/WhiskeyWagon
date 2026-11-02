#pragma once

#include <array>
#include <cstdint>

#include "poc_config.hpp"

namespace whiskey_wagon {
inline constexpr float kPi = 3.14159265f;

// Short Taylor series keep the geometry usable in compile-time tests; accurate to well
// under 0.1 percent for beam angles up to 60 degrees.
constexpr float sin_small(float x)
{
    const float x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72))));
}
constexpr float cos_small(float x)
{
    const float x2 = x * x;
    return 1 - x2 / 2 * (1 - x2 / 12 * (1 - x2 / 30 * (1 - x2 / 56)));
}
constexpr float clamp(float value, float low, float high)
{
    return value < low ? low : value > high ? high : value;
}

// Inside-to-outside rear wheel speed ratio for a steering demand. The whole front beam
// pivots, so the turn center sits on the rear-axle line at wheelbase / tan(angle) from
// the centerline. Never negative: the inside wheel slows but does not reverse.
constexpr float inside_wheel_ratio(float steering)
{
    const float angle = clamp(steering < 0 ? -steering : steering, 0, 1) * kSteerLockDeg * kPi / 180;
    const float along = kWheelbaseMm * cos_small(angle);
    const float across = kRearTrackMm / 2 * sin_small(angle);
    return clamp((along - across) / (along + across), 0, 1);
}

// Rear wheel targets in wagon-forward wheel rpm, {left, right}. The outside wheel keeps
// the requested speed. Negative steering is left, matching the servo mapping.
constexpr std::array<float, 2> wheel_targets(float drive, float steering)
{
    const float speed = drive * kMaxWheelRpm;
    const float inside = speed * inside_wheel_ratio(steering);
    if (steering < 0) return {inside, speed};
    if (steering > 0) return {speed, inside};
    return {speed, speed};
}

// One wheel: windowed encoder speed plus feedforward/PI duty in the wagon-forward frame.
class WheelRegulator {
public:
    constexpr void measure(int64_t position, uint32_t elapsed_ms, int forward_sign,
                           unsigned int reduction)
    {
        if (!primed_) {
            primed_ = true;
            previous_ = position;
            return;
        }
        counts_[slot_] = (position - previous_) * forward_sign;
        window_ms_[slot_] = elapsed_ms;
        previous_ = position;
        slot_ = (slot_ + 1) % kSpeedWindowTicks;
        int64_t counts = 0;
        uint32_t ms = 0;
        for (uint32_t i = 0; i < kSpeedWindowTicks; ++i) {
            counts += counts_[i];
            ms += window_ms_[i];
        }
        rpm_ = ms == 0 ? 0
                       : static_cast<float>(counts) * 60000.0f /
                             (static_cast<float>(ms) *
                              static_cast<float>(kEncoderCountsPerMotorRev * reduction));
    }
    constexpr void control(float target, uint32_t elapsed_ms, float full_duty_rpm, int max_duty)
    {
        const int direction = target > 0 ? 1 : target < 0 ? -1 : 0;
        if (direction != direction_) {
            integral_ = 0;
            direction_ = direction;
        }
        target_ = target;
        if (direction == 0) {
            duty_ = 0;
            saturated_ = false;
            return;
        }
        const float wanted = target * static_cast<float>(direction);
        const float error = wanted - rpm_ * static_cast<float>(direction);
        const float feedforward = wanted / full_duty_rpm * 1000;
        const float proportional = kSpeedKp * error;
        const float integral = clamp(integral_ + kSpeedKi * error * static_cast<float>(elapsed_ms) / 1000,
                                     -1000, kMaxCorrection);
        const float correction = clamp(proportional + integral, -1000, kMaxCorrection);
        const float raw = feedforward + correction;
        // Anti-windup: hold the integral while the output is pinned in the error's direction.
        saturated_ = error > 0 &&
                     (raw >= static_cast<float>(max_duty) || proportional + integral >= kMaxCorrection);
        if (!saturated_ && !(error < 0 && raw <= 0)) integral_ = integral;
        // Never drive against the requested direction; slowing is left to coasting.
        duty_ = static_cast<int>(clamp(raw, 0, static_cast<float>(max_duty))) * direction;
    }
    constexpr float rpm() const { return rpm_; }
    constexpr float target() const { return target_; }
    constexpr int duty() const { return duty_; }
    constexpr bool saturated() const { return saturated_; }

private:
    bool primed_ = false;
    int64_t previous_ = 0;
    std::array<int64_t, kSpeedWindowTicks> counts_ = {};
    std::array<uint32_t, kSpeedWindowTicks> window_ms_ = {};
    uint32_t slot_ = 0;
    float rpm_ = 0;
    float target_ = 0;
    float integral_ = 0;
    int direction_ = 0;
    int duty_ = 0;
    bool saturated_ = false;
};

// Both rear wheels. When one wheel runs out of duty and falls behind, the shared speed is
// scaled down so the other wheel keeps the commanded left/right ratio instead of pulling
// ahead and steering the wagon.
class DriveRegulator {
public:
    constexpr void update(float drive, float steering, const std::array<int64_t, 2>& positions,
                          uint32_t elapsed_ms, int max_duty)
    {
        for (unsigned int wheel = 0; wheel < wheels_.size(); ++wheel)
            wheels_[wheel].measure(positions[wheel], elapsed_ms,
                                   kEncoderSign[wheel] * (kMotorInverted[wheel] ? -1 : 1),
                                   kDriveMotorProfiles[wheel].gearbox_reduction);
        const auto requested = wheel_targets(drive, steering);
        const float seconds = static_cast<float>(elapsed_ms) / 1000;
        if (drive == 0) {
            scale_ = 1;
        } else {
            std::array<float, 2> fraction = {1, 1};
            for (unsigned int wheel = 0; wheel < wheels_.size(); ++wheel) {
                const float target = wheels_[wheel].target();
                if (target > 0.5f || target < -0.5f)
                    fraction[wheel] = wheels_[wheel].rpm() / target;
            }
            float limit = 1;
            bool any_saturated = false;
            for (unsigned int wheel = 0; wheel < wheels_.size(); ++wheel) {
                if (!wheels_[wheel].saturated()) continue;
                any_saturated = true;
                if (fraction[wheel] < fraction[1 - wheel] - kSyncTolerance)
                    limit = clamp(scale_ * fraction[wheel], 0, limit);
            }
            if (limit < scale_)
                scale_ = clamp(scale_ - kSyncDropPerSecond * seconds, limit, 1);
            else if (!any_saturated)
                scale_ = clamp(scale_ + kSyncRecoverPerSecond * seconds, 0, 1);
        }
        for (unsigned int wheel = 0; wheel < wheels_.size(); ++wheel)
            wheels_[wheel].control(requested[wheel] * scale_, elapsed_ms,
                                   kFullDutyWheelRpm[wheel], max_duty);
    }
    constexpr const WheelRegulator& wheel(unsigned int index) const { return wheels_[index]; }
    constexpr float scale() const { return scale_; }

private:
    std::array<WheelRegulator, 2> wheels_ = {};
    float scale_ = 1;
};
} // namespace whiskey_wagon
