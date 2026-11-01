#pragma once

#include <cstdint>
#include <string_view>

namespace whiskey_wagon {
enum class ServoTestCommand { kInvalid, kArm, kPulse, kStop, kStatus, kHelp };

struct ServoTestRequest {
    ServoTestCommand command = ServoTestCommand::kInvalid;
    uint16_t pulse_us = 0;
};

constexpr ServoTestRequest servo_test_command(std::string_view text)
{
    if (text == "arm") return {ServoTestCommand::kArm, 0};
    if (text == "stop") return {ServoTestCommand::kStop, 0};
    if (text == "status") return {ServoTestCommand::kStatus, 0};
    if (text == "help") return {ServoTestCommand::kHelp, 0};
    if (text.size() != 10 || text.substr(0, 6) != "pulse ") return {};
    uint16_t pulse = 0;
    for (const char digit : text.substr(6)) {
        if (digit < '0' || digit > '9') return {};
        pulse = static_cast<uint16_t>(pulse * 10 + digit - '0');
    }
    return {ServoTestCommand::kPulse, pulse};
}

constexpr bool servo_clock_supported(uint32_t system_hz)
{
    return system_hz >= 1000000 && system_hz <= 255937500 && system_hz % 62500 == 0;
}

class ServoTest {
public:
    // Arduino Servo library default attach() range; not a calibrated SG92R range.
    static constexpr uint16_t kMinPulseUs = 544;
    static constexpr uint16_t kMaxPulseUs = 2400;
    static constexpr uint32_t kBurstMs = 1000;
    static constexpr uint32_t kArmMs = 5000;
    static constexpr uint32_t kCooldownMs = 500;

    constexpr bool arm(uint64_t now)
    {
        update(now);
        if (running() || now < ready_at_) return false;
        armed_until_ = now + kArmMs;
        return true;
    }

    constexpr bool start(uint16_t pulse_us, uint64_t now)
    {
        update(now);
        if (!armed(now) || running() || pulse_us < kMinPulseUs || pulse_us > kMaxPulseUs) {
            stop(now);
            return false;
        }
        armed_until_ = 0;
        pulse_us_ = pulse_us;
        stop_at_ = now + kBurstMs;
        return true;
    }

    constexpr void stop(uint64_t now)
    {
        if (running()) ready_at_ = now + kCooldownMs;
        pulse_us_ = 0;
        armed_until_ = 0;
    }

    constexpr void update(uint64_t now)
    {
        if (running() && now >= stop_at_) stop(now);
        if (now >= armed_until_) armed_until_ = 0;
    }

    constexpr bool armed(uint64_t now) const { return armed_until_ != 0 && now < armed_until_; }
    constexpr bool running() const { return pulse_us_ != 0; }
    constexpr uint16_t pulse_us() const { return pulse_us_; }

private:
    uint16_t pulse_us_ = 0;
    uint64_t armed_until_ = 0;
    uint64_t stop_at_ = 0;
    uint64_t ready_at_ = 0;
};
} // namespace whiskey_wagon
