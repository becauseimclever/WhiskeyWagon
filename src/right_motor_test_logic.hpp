#pragma once

#include <cstdint>
#include <string_view>
#include "bench_logic.hpp"

namespace whiskey_wagon {
enum class MotorTestCommand {
    kInvalid, kArm, kForward, kReverse, kForward50, kReverse50,
    kForward50Long, kReverse50Long, kForward100, kReverse100,
    kStop, kStatus, kHelp, kMark, kCalibrate10, kBoth100, kLeft100
};

constexpr MotorTestCommand motor_test_command(std::string_view text)
{
    if (text == "arm") return MotorTestCommand::kArm;
    if (text == "fwd") return MotorTestCommand::kForward;
    if (text == "rev") return MotorTestCommand::kReverse;
    if (text == "fwd50") return MotorTestCommand::kForward50;
    if (text == "rev50") return MotorTestCommand::kReverse50;
    if (text == "fwd50long") return MotorTestCommand::kForward50Long;
    if (text == "rev50long") return MotorTestCommand::kReverse50Long;
    if (text == "fwd100") return MotorTestCommand::kForward100;
    if (text == "rev100") return MotorTestCommand::kReverse100;
    if (text == "both100") return MotorTestCommand::kBoth100;
    if (text == "left100") return MotorTestCommand::kLeft100;
    if (text == "stop") return MotorTestCommand::kStop;
    if (text == "status") return MotorTestCommand::kStatus;
    if (text == "help") return MotorTestCommand::kHelp;
    if (text == "mark") return MotorTestCommand::kMark;
    if (text == "cal10") return MotorTestCommand::kCalibrate10;
    return MotorTestCommand::kInvalid;
}

constexpr double encoder_rpm(int64_t steps, uint64_t elapsed_ms, double counts_per_rev)
{
    return elapsed_ms && counts_per_rev > 0
               ? static_cast<double>(steps) * 60000.0 / elapsed_ms / counts_per_rev : 0;
}

class HallCalibration {
public:
    constexpr void cancel_mark() { marked_ = false; }
    constexpr void mark(EncoderCounter counts)
    {
        start_ = counts;
        marked_ = true;
        counts_per_rev_ = 0;
    }
    constexpr bool finish10(EncoderCounter counts)
    {
        if (!marked_) return false;
        marked_ = false;
        const auto positive = counts.positive_steps - start_.positive_steps;
        const auto negative = counts.negative_steps - start_.negative_steps;
        if (counts.invalid_transitions != start_.invalid_transitions ||
            counts.a_edges == start_.a_edges || counts.b_edges == start_.b_edges ||
            (positive == 0) == (negative == 0)) return false;
        counts_per_rev_ = static_cast<double>(positive + negative) / 10;
        return true;
    }
    constexpr double counts_per_rev() const { return counts_per_rev_; }

private:
    EncoderCounter start_;
    bool marked_ = false;
    double counts_per_rev_ = 0;
};

class RightMotorTest {
public:
    static constexpr uint32_t kPulseMs = 500;
    static constexpr uint32_t kLongPulseMs = 5000;
    static constexpr uint32_t kFullDutyPulseMs = 1000;
    static constexpr uint32_t kLeftFullDutyPulseMs = 2000;
    static constexpr uint32_t kCooldownMs = 500;
    static constexpr uint32_t kArmMs = 5000;
    static constexpr uint16_t kDutyPermille = 250;
    static constexpr uint16_t kDiagnosticDutyPermille = 500;
    static constexpr uint16_t kFullDutyPermille = 1000;

    constexpr bool arm(uint64_t now)
    {
        update(now);
        if (running() || now < ready_at_) return false;
        armed_until_ = now + kArmMs;
        return true;
    }
    constexpr bool start(int direction, uint64_t now, uint16_t duty = kDutyPermille,
                         uint32_t duration = kPulseMs)
    {
        update(now);
        const bool valid_pulse =
            (duration == kPulseMs && (duty == kDutyPermille || duty == kDiagnosticDutyPermille)) ||
            (duration == kLongPulseMs && duty == kDiagnosticDutyPermille) ||
            ((duration == kFullDutyPulseMs || duration == kLeftFullDutyPulseMs) &&
             duty == kFullDutyPermille);
        if ((direction != 1 && direction != -1) || !valid_pulse ||
            !armed(now) || running()) return false;
        armed_until_ = 0;
        direction_ = direction;
        duty_ = duty;
        duration_ms_ = duration;
        stop_at_ = now + duration;
        return true;
    }
    constexpr void stop(uint64_t now)
    {
        if (running()) ready_at_ = now + kCooldownMs;
        direction_ = 0;
        duty_ = 0;
        duration_ms_ = 0;
        armed_until_ = 0;
    }
    constexpr void update(uint64_t now)
    {
        if (running() && now >= stop_at_) stop(now);
        if (now >= armed_until_) armed_until_ = 0;
    }
    constexpr bool armed(uint64_t now) const { return armed_until_ != 0 && now < armed_until_; }
    constexpr bool running() const { return direction_ != 0; }
    constexpr int direction() const { return direction_; }
    constexpr uint16_t duty() const { return duty_; }
    constexpr uint32_t duration_ms() const { return duration_ms_; }

private:
    int direction_ = 0;
    uint16_t duty_ = 0;
    uint32_t duration_ms_ = 0;
    uint64_t armed_until_ = 0;
    uint64_t stop_at_ = 0;
    uint64_t ready_at_ = 0;
};
} // namespace whiskey_wagon
