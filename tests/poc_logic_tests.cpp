#include <algorithm>

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
        if (motor.gearbox_reduction != 164 || motor.rated_output_rpm != 35 ||
            motor.supply_mv != 6000 || motor.running_ma != 150 || motor.stall_ma != 500)
            return false;
    const int full = kPocMaxDuty;
    const auto forward = poc_outputs({full, full}, -1, true);
    const auto reverse = poc_outputs({-full, -full}, 1, true);
    for (unsigned int motor = 0; motor < 2; ++motor)
        if (forward.motor_permille[motor] != (kMotorInverted[motor] ? -kPocMaxDuty : kPocMaxDuty) ||
            reverse.motor_permille[motor] != -forward.motor_permille[motor])
            return false;
    const auto disabled = poc_outputs({full, full}, 1, false);
    return forward.servo_us == kServoLeftUs && reverse.servo_us == kServoRightUs &&
           poc_outputs({}, 0, true).servo_us == kServoCenterUs && disabled.servo_us == 0 &&
           disabled.motor_permille[0] == 0 && disabled.motor_permille[1] == 0;
}
static_assert(profile_and_output_tests());

// 4598 counts/s on the left motor matched a 6160-6270 rpm disc tachometer reading.
static_assert(encoder_rpm_tenths(4598, 1000) == 62700);
static_assert(encoder_rpm_tenths(-4598, 1000) == -62700);
static_assert(encoder_rpm_tenths(4598, 1000, 164) == 382);
static_assert(encoder_rpm_tenths(1000, 0) == 0);

constexpr bool configured_acceleration_tests()
{
    for (const int direction : {1, -1}) {
        PocControl control;
        control.report({}, 0);
        const DriveInput input = {direction > 0, direction < 0};
        for (uint32_t now = 1; now <= kPocAccelerationMs; ++now) {
            control.report(input, now);
            control.tick(now, 1, {}, false);
            if (now < kPocAccelerationMs && magnitude(control.drive()) >= 1) return false;
            if (kPocAccelerationMs % 2 == 0 && now == kPocAccelerationMs / 2 &&
                control.drive() != direction * 0.5f) return false;
        }
        if (control.drive() != direction) return false;
        const auto outputs = control.outputs();
        // No encoder motion: the regulator pushes to its limit, which here is the duty cap.
        const int full = direction * static_cast<int>(kPocMaxDuty);
        const auto expected = poc_outputs({full, full}, 0, true);
        for (unsigned int motor = 0; motor < outputs.motor_permille.size(); ++motor)
            if (outputs.motor_permille[motor] != expected.motor_permille[motor]) return false;
        for (uint32_t elapsed = 1; elapsed <= 700; ++elapsed) {
            const uint32_t now = kPocAccelerationMs + elapsed;
            control.report({}, now);
            control.tick(now, 1, {}, false);
            if (elapsed == 350 && control.drive() != direction * 0.5f) return false;
        }
        if (control.drive() != 0) return false;
    }
    return true;
}
static_assert(configured_acceleration_tests());

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

constexpr bool edge_newer_than_clock_tests()
{
    // A fast motor's newest edge can be stamped a millisecond after the control clock read.
    if (edge_age_ms(1000, 1'001'500) != 0 || edge_age_ms(1000, 400'000) != 600) return false;
    PocControl control;
    control.report({}, 0);
    for (uint32_t now = 10; now <= 3000; now += 10) {
        auto encoders = moving_encoders(now);
        for (auto& encoder : encoders) encoder.last_edge_us = (now + 1) * 1000ull;
        control.report({true}, now);
        control.tick(now, 10, encoders, true);
    }
    return control.fault() == PocFault::kNone && control.drive() == 1;
}
static_assert(edge_newer_than_clock_tests());

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
        const auto duty = hall.outputs().motor_permille;
        const auto largest = std::max(duty[0] < 0 ? -duty[0] : duty[0], duty[1] < 0 ? -duty[1] : duty[1]);
        if (!threshold_start && largest >= kHallMonitoringDuty)
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
    const uint32_t late = 1950 + kPocReportTimeoutMs;
    control.report({false, true}, late);
    if (control.qualified()) return false;
    for (uint32_t elapsed = 10; elapsed <= 700; elapsed += 10)
        control.tick(late + elapsed, 10, {}, false);
    if (control.drive() != 0) return false;
    control.report({}, late + 701);
    control.report({false, true}, late + 702);
    control.tick(late + 712, 10, {}, false);
    return control.qualified() && control.drive() < 0;
}
static_assert(normal_reversal_and_late_report_tests());

constexpr bool connected_link_tests()
{
    // A live connected link holds a single fresh press past the report lease.
    PocControl control;
    control.link_alive(0);
    if (!control.lost() || control.qualified()) return false;
    control.report({}, 0);
    control.report({true}, 10);
    for (uint32_t now = 20; now <= 5000; now += 10) {
        control.link_alive(now);
        control.tick(now, 10, {}, false);
    }
    if (control.lost() || control.drive() != 1) return false;
    // Link loss without disconnect callback still expires through the lease.
    for (uint32_t now = 5010; now < 5000 + kPocReportTimeoutMs; now += 10)
        control.tick(now, 10, {}, false);
    if (control.lost()) return false;
    control.tick(5000 + kPocReportTimeoutMs, 10, {}, false);
    if (!control.lost() || control.qualified()) return false;
    // A link that becomes alive again cannot revive the expired held input.
    control.link_alive(5000 + kPocReportTimeoutMs + 10);
    control.tick(5000 + kPocReportTimeoutMs + 10, 10, {}, false);
    if (!control.lost()) return false;
    control.report({true}, 5000 + kPocReportTimeoutMs + 20);
    if (control.qualified()) return false;

    // Disconnect clears held input even while the link was renewing it.
    PocControl disconnected;
    disconnected.report({}, 0);
    disconnected.report({true}, 10);
    disconnected.link_alive(2000);
    disconnected.tick(2000, 10, {}, false);
    disconnected.disconnect();
    disconnected.link_alive(2010);
    for (uint32_t now = 2010; now <= 2800; now += 10)
        disconnected.tick(now, 10, {}, false);
    if (!disconnected.lost() || disconnected.drive() != 0) return false;

    // Liveness does not bypass a latched fault.
    PocControl faulted;
    faulted.report({}, 0);
    faulted.report({true}, 10);
    faulted.tick(20, 10, {}, false);
    faulted.tick(21 + kControlOverrunMs, kControlOverrunMs + 1, {}, false);
    faulted.link_alive(30 + kControlOverrunMs);
    return faulted.fault() == PocFault::kControlOverrun && faulted.drive() == 0;
}
static_assert(connected_link_tests());

constexpr bool quiet_link_tests()
{
    // A quiet link becomes neutral after the settle window so the first press drives.
    PocControl control;
    control.quiet_link(kPocQuietNeutralMs - 1, 0);
    if (!control.lost() || control.qualified()) return false;
    control.quiet_link(kPocQuietNeutralMs, 0);
    if (control.lost() || !control.qualified()) return false;
    control.report({true}, kPocQuietNeutralMs + 100);
    control.tick(kPocQuietNeutralMs + 110, 10, {}, false);
    if (control.drive() <= 0) return false;

    // A held button reported at connect still blocks driving until released.
    PocControl held;
    held.report({true}, 10);
    held.quiet_link(kPocQuietNeutralMs + 10, 0);
    held.link_alive(kPocQuietNeutralMs + 10);
    held.tick(kPocQuietNeutralMs + 10, 10, {}, false);
    if (held.qualified() || held.drive() != 0) return false;

    // Quiet links never bypass a latched fault.
    PocControl faulted;
    faulted.report({}, 0);
    faulted.report({true}, 10);
    faulted.tick(20, 10, {}, false);
    faulted.tick(21 + kControlOverrunMs, kControlOverrunMs + 1, {}, false);
    faulted.disconnect();
    faulted.quiet_link(30 + kControlOverrunMs + kPocQuietNeutralMs, 0);
    return faulted.fault() == PocFault::kControlOverrun && faulted.lost() &&
           faulted.drive() == 0;
}
static_assert(quiet_link_tests());

constexpr bool near(float value, float expected, float tolerance)
{
    return value >= expected - tolerance && value <= expected + tolerance;
}

// First-order wheel model: full_rpm at 1000 permille forward duty, 150 ms time constant.
struct WheelPlant {
    float full_rpm;
    float rpm = 0;
    float counts = 0;
    constexpr void step(int duty, uint32_t ms)
    {
        const float seconds = static_cast<float>(ms) / 1000;
        rpm += (full_rpm * static_cast<float>(duty) / 1000 - rpm) * seconds / 0.15f;
        counts += rpm * static_cast<float>(kEncoderCountsPerMotorRev *
                                           kDriveMotorProfiles[0].gearbox_reduction) *
                  seconds / 60;
    }
};

constexpr int64_t raw_position(float forward_counts, unsigned int wheel)
{
    return static_cast<int64_t>(forward_counts) * kEncoderSign[wheel] *
           (kMotorInverted[wheel] ? -1 : 1);
}

constexpr DriveRegulator run_drive(std::array<WheelPlant, 2>& plants, float drive,
                                   float steering, uint32_t ms)
{
    DriveRegulator regulator;
    for (uint32_t t = 0; t < ms; t += 10) {
        regulator.update(drive, steering,
                         {raw_position(plants[0].counts, 0), raw_position(plants[1].counts, 1)},
                         10, 1000);
        for (unsigned int wheel = 0; wheel < 2; ++wheel)
            plants[wheel].step(regulator.wheel(wheel).duty(), 10);
    }
    return regulator;
}

constexpr bool regulator_sync_tests()
{
    // Mismatched motors under different loads still hold the same commanded speed.
    std::array<WheelPlant, 2> plants = {{{36}, {40}}};
    const auto straight = run_drive(plants, 1, 0, 3000);
    if (!near(plants[0].rpm, kMaxWheelRpm, 0.5f) || !near(plants[1].rpm, kMaxWheelRpm, 0.5f) ||
        !near(straight.wheel(0).rpm(), plants[0].rpm, 0.5f) || straight.scale() != 1)
        return false;
    std::array<WheelPlant, 2> reverse = {{{36}, {40}}};
    run_drive(reverse, -0.5f, 0, 3000);
    if (!near(reverse[0].rpm, -kMaxWheelRpm / 2, 0.5f) ||
        !near(reverse[1].rpm, -kMaxWheelRpm / 2, 0.5f))
        return false;
    // A wheel that cannot reach the target holds the other back to its speed.
    std::array<WheelPlant, 2> weak = {{{30}, {40}}};
    const auto limited = run_drive(weak, 1, 0, 4000);
    return limited.scale() < 1 && weak[0].rpm > 28 && near(weak[1].rpm, weak[0].rpm, 1.5f);
}
static_assert(regulator_sync_tests());

constexpr bool differential_tests()
{
    if (!near(sin_small(kPi / 6), 0.5f, 1e-4f) || !near(cos_small(kPi / 3), 0.5f, 1e-4f))
        return false;
    if (inside_wheel_ratio(0) != 1 || inside_wheel_ratio(-1) != inside_wheel_ratio(1) ||
        inside_wheel_ratio(1) <= 0 || inside_wheel_ratio(0.5f) <= inside_wheel_ratio(1))
        return false;
    // 30 degree beam: R = 350 / tan 30 = 606 mm, inside/outside = (606 - 158) / (606 + 158).
    if (kSteerLockDeg == 30 && !near(inside_wheel_ratio(1), 0.5865f, 0.001f)) return false;
    const auto left = wheel_targets(1, -1);
    const auto reverse_right = wheel_targets(-1, 1);
    if (left[1] != kMaxWheelRpm || !(left[0] > 0 && left[0] < left[1]) ||
        reverse_right[0] != -kMaxWheelRpm ||
        !(reverse_right[1] < 0 && reverse_right[1] > reverse_right[0]))
        return false;
    std::array<WheelPlant, 2> plants = {{{38}, {39}}};
    run_drive(plants, 1, -1, 3000);
    return near(plants[0].rpm, kMaxWheelRpm * inside_wheel_ratio(1), 0.5f) &&
           near(plants[1].rpm, kMaxWheelRpm, 0.5f);
}
static_assert(differential_tests());

constexpr bool regulator_limit_tests()
{
    // Dead feedback never pushes past feedforward plus the bounded correction.
    DriveRegulator dead;
    for (uint32_t t = 0; t < 3000; t += 10) {
        dead.update(0.5f, 0, {0, 0}, 10, 1000);
        for (unsigned int wheel = 0; wheel < 2; ++wheel) {
            const float limit = kMaxWheelRpm / 2 / kFullDutyWheelRpm[wheel] * 1000 + kMaxCorrection;
            if (dead.wheel(wheel).duty() <= 0 || static_cast<float>(dead.wheel(wheel).duty()) > limit)
                return false;
        }
    }
    // Overspeed (downhill or pushed) coasts rather than driving against the direction.
    DriveRegulator fast;
    for (uint32_t t = 0; t <= 500; t += 10) {
        const float counts = static_cast<float>(t) * 10;
        fast.update(0.2f, 0, {raw_position(counts, 0), raw_position(counts, 1)}, 10, 1000);
    }
    if (fast.wheel(0).duty() != 0 || fast.wheel(1).duty() != 0) return false;
    fast.update(0, 0, {raw_position(5100, 0), raw_position(5100, 1)}, 10, 1000);
    if (fast.wheel(0).duty() != 0 || fast.wheel(0).target() != 0) return false;
    // Y coast zeroes the motor outputs at once, without waiting for a control tick.
    PocControl control;
    control.report({}, 0);
    for (uint32_t now = 10; now <= 500; now += 10) {
        control.report({true}, now);
        control.tick(now, 10, {}, false);
    }
    if (control.outputs().motor_permille[0] == 0) return false;
    control.report({true, false, true}, 501);
    return control.outputs().motor_permille[0] == 0 && control.outputs().motor_permille[1] == 0;
}
static_assert(regulator_limit_tests());
