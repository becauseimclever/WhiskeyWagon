#include "poc_logic.hpp"

using namespace whiskey_wagon;

constexpr std::array<EncoderSnapshot, 2> moving_encoders(uint32_t now)
{
    std::array<EncoderSnapshot, 2> encoders = {};
    for (auto& encoder : encoders) {
        encoder.counts.a_edges = now / 10;
        encoder.counts.b_edges = now / 10;
        encoder.last_edge_us = static_cast<uint64_t>(now) * 1000;
    }
    return encoders;
}

constexpr bool profile_and_output_tests()
{
    for (const auto& motor : kDriveMotorProfiles)
        if (motor.gearbox_reduction != 171 || motor.rated_output_rpm != 35 ||
            motor.supply_mv != 6000 || motor.running_ma != 150 || motor.stall_ma != 500)
            return false;
    const auto forward = poc_outputs(1, -1, true);
    const auto reverse = poc_outputs(-1, 1, true);
    for (unsigned int motor = 0; motor < 2; ++motor)
        if (forward.motor_permille[motor] != (kMotorInverted[motor] ? -kPocMaxDuty : kPocMaxDuty) ||
            reverse.motor_permille[motor] != -forward.motor_permille[motor])
            return false;
    return forward.servo_us == kServoLeftUs && reverse.servo_us == kServoRightUs &&
           poc_outputs(0, 0, true).servo_us == kServoCenterUs &&
           poc_outputs(0, 1, false).servo_us == 0;
}
static_assert(profile_and_output_tests());

constexpr bool qualification_and_loss_tests()
{
    PocControl control;
    control.report({true}, 0);
    control.tick(100, 100, {}, false);
    if (control.qualified() || control.drive() != 0) return false;
    control.report({}, 100);
    for (uint32_t now = 110; now <= 1100; now += 10) {
        control.report({true, false, false, true}, now);
        control.tick(now, 10, {}, false);
    }
    if (control.drive() != 1 || control.steering() != -1) return false;
    for (uint32_t now = 1110; now < 1100 + kPocReportTimeoutMs; now += 10)
        control.tick(now, 10, {}, false);
    const uint32_t expiry = 1100 + kPocReportTimeoutMs;
    control.tick(expiry, 10, {}, false);
    if (!control.lost() || control.qualified() || control.drive() >= 1) return false;
    control.report({true}, expiry + 1);
    if (control.qualified()) return false;
    for (uint32_t now = expiry + 10; now <= expiry + 700; now += 10)
        control.tick(now, 10, {}, false);
    if (control.drive() != 0 || control.steering() != 0) return false;
    control.report({}, expiry + 701);
    control.report({true}, expiry + 702);
    control.tick(expiry + 712, 10, {}, false);
    if (control.drive() <= 0) return false;
    control.disconnect();
    control.tick(expiry + 722, 10, {}, false);
    return control.lost() && !control.qualified();
}
static_assert(qualification_and_loss_tests());

constexpr bool stop_and_reversal_tests()
{
    PocControl control;
    control.report({}, 0);
    for (uint32_t now = 10; now <= 1000; now += 10) {
        control.report({true}, now);
        control.tick(now, 10, {}, false);
    }
    control.report({true, false, true, true}, 1001);
    if (control.drive() != 0 || !control.coast()) return false;
    control.report({true}, 1002);
    control.tick(1012, 10, {}, false);
    if (control.drive() != 0) return false;
    control.report({false, true, false, true}, 1013);
    for (uint32_t now = 1023; now <= 1243; now += 10) {
        control.report({false, true, false, true}, now);
        control.tick(now, 10, {}, false);
        if (control.drive() != 0) return false;
    }
    if (control.steering() >= 0) return false; // Steering continues during drive dwell.
    control.report({false, true}, 1251);
    control.tick(1251, 8, {}, false);
    if (control.drive() >= 0) return false;
    control.sleep();
    control.report({true}, 1252);
    control.tick(1262, 10, {}, false);
    return control.drive() == 0 && control.outputs().servo_us == 0;
}
static_assert(stop_and_reversal_tests());

constexpr bool fault_tests()
{
    PocControl silent;
    PocControl one_channel;
    PocControl invalid;
    PocControl healthy;
    silent.report({}, 0);
    one_channel.report({}, 0);
    invalid.report({}, 0);
    healthy.report({}, 0);
    for (uint32_t now = 10; now <= 1600; now += 10) {
        auto encoders = moving_encoders(now);
        auto missing_channel = encoders;
        missing_channel[0].counts.b_edges = 0;
        auto bad = encoders;
        if (now >= 200) bad[1].counts.invalid_transitions = 1;
        silent.report({true}, now);
        one_channel.report({true}, now);
        invalid.report({true}, now);
        healthy.report({true}, now);
        silent.tick(now, 10, {}, true);
        one_channel.tick(now, 10, missing_channel, true);
        invalid.tick(now, 10, bad, true);
        healthy.tick(now, 10, encoders, true);
    }
    if (silent.fault() != PocFault::kHallSilent ||
        one_channel.fault() != PocFault::kHallSilent ||
        invalid.fault() != PocFault::kHallInvalid ||
        healthy.fault() != PocFault::kNone || healthy.drive() != 1) return false;
    if (silent.fault_motor() != 0 || healthy.fault_motor() != -1) return false;
    silent.connection_reset();
    silent.report({}, 1700);
    silent.report({true}, 1710);
    silent.tick(1720, 10, moving_encoders(1720), true);
    if (silent.drive() != 0 || silent.outputs().servo_us != 0) return false;
    PocControl idle;
    idle.tick(kControlOverrunMs + 1, kControlOverrunMs + 1, {}, false);
    idle.tick(kControlOverrunMs + 2, kControlOverrunMs, {}, false);
    if (idle.fault() != PocFault::kNone || idle.idle_overruns() != 1 ||
        idle.max_idle_overrun_ms() != kControlOverrunMs + 1) return false;
    PocControl overrun;
    overrun.report({}, 0);
    overrun.report({true}, 10);
    overrun.tick(20, 10, {}, false);
    if (overrun.drive() == 0) return false;
    overrun.tick(20 + kControlOverrunMs + 1, kControlOverrunMs + 1, {}, false);
    return overrun.fault() == PocFault::kControlOverrun && overrun.outputs().servo_us == 0;
}
static_assert(fault_tests());

constexpr bool exact_threshold_and_wrap_tests()
{
    PocControl control;
    constexpr uint32_t start = UINT32_MAX - 50;
    control.report({}, start);
    control.report({true}, start + 1);
    control.tick(start + 11, 10, {}, false);
    control.tick(start + kPocReportTimeoutMs, 10, {}, false);
    if (control.lost()) return false;
    control.tick(start + kPocReportTimeoutMs + 1, 10, {}, false);
    if (!control.lost()) return false;
    PocControl hall;
    hall.report({}, 0);
    uint32_t threshold_start = 0;
    for (uint32_t now = 10; now <= 2000; now += 10) {
        hall.report({true}, now);
        hall.tick(now, 10, {}, true);
        if (!threshold_start && magnitude(hall.drive()) * kPocMaxDuty >= kHallMonitoringDuty)
            threshold_start = now;
        if (threshold_start && now == threshold_start + kHallSilenceMs - 10 &&
            hall.fault() != PocFault::kNone) return false;
        if (threshold_start && now == threshold_start + kHallSilenceMs)
            return hall.fault() == PocFault::kHallSilent;
    }
    return false;
}
static_assert(exact_threshold_and_wrap_tests());

constexpr bool reversal_timeout_tests()
{
    PocControl control;
    control.report({}, 0);
    for (uint32_t now = 10; now <= 1000; now += 10) {
        control.report({true}, now);
        control.tick(now, 10, moving_encoders(now), true);
    }
    control.report({true, false, true}, 1000);
    for (uint32_t now = 1010; now <= 2990; now += 10) {
        control.report({false, true}, now);
        control.tick(now, 10, moving_encoders(now), true);
        if (control.drive() != 0 || control.fault() != PocFault::kNone) return false;
    }
    control.report({false, true}, 3000);
    control.tick(3000, 10, moving_encoders(3000), true);
    return control.fault() == PocFault::kReversalTimeout && control.drive() == 0;
}
static_assert(reversal_timeout_tests());

constexpr bool normal_reversal_and_late_report_tests()
{
    PocControl control;
    control.report({}, 0);
    for (uint32_t now = 10; now <= 1000; now += 10) {
        control.report({true}, now);
        control.tick(now, 10, {}, false);
    }
    for (uint32_t now = 1010; now <= 1700; now += 10) {
        control.report({false, true}, now);
        control.tick(now, 10, {}, false);
        if (control.drive() < 0) return false;
    }
    if (control.drive() != 0) return false;
    for (uint32_t now = 1710; now < 1950; now += 10) {
        control.report({false, true}, now);
        control.tick(now, 10, {}, false);
        if (control.drive() != 0) return false;
    }
    control.report({false, true}, 1950);
    control.tick(1950, 10, {}, false);
    if (control.drive() >= 0) return false;
    // A late held report cannot renew the old command, even before a timeout tick.
    control.report({false, true}, 1950 + kPocReportTimeoutMs);
    if (control.qualified()) return false;
    control.tick(1960 + kPocReportTimeoutMs, 10, {}, false);
    if (control.drive() != 0) return false;
    control.report({}, 1961 + kPocReportTimeoutMs);
    control.report({false, true}, 1962 + kPocReportTimeoutMs);
    control.tick(1972 + kPocReportTimeoutMs, 10, {}, false);
    return control.qualified() && control.drive() < 0;
}
static_assert(normal_reversal_and_late_report_tests());
