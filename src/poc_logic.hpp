#pragma once

#include "encoder_capture.hpp"
#include "poc_config.hpp"

namespace whiskey_wagon {
enum class PocFault { kNone, kHallInvalid, kHallSilent, kReversalTimeout, kControlOverrun };

struct PocOutputs {
    std::array<int, 2> motor_permille = {};
    uint16_t servo_us = 0;
};

constexpr PocOutputs poc_outputs(float drive, float steering, bool servo_enabled)
{
    PocOutputs outputs;
    const int duty = static_cast<int>(drive * kPocMaxDuty);
    for (unsigned int motor = 0; motor < outputs.motor_permille.size(); ++motor)
        outputs.motor_permille[motor] = kMotorInverted[motor] ? -duty : duty;
    if (servo_enabled) {
        const int travel = steering < 0 ? static_cast<int>(kServoCenterUs) - kServoLeftUs
                                       : static_cast<int>(kServoRightUs) - kServoCenterUs;
        outputs.servo_us = static_cast<uint16_t>(kServoCenterUs + steering * travel);
    }
    return outputs;
}

class PocControl {
public:
    constexpr void connection_reset()
    {
        control_.connection_reset();
        have_report_ = false;
        lost_ = true;
    }
    constexpr void disconnect() { connection_reset(); }
    constexpr void sleep()
    {
        connection_reset();
        control_.sleep();
        sleeping_ = true;
    }
    constexpr void report(DriveInput input, uint32_t now)
    {
        if (fault_ != PocFault::kNone || sleeping_) return;
        if (!have_report_ || now - last_report_ms_ >= kPocReportTimeoutMs)
            control_.connection_reset();
        have_report_ = true;
        lost_ = false;
        last_report_ms_ = now;
        const bool was_driving = control_.drive() != 0;
        control_.report(input);
        if (was_driving && control_.drive() == 0) zero_since_ms_ = now;
    }
    constexpr void tick(uint32_t now, uint32_t elapsed,
                        const std::array<EncoderSnapshot, 2>& encoders, bool monitor_hall)
    {
        if (fault_ != PocFault::kNone || sleeping_) return;
        if (elapsed > 100) {
            fail(PocFault::kControlOverrun);
            return;
        }
        if (have_report_ && now - last_report_ms_ >= kPocReportTimeoutMs) {
            connection_reset();
        }
        bool hold_drive = false;
        if (control_.drive() == 0 && last_direction_ != 0 &&
            control_.requested_direction() * last_direction_ < 0) {
            hold_drive = now - zero_since_ms_ < kReversalDwellMs;
            if (monitor_hall) {
                for (const auto& encoder : encoders)
                    hold_drive |= encoder.last_edge_us != 0 &&
                                  static_cast<uint32_t>(now -
                                      static_cast<uint32_t>(encoder.last_edge_us / 1000)) <
                                      kReversalDwellMs;
            }
            if (hold_drive && now - zero_since_ms_ >= kReversalTimeoutMs) {
                fail(PocFault::kReversalTimeout);
                return;
            }
        }
        const bool was_driving = control_.drive() != 0;
        control_.tick(elapsed, hold_drive);
        if (control_.drive() != 0) last_direction_ = control_.drive() > 0 ? 1 : -1;
        if (was_driving && control_.drive() == 0) zero_since_ms_ = now;
        if (!monitor_hall) return;
        const int duty = static_cast<int>(magnitude(control_.drive()) * kPocMaxDuty);
        for (unsigned int motor = 0; motor < encoders.size(); ++motor) {
            const auto& sample = encoders[motor];
            const uint32_t invalid = sample.counts.invalid_transitions;
            if (duty && invalid != previous_invalid_[motor]) {
                fail(PocFault::kHallInvalid);
                return;
            }
            previous_invalid_[motor] = invalid;
            if (duty < kHallMonitoringDuty) {
                monitoring_[motor] = false;
                continue;
            }
            if (!monitoring_[motor]) {
                monitoring_[motor] = true;
                monitoring_since_[motor] = now;
                channel_since_[motor] = {now, now};
                previous_edges_[motor] = {sample.counts.a_edges, sample.counts.b_edges};
            }
            const std::array<uint32_t, 2> edges = {sample.counts.a_edges, sample.counts.b_edges};
            for (unsigned int channel = 0; channel < edges.size(); ++channel) {
                if (edges[channel] != previous_edges_[motor][channel])
                    channel_since_[motor][channel] = now;
                previous_edges_[motor][channel] = edges[channel];
                if (now - channel_since_[motor][channel] >= kHallSilenceMs) {
                    fail(PocFault::kHallSilent);
                    return;
                }
            }
            const auto edge_age = now - static_cast<uint32_t>(sample.last_edge_us / 1000);
            if (now - monitoring_since_[motor] >= kHallSilenceMs &&
                (!sample.last_edge_us || edge_age >= kHallSilenceMs)) {
                fail(PocFault::kHallSilent);
                return;
            }
        }
    }
    constexpr float drive() const { return control_.drive(); }
    constexpr float steering() const { return control_.steering(); }
    constexpr bool qualified() const { return control_.qualified(); }
    constexpr bool coast() const { return control_.coast(); }
    constexpr bool lost() const { return lost_; }
    constexpr PocFault fault() const { return fault_; }
    constexpr PocOutputs outputs() const
    {
        return poc_outputs(drive(), steering(), !sleeping_ && fault_ == PocFault::kNone);
    }

private:
    constexpr void fail(PocFault fault)
    {
        fault_ = fault;
        control_.sleep();
    }
    BenchControl control_;
    PocFault fault_ = PocFault::kNone;
    bool sleeping_ = false;
    bool have_report_ = false;
    bool lost_ = true;
    uint32_t last_report_ms_ = 0;
    int last_direction_ = 0;
    uint32_t zero_since_ms_ = 0;
    std::array<bool, 2> monitoring_ = {};
    std::array<uint32_t, 2> monitoring_since_ = {};
    std::array<uint32_t, 2> previous_invalid_ = {};
    std::array<std::array<uint32_t, 2>, 2> previous_edges_ = {};
    std::array<std::array<uint32_t, 2>, 2> channel_since_ = {};
};
} // namespace whiskey_wagon
