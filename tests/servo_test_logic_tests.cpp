#include "servo_test_logic.hpp"

using namespace whiskey_wagon;

constexpr bool command_tests()
{
    return servo_test_command("arm").command == ServoTestCommand::kArm &&
           servo_test_command("stop").command == ServoTestCommand::kStop &&
           servo_test_command("status").command == ServoTestCommand::kStatus &&
           servo_test_command("help").command == ServoTestCommand::kHelp &&
           servo_test_command("pulse 1400").pulse_us == 1400 &&
           servo_test_command("pulse 1600").pulse_us == 1600 &&
           servo_test_command("pulse 0544").pulse_us == 544 &&
           servo_test_command("pulse 2400").pulse_us == 2400 &&
           servo_test_command("pulse 0000").command == ServoTestCommand::kPulse &&
           servo_test_command("pulse 150").command == ServoTestCommand::kInvalid &&
           servo_test_command("pulse 15000").command == ServoTestCommand::kInvalid &&
           servo_test_command("pulse +1500").command == ServoTestCommand::kInvalid &&
           servo_test_command("pulse 15a0").command == ServoTestCommand::kInvalid &&
           servo_test_command("pulse 1500 ").command == ServoTestCommand::kInvalid &&
           servo_test_command("").command == ServoTestCommand::kInvalid;
}
static_assert(command_tests());

constexpr bool arm_and_timeout_tests()
{
    ServoTest test;
    if (test.running() || test.armed(0) || test.start(1500, 0)) return false;
    if (!test.arm(0) || !test.armed(4999) || test.armed(5000)) return false;
    if (test.start(1500, 5000) || test.armed(5000)) return false;
    if (!test.arm(5001) || !test.start(1500, 5001)) return false;
    if (!test.running() || test.pulse_us() != 1500 || test.armed(5001)) return false;
    test.update(6000);
    if (!test.running()) return false;
    test.update(6001);
    if (test.running() || test.pulse_us() != 0 || test.arm(6500)) return false;
    return test.arm(6501);
}
static_assert(arm_and_timeout_tests());

constexpr bool pulse_limit_tests()
{
    ServoTest test;
    if (!test.arm(0) || test.start(543, 0) || test.armed(0)) return false;
    if (!test.arm(1) || test.start(2401, 1) || test.armed(1)) return false;
    if (!test.arm(2) || test.start(0, 2) || test.armed(2)) return false;
    if (!test.arm(3) || test.start(65535, 3) || test.armed(3)) return false;
    if (!test.arm(4) || !test.start(544, 4)) return false;
    test.stop(5);
    if (test.running() || test.armed(5) || test.arm(504)) return false;
    if (!test.arm(505) || !test.start(2400, 505)) return false;
    if (test.start(1500, 506) || test.running() || test.armed(506)) return false;
    return !test.arm(1005) && test.arm(1006);
}
static_assert(pulse_limit_tests());

constexpr bool allowed_pulse_tests()
{
    for (uint16_t pulse = ServoTest::kMinPulseUs; pulse <= ServoTest::kMaxPulseUs; ++pulse) {
        ServoTest test;
        if (!test.arm(0) || !test.start(pulse, 0) || test.pulse_us() != pulse) return false;
        test.update(ServoTest::kBurstMs - 1);
        if (!test.running()) return false;
        test.update(ServoTest::kBurstMs);
        if (test.running()) return false;
    }
    return true;
}
static_assert(allowed_pulse_tests());

constexpr bool stop_and_reconnect_tests()
{
    ServoTest test;
    if (!test.arm(10)) return false;
    test.stop(11);
    if (test.armed(11) || test.start(1500, 11)) return false;
    if (!test.arm(12) || !test.start(1500, 12)) return false;
    test.stop(13);
    if (test.running() || test.armed(13) || test.start(1500, 13)) return false;
    return !test.arm(512) && test.arm(513);
}
static_assert(stop_and_reconnect_tests());

static_assert(ServoTest::kMinPulseUs == 544 && ServoTest::kMaxPulseUs == 2400);
static_assert(ServoTest::kBurstMs == 1000 && ServoTest::kArmMs == 5000 &&
              ServoTest::kCooldownMs == 500);
static_assert(servo_clock_supported(125000000) && servo_clock_supported(125062500));
static_assert(!servo_clock_supported(999999) && !servo_clock_supported(125000001) &&
              !servo_clock_supported(256000000));
