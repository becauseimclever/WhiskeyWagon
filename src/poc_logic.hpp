#pragma once

#include "drive_regulator.hpp"
#include "encoder_capture.hpp"
#include "poc_config.hpp"

namespace whiskey_wagon {
enum class PocFault { kNone, kHallInvalid, kHallSilent, kReversalTimeout, kControlOverrun };

struct PocOutputs {
    std::array<int, 2> motor_permille = {};
    uint16_t servo_us = 0;
};

// The control clock is sampled before the encoder snapshot, so the newest edge can be a
// millisecond ahead of `now`; treat that as age zero instead of a wrapped huge age.
constexpr uint32_t edge_age_ms(uint32_t now, uint64_t last_edge_us)
{
    const auto age = static_cast<int32_t>(now - static_cast<uint32_t>(last_edge_us / 1000));
    return age < 0 ? 0 : static_cast<uint32_t>(age);
}

// Maps wagon-forward wheel duty to electrical motor duty and steering to the servo pulse.
constexpr PocOutputs poc_outputs(std::array<int, 2> forward_duty, float steering, bool enabled)
{
    PocOutputs outputs;
    if (!enabled) return outputs;
    for (unsigned int motor = 0; motor < outputs.motor_permille.size(); ++motor)
        outputs.motor_permille[motor] = kMotorInverted[motor] ? -forward_duty[motor]
                                                              : forward_duty[motor];
    const int travel = steering < 0 ? static_cast<int>(kServoCenterUs) - kServoLeftUs
                                   : static_cast<int>(kServoRightUs) - kServoCenterUs;
    outputs.servo_us = static_cast<uint16_t>(kServoCenterUs + steering * travel);
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
    // Some controllers only report changes. While the controller stays connected, the last
    // fresh report remains valid until the link drops (bounded only by the controller's
    // Bluetooth supervision timeout). This never creates or qualifies input and cannot
    // revive an expired lease.
    constexpr void link_alive(uint32_t now)
    {
        if (fault_ != PocFault::kNone || sleeping_ || !have_report_ ||
            now - last_report_ms_ >= kPocReportTimeoutMs)
            return;
        last_report_ms_ = now;
    }
    // Change-only controllers send nothing at connect, so qualification would otherwise
    // consume the first press. A link that stays quiet past the settle window is at rest.
    constexpr void quiet_link(uint32_t now, uint32_t connected_ms)
    {
        if (fault_ != PocFault::kNone || sleeping_ || have_report_ ||
            now - connected_ms < kPocQuietNeutralMs)
            return;
        report({}, now);
    }
    constexpr void tick(uint32_t now, uint32_t elapsed,
                        const std::array<EncoderSnapshot, 2>& encoders, bool monitor_hall)
    {
        if (fault_ != PocFault::kNone || sleeping_) return;
        if (elapsed > kControlOverrunMs) {
            // A stall only risks stale motor output while drive is nonzero; Bluetooth
            // setup and pairing can stall the run loop while the wagon is idle.
            if (control_.drive() != 0) {
                fail(PocFault::kControlOverrun);
                return;
            }
            ++idle_overruns_;
            if (elapsed > max_idle_overrun_ms_) max_idle_overrun_ms_ = elapsed;
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
                                  edge_age_ms(now, encoder.last_edge_us) < kReversalDwellMs;
            }
            if (hold_drive && now - zero_since_ms_ >= kReversalTimeoutMs) {
                fail(PocFault::kReversalTimeout);
                return;
            }
        }
        const bool was_driving = control_.drive() != 0;
        control_.tick(elapsed, hold_drive, kPocAccelerationMs);
        if (control_.drive() != 0) last_direction_ = control_.drive() > 0 ? 1 : -1;
        if (was_driving && control_.drive() == 0) zero_since_ms_ = now;
        regulator_.update(control_.drive(), control_.steering(),
                          {encoders[0].counts.tentative_position,
                           encoders[1].counts.tentative_position},
                          elapsed, kPocMaxDuty);
        if (!monitor_hall) return;
        for (unsigned int motor = 0; motor < encoders.size(); ++motor) {
            const auto& sample = encoders[motor];
            const int wheel_duty = regulator_.wheel(motor).duty();
            const int duty = control_.drive() == 0 ? 0 : wheel_duty < 0 ? -wheel_duty : wheel_duty;
            const uint32_t invalid = sample.counts.invalid_transitions;
            if (duty && invalid != previous_invalid_[motor]) {
                fault_motor_ = static_cast<int>(motor);
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
                    fault_motor_ = static_cast<int>(motor);
                    fail(PocFault::kHallSilent);
                    return;
                }
            }
            const auto edge_age = edge_age_ms(now, sample.last_edge_us);
            if (now - monitoring_since_[motor] >= kHallSilenceMs &&
                (!sample.last_edge_us || edge_age >= kHallSilenceMs)) {
                fault_motor_ = static_cast<int>(motor);
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
    constexpr int fault_motor() const { return fault_motor_; } // -1 when not a Hall fault
    constexpr uint32_t idle_overruns() const { return idle_overruns_; }
    constexpr uint32_t max_idle_overrun_ms() const { return max_idle_overrun_ms_; }
    constexpr PocOutputs outputs() const
    {
        // Zero drive (release complete, Y coast, loss, fault) bypasses the regulator.
        std::array<int, 2> duty = {};
        if (control_.drive() != 0)
            duty = {regulator_.wheel(0).duty(), regulator_.wheel(1).duty()};
        return poc_outputs(duty, steering(), !sleeping_ && fault_ == PocFault::kNone);
    }
    constexpr const DriveRegulator& regulator() const { return regulator_; }

private:
    constexpr void fail(PocFault fault)
    {
        fault_ = fault;
        control_.sleep();
    }
    BenchControl control_;
    DriveRegulator regulator_;
    PocFault fault_ = PocFault::kNone;
    int fault_motor_ = -1;
    uint32_t idle_overruns_ = 0;
    uint32_t max_idle_overrun_ms_ = 0;
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
