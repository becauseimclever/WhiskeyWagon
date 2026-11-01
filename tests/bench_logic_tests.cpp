#include "bench_logic.hpp"
#include "right_motor_test_logic.hpp"
#include "hall_motion.hpp"

using namespace whiskey_wagon;

constexpr bool lifecycle_tests()
{
    LinkLifecycle link;
    link.start(false, 100);
    link.pair(1000);
    link.update(60099);
    if (link.mode() != LinkMode::kPairing) return false;
    link.update(60100);
    if (link.mode() != LinkMode::kSleeping) return false;
    link.update(999999);
    if (link.mode() != LinkMode::kSleeping) return false;
    link.start(true, 200000);
    link.update(230000);
    if (link.mode() != LinkMode::kReconnect) return false;
    link.update(230001);
    if (link.mode() != LinkMode::kPairing) return false;
    link.connected();
    link.update(999999);
    if (link.mode() != LinkMode::kConnected) return false;
    link.disconnected(1000000);
    return link.mode() == LinkMode::kReconnect;
}
static_assert(lifecycle_tests());

constexpr bool button_tests()
{
    PairButton button;
    button.update(true, 0);
    button.update(false, 10);
    if (button.update(false, 50) != ButtonEvent::kNone) return false;
    button.update(true, 100);
    button.update(true, 130);
    if (button.update(true, 2129) != ButtonEvent::kNone) return false;
    if (button.update(true, 2130) != ButtonEvent::kHold) return false;
    if (button.update(true, 5000) != ButtonEvent::kNone) return false;
    button.update(false, 5010);
    if (button.update(false, 5040) != ButtonEvent::kNone) return false;
    button.update(true, 6000);
    button.update(true, 6030);
    button.update(false, 6100);
    return button.update(false, 6130) == ButtonEvent::kShortPress;
}
static_assert(button_tests());

constexpr bool control_tests()
{
    BenchControl control;
    control.report({true});
    control.tick(1000);
    if (control.drive() != 0 || control.qualified()) return false;
    control.report({});
    control.report({true});
    control.tick(500);
    if (control.drive() != 0.5f) return false;
    control.tick(500);
    if (control.drive() != 1) return false;
    control.report({});
    control.tick(350);
    if (control.drive() != 0.5f) return false;
    control.tick(350);
    if (control.drive() != 0) return false;
    control.report({true, true});
    control.tick(1000);
    if (control.drive() != 1) return false;
    control.report({false, true});
    control.tick(700);
    if (control.drive() != 0) return false; // No fallback to older X.
    control.report({});
    control.report({false, true});
    control.tick(1000);
    if (control.drive() != -1) return false;
    control.report({true, true});
    control.tick(700);
    if (control.drive() != 0) return false; // Must pass through zero.
    control.tick(1000);
    if (control.drive() != 1) return false;
    control.report({true, true, true});
    if (control.drive() != 0 || !control.coast()) return false;
    control.report({true, true, false});
    control.tick(1000);
    if (control.drive() != 0) return false;
    control.report({});
    control.report({false, false, false, true});
    control.tick(500);
    if (control.steering() != -1) return false;
    control.report({false, false, false, true, true});
    control.tick(500);
    if (control.steering() != 0) return false;
    control.report({true, false, false, false, true});
    control.tick(1000);
    control.disconnect();
    control.tick(700);
    if (control.drive() != 0 || control.steering() != 0) return false;
    control.report({true});
    control.tick(1000);
    if (control.drive() != 0) return false;
    control.sleep();
    return control.drive() == 0 && control.steering() == 0;
}
static_assert(control_tests());

constexpr bool tie_and_reset_tests()
{
    BenchControl control;
    control.report({false, false, false, true});
    control.tick(500);
    if (control.steering() != -1) return false;
    control.report({true, true, true});
    control.tick(1000);
    if (!control.coast() || control.drive() != 0) return false;
    control.report({});
    control.report({true});
    control.tick(1000);
    if (control.drive() != 1) return false;
    control.connection_reset();
    control.report({false, true});
    control.tick(700);
    if (control.drive() != 0 || control.qualified()) return false;
    control.report({});
    control.report({false, true});
    control.tick(500);
    if (control.drive() != -0.5f) return false;
    control.report({false, true, true});
    return control.drive() == 0 && control.coast();
}
static_assert(tie_and_reset_tests());

constexpr bool independent_steering_tests()
{
    BenchControl control;
    control.report({true, false, false, false, true});
    control.tick(500);
    if (control.qualified() || control.drive() != 0 || control.steering() != 1) return false;
    control.report({});
    control.tick(500);
    control.report({true, false, false, true});
    control.tick(1000);
    control.report({true, false, true, true});
    if (control.drive() != 0 || control.steering() != -1) return false;
    control.tick(1000);
    return control.drive() == 0 && control.steering() == -1 && control.coast();
}
static_assert(independent_steering_tests());

static_assert(stick_steering(0) == 0 && stick_steering(64) == 0 && stick_steering(-64) == 0);
static_assert(stick_steering(-512) == -1 && stick_steering(511) == 1);
static_assert(stick_steering(300) > 0 && stick_steering(300) < 1);

constexpr bool axis_steering_tests()
{
    BenchControl control;
    control.report({false, false, false, false, false, stick_steering(-512)});
    control.tick(500);
    if (control.steering() != -1 || control.drive() != 0) return false;
    control.report({false, false, false, false, true, stick_steering(-512)});
    control.tick(1000);
    if (control.steering() != 1) return false;
    control.report({false, false, false, false, false, stick_steering(20)});
    control.tick(500);
    return control.steering() == 0;
}
static_assert(axis_steering_tests());

constexpr bool encoder_tests()
{
    EncoderCounter counter;
    counter.sample(2);
    counter.sample(3);
    counter.sample(1);
    counter.sample(0);
    if (counter.tentative_position != 4 || counter.a_edges != 2 ||
        counter.b_edges != 2 || counter.invalid_transitions != 0) return false;
    counter.sample(1);
    counter.sample(3);
    counter.sample(2);
    counter.sample(0);
    if (counter.tentative_position != 0) return false;
    counter.sample(3);
    return counter.invalid_transitions == 1 && counter.tentative_position == 0;
}
static_assert(encoder_tests());

constexpr bool right_motor_test_tests()
{
    RightMotorTest test;
    if (test.start(1, 0) || test.running()) return false;
    if (!test.arm(0) || !test.armed(4999) || test.armed(5000)) return false;
    if (test.start(1, 5000)) return false;
    if (!test.arm(5001) || test.start(0, 5001) || !test.start(1, 5001)) return false;
    if (test.armed(5001) || test.arm(5002) || test.start(-1, 5002)) return false;
    test.update(5500);
    if (!test.running() || test.direction() != 1) return false;
    test.update(5501);
    if (test.running() || test.arm(6000) || !test.arm(6001)) return false;
    if (!test.start(-1, 6001) || test.direction() != -1) return false;
    test.stop(6100);
    if (test.running() || test.armed(6100) || test.arm(6599)) return false;
    if (!test.arm(6600)) return false;
    test.stop(6601); // USB loss, invalid input and explicit stop all revoke arming.
    if (test.start(1, 6601)) return false;
    return RightMotorTest::kPulseMs == 500 && RightMotorTest::kDutyPermille == 250 &&
           motor_test_command("arm") == MotorTestCommand::kArm &&
           motor_test_command("fwd") == MotorTestCommand::kForward &&
           motor_test_command("rev") == MotorTestCommand::kReverse &&
           motor_test_command("stop") == MotorTestCommand::kStop &&
           motor_test_command("status") == MotorTestCommand::kStatus &&
           motor_test_command("help") == MotorTestCommand::kHelp &&
           motor_test_command("fwd 100") == MotorTestCommand::kInvalid &&
           motor_test_command("") == MotorTestCommand::kInvalid;
}
static_assert(right_motor_test_tests());

constexpr bool diagnostic_duty_tests()
{
    RightMotorTest test;
    if (test.duty() != 0 || !test.arm(0) || test.start(1, 0, 0) ||
        test.start(1, 0, 501) || !test.start(1, 0, 500) || test.duty() != 500) return false;
    test.update(500);
    if (test.duty() != 0 || test.running() || !test.arm(1000) ||
        !test.start(-1, 1000) || test.duty() != 250) return false;
    test.stop(1001);
    return test.duty() == 0 &&
           motor_test_command("fwd50") == MotorTestCommand::kForward50 &&
           motor_test_command("rev50") == MotorTestCommand::kReverse50 &&
           motor_test_command("fwd100") == MotorTestCommand::kForward100 &&
           motor_test_command("rev100") == MotorTestCommand::kReverse100;
}
static_assert(diagnostic_duty_tests());

constexpr bool long_pulse_tests()
{
    RightMotorTest test;
    if (!test.arm(100) || test.start(1, 100, 250, 5000) ||
        test.start(1, 100, 500, 5001) || test.start(1, 100, 500, 0) ||
        !test.start(1, 100, 500, 5000) || test.duration_ms() != 5000) return false;
    test.update(5099);
    if (!test.running() || test.arm(5099) || test.start(-1, 5099, 500, 5000)) return false;
    test.update(5100);
    if (test.running() || test.duty() != 0 || test.duration_ms() != 0 ||
        test.arm(5599) || !test.arm(5600)) return false;
    if (!test.start(-1, 5600, 500, 5000)) return false;
    test.stop(5601);
    return !test.running() && test.duty() == 0 &&
           motor_test_command("fwd50long") == MotorTestCommand::kForward50Long &&
           motor_test_command("rev50long") == MotorTestCommand::kReverse50Long;
}
static_assert(long_pulse_tests());

constexpr bool full_duty_pulse_tests()
{
    RightMotorTest test;
    if (!test.arm(0) || test.start(1, 0, 1000, 5000) ||
        test.start(1, 0, 1000, 500) || test.start(1, 0, 1001, 1000) ||
        test.start(1, 0, 500, 1000) || !test.start(1, 0, 1000, 1000)) return false;
    test.update(999);
    if (!test.running() || test.duty() != 1000 || test.duration_ms() != 1000) return false;
    test.update(1000);
    return !test.running() && test.duty() == 0 && !test.armed(1000);
}
static_assert(full_duty_pulse_tests());
static_assert(motor_test_command("both100") == MotorTestCommand::kBoth100);
static_assert(motor_test_command("both100long") == MotorTestCommand::kInvalid);
static_assert(motor_test_command("left100") == MotorTestCommand::kLeft100);
constexpr bool left_pulse_tests()
{
    RightMotorTest test;
    if (!test.arm(0) || test.start(1, 0, 500, 2000) ||
        test.start(1, 0, 1000, 2001) || !test.start(1, 0, 1000, 2000)) return false;
    test.update(1999);
    if (!test.running()) return false;
    test.update(2000);
    return !test.running() && test.duty() == 0;
}
static_assert(left_pulse_tests());

constexpr bool hall_measurement_tests()
{
    HallCalibration calibration;
    EncoderCounter counter;
    if (calibration.finish10(counter)) return false;
    calibration.mark(counter);
    if (calibration.finish10(counter)) return false;
    calibration.mark(counter);
    for (int i = 0; i < 10; ++i) {
        counter.sample(2); counter.sample(3); counter.sample(1); counter.sample(0);
    }
    if (counter.positive_steps != 40 || counter.negative_steps ||
        !calibration.finish10(counter) || calibration.counts_per_rev() != 4) return false;
    if (encoder_rpm(40, 1000, 4) != 600 || encoder_rpm(-40, 2000, 4) != -300)
        return false;
    calibration.mark(counter);
    counter.sample(2); counter.sample(0);
    if (calibration.finish10(counter)) return false;
    calibration.mark(counter);
    counter.sample(3);
    if (calibration.finish10(counter) || calibration.counts_per_rev() != 0) return false;
    calibration.mark(counter);
    calibration.cancel_mark();
    return !calibration.finish10(counter) &&
           motor_test_command("mark") == MotorTestCommand::kMark &&
           motor_test_command("cal10") == MotorTestCommand::kCalibrate10 &&
           encoder_rpm(12, 0, 12) == 0 && encoder_rpm(12, 1000, 0) == 0;
}
static_assert(hall_measurement_tests());

constexpr bool hall_motion_tests()
{
    HallMotionTracker tracker;
    EncoderCounter counts;
    if (tracker.update(counts, 0, 1000) != HallMotion::kNoRecentEdges) return false;
    counts.sample(2);
    if (tracker.update(counts, 1000, 1000) != HallMotion::kPositive) return false;
    if (tracker.update(counts, 1000, 250999) != HallMotion::kPositive) return false;
    if (tracker.update(counts, 1000, 251000) != HallMotion::kNoRecentEdges) return false;
    counts.sample(0);
    if (tracker.update(counts, 300000, 300000) != HallMotion::kNegative) return false;
    counts.sample(2); counts.sample(0);
    if (tracker.update(counts, 400000, 400000) != HallMotion::kUncertain) return false;
    counts.sample(3);
    if (tracker.update(counts, 500000, 500000) != HallMotion::kUncertain) return false;
    counts.sample(1);
    if (tracker.update(counts, 600000, 600000) != HallMotion::kPositive) return false;
    return tracker.update(counts, 600000, 850000) == HallMotion::kNoRecentEdges;
}
static_assert(hall_motion_tests());
