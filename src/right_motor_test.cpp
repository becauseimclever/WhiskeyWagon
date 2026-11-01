#include <cstdio>
#include <string_view>

#include "board_config.hpp"
#include "encoder_capture.hpp"
#include "hall_motion.hpp"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "right_motor_test_logic.hpp"

namespace {
using namespace whiskey_wagon;
constexpr auto kIn1 = kMotorInputPins[2];
constexpr auto kIn2 = kMotorInputPins[3];
constexpr uint16_t kPwmTop = 999;
RightMotorTest test;
alarm_id_t stop_alarm = 0;
volatile bool pulse_expired = false;
EncoderSnapshot before_pulse;
uint64_t pulse_started_ms = 0;
HallCalibration calibration;
EncoderSnapshot rate_previous;
uint64_t rate_previous_ms = 0;
HallMotionTracker motion_tracker;

void print_motion()
{
    const auto sample = encoder_snapshot()[1];
    const uint64_t now_us = time_us_64();
    const auto state = motion_tracker.update(sample.counts, sample.last_edge_us, now_us);
    const char* name = "NO_RECENT_EDGES";
    switch (state) {
    case HallMotion::kPositive: name = "HALL_POSITIVE"; break;
    case HallMotion::kNegative: name = "HALL_NEGATIVE"; break;
    case HallMotion::kUncertain: name = "UNCERTAIN_MIXED_OR_INVALID"; break;
    case HallMotion::kNoRecentEdges: break;
    }
    printf("motion t_ms=%llu state=%s edge_age_ms=%lld source=HALL_ONLY\n",
           static_cast<unsigned long long>(now_us / 1000), name,
           sample.last_edge_us ? static_cast<long long>((now_us - sample.last_edge_us) / 1000) : -1);
}
#if WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED
constexpr const char* kStopDescription = "PWM zero; STBY UNCONTROLLED, GP14 untouched";
#else
constexpr const char* kStopDescription = "PWM zero; STBY LOW";
#endif

void inhibit()
{
#if !WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED
    gpio_put(kStandbyPin, false);
#endif
    pwm_set_both_levels(pwm_gpio_to_slice_num(kIn1), 0, 0);
    pwm_set_both_levels(pwm_gpio_to_slice_num(kMotorInputPins[0]), 0, 0);
}

int64_t on_pulse_timeout(alarm_id_t, void*)
{
    inhibit();
    pulse_expired = true;
    return 0;
}

void stop(uint64_t now)
{
    inhibit();
    if (stop_alarm > 0) cancel_alarm(stop_alarm);
    stop_alarm = 0;
    pulse_expired = false;
    test.stop(now);
}

void status(uint64_t now)
{
    const auto sample = encoder_snapshot()[1];
    const uint32_t pwm_levels = pwm_hw->slice[pwm_gpio_to_slice_num(kIn1)].cc;
    printf("status t_ms=%llu armed=%u direction=%d duty_permille=%u ",
           static_cast<unsigned long long>(now), test.armed(now), test.direction(),
           test.duty());
#if WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED
    printf("stby=UNCONTROLLED gp14=UNTOUCHED ");
#else
    printf("stby=%u ", gpio_get(kStandbyPin));
#endif
    const uint32_t left_levels = pwm_hw->slice[pwm_gpio_to_slice_num(kMotorInputPins[0])].cc;
    printf(
           "left=%u,%u right=%u,%u pwm_cc=%u,%u pwm_counter=%u hall_ab=%u a_edges=%lu b_edges=%lu "
           "tentative_x4=%lld invalid=%lu last_edge_us=%llu positive=%lu negative=%lu\n",
           gpio_get(kMotorInputPins[0]), gpio_get(kMotorInputPins[1]),
           gpio_get(kIn1), gpio_get(kIn2),
           static_cast<unsigned int>(pwm_levels & 0xffff),
           static_cast<unsigned int>(pwm_levels >> 16),
           pwm_get_counter(pwm_gpio_to_slice_num(kIn1)), sample.counts.previous,
           static_cast<unsigned long>(sample.counts.a_edges),
           static_cast<unsigned long>(sample.counts.b_edges),
           static_cast<long long>(sample.counts.tentative_position),
           static_cast<unsigned long>(sample.counts.invalid_transitions),
           static_cast<unsigned long long>(sample.last_edge_us),
           static_cast<unsigned long>(sample.counts.positive_steps),
           static_cast<unsigned long>(sample.counts.negative_steps));
    printf("bridge_pwm left_cc=%u,%u right_cc=%u,%u\n",
           static_cast<unsigned int>(left_levels & 0xffff),
           static_cast<unsigned int>(left_levels >> 16),
           static_cast<unsigned int>(pwm_levels & 0xffff),
           static_cast<unsigned int>(pwm_levels >> 16));
}

void print_rate(uint64_t now)
{
    const auto sample = encoder_snapshot()[1];
    if (rate_previous_ms && now > rate_previous_ms) {
        const auto elapsed = now - rate_previous_ms;
        const auto positive = sample.counts.positive_steps - rate_previous.counts.positive_steps;
        const auto negative = sample.counts.negative_steps - rate_previous.counts.negative_steps;
        const auto invalid = sample.counts.invalid_transitions -
                             rate_previous.counts.invalid_transitions;
        const int64_t net = static_cast<int64_t>(positive) - negative;
        printf("rate window_ms=%llu positive=%lu negative=%lu invalid=%lu signed_x4_per_s=%.2f ",
               static_cast<unsigned long long>(elapsed), static_cast<unsigned long>(positive),
               static_cast<unsigned long>(negative), static_cast<unsigned long>(invalid),
               static_cast<double>(net) * 1000 / elapsed);
        if (calibration.counts_per_rev() <= 0 || invalid || (positive && negative)) {
            printf("motor_rpm=UNAVAILABLE reason=%s\n",
                   calibration.counts_per_rev() <= 0 ? "UNCALIBRATED" : "INVALID_OR_MIXED_DIRECTION");
        } else {
            printf("motor_rpm=%.2f cpr_x4=%.2f sign=HALL_CONVENTION\n",
                   encoder_rpm(net, elapsed, calibration.counts_per_rev()),
                   calibration.counts_per_rev());
        }
    }
    rate_previous = sample;
    rate_previous_ms = now;
}

void help()
{
    printf("MOTOR_BENCH_TEST USB CDC; no Bluetooth/servo; left enabled by both100 or left100.\n"
           "Commands + Enter: arm, fwd, rev, stop, status, help. ! or Ctrl-C stops immediately.\n"
           "fwd/rev: 25%%; explicit fwd50/rev50: 50%% diagnostic duty (never automatic).\n"
           "Short pulses: <=500 ms. fwd50long/rev50long: 50%%, <=5000 ms.\n"
           "fwd100/rev100: 100%%, <=1000 ms, fresh arm required.\n"
           "both100: BOTH motors electrical forward at 100%%, <=1000 ms, fresh arm required.\n"
           "left100: LEFT ONLY electrical forward at 100%%, <=2000 ms, fresh arm required.\n"
           "Re-arm after 500 ms cooldown. Keep wheel lifted/disconnect ready.\n"
           "Arm expires in 5 s; USB DTR required. fwd/rev are electrical polarity, not calibrated.\n");
    printf("Calibration: mark, turn MOTOR shaft exactly 10 revolutions one direction, cal10.\n"
           "Do not force shaft. Calibration is RAM-only; no power-cycle between mark/cal10.\n");
    printf("Hall motion every 100 ms; direction expires after 250 ms without edges.\n"
           "HALL_POSITIVE/NEGATIVE are measured signs, not commanded direction.\n"
           "NO_RECENT_EDGES does not prove stopped or distinguish a disconnected sensor.\n");
    printf("Stop policy: %s; not a mechanical brake.\n", kStopDescription);
}

enum class MotorSelection { kRight, kBoth, kLeft };

bool start_pulse(int direction, uint16_t duty, uint32_t duration, uint64_t now,
                 MotorSelection motors = MotorSelection::kRight)
{
    const bool left = motors == MotorSelection::kLeft;
    if ((motors != MotorSelection::kRight &&
         (direction != 1 || duty != RightMotorTest::kFullDutyPermille ||
          duration != (left ? RightMotorTest::kLeftFullDutyPulseMs : RightMotorTest::kFullDutyPulseMs))) ||
        (motors == MotorSelection::kRight && duration == RightMotorTest::kLeftFullDutyPulseMs)) {
        stop(now);
        printf("ERROR: invalid motor selection/pulse; both motors stopped/disarmed\n");
        return false;
    }
    calibration.cancel_mark();
    if (!test.start(direction, now, duty, duration)) {
        printf("ERROR: pulse rejected; stop, wait for cooldown, then arm first\n");
        return false;
    }
    before_pulse = encoder_snapshot()[1];
    pulse_started_ms = now;
    const uint32_t interrupts = save_and_disable_interrupts();
    pulse_expired = false;
    stop_alarm = add_alarm_in_ms(duration, on_pulse_timeout, nullptr, true);
    if (stop_alarm > 0) {
        const uint16_t level = (kPwmTop + 1) * duty / 1000;
        pwm_set_both_levels(pwm_gpio_to_slice_num(kIn1),
                            !left && direction > 0 ? level : 0,
                            !left && direction < 0 ? level : 0);
        pwm_set_both_levels(pwm_gpio_to_slice_num(kMotorInputPins[0]),
                            motors != MotorSelection::kRight ? level : 0, 0);
#if !WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED
        gpio_put(kStandbyPin, true);
#endif
    }
    restore_interrupts(interrupts);
    if (stop_alarm <= 0) {
        stop(now);
        printf("ERROR: stop alarm unavailable; pulse inhibited\n");
        return false;
    }
    printf("PULSE motors=%s direction=%d duty_permille=%u limit_ms=%lu\n",
           left ? "left" : motors == MotorSelection::kBoth ? "both" : "right",
           direction, duty, static_cast<unsigned long>(duration));
    return true;
}

void command(std::string_view line, uint64_t now)
{
    switch (motor_test_command(line)) {
    case MotorTestCommand::kArm:
        printf("%s\n", test.arm(now) ? "ARMED for one pulse; expires in 5 s"
                                           : "ERROR: running or cooling down");
        break;
    case MotorTestCommand::kBoth100:
        start_pulse(1, RightMotorTest::kFullDutyPermille,
                    RightMotorTest::kFullDutyPulseMs, now, MotorSelection::kBoth);
        break;
    case MotorTestCommand::kLeft100:
        start_pulse(1, RightMotorTest::kFullDutyPermille,
                    RightMotorTest::kLeftFullDutyPulseMs, now, MotorSelection::kLeft);
        break;
    case MotorTestCommand::kForward:
    case MotorTestCommand::kReverse:
    case MotorTestCommand::kForward50:
    case MotorTestCommand::kReverse50:
    case MotorTestCommand::kForward50Long:
    case MotorTestCommand::kReverse50Long:
    case MotorTestCommand::kForward100:
    case MotorTestCommand::kReverse100: {
        const auto request = motor_test_command(line);
        const int direction = request == MotorTestCommand::kForward ||
                                      request == MotorTestCommand::kForward50 ||
                                      request == MotorTestCommand::kForward50Long ||
                                      request == MotorTestCommand::kForward100 ? 1 : -1;
        const bool long_pulse = request == MotorTestCommand::kForward50Long ||
                                request == MotorTestCommand::kReverse50Long;
        const bool full_duty = request == MotorTestCommand::kForward100 ||
                               request == MotorTestCommand::kReverse100;
        const uint16_t duty = full_duty ? RightMotorTest::kFullDutyPermille :
                                     request == MotorTestCommand::kForward ||
                                     request == MotorTestCommand::kReverse
                                 ? RightMotorTest::kDutyPermille
                                 : RightMotorTest::kDiagnosticDutyPermille;
        const uint32_t duration = full_duty ? RightMotorTest::kFullDutyPulseMs :
                                 long_pulse ? RightMotorTest::kLongPulseMs : RightMotorTest::kPulseMs;
        start_pulse(direction, duty, duration, now);
        break;
    }
    case MotorTestCommand::kStop:
        stop(now);
        printf("STOPPED; %s; disarmed (not a mechanical brake)\n", kStopDescription);
        break;
    case MotorTestCommand::kStatus: status(now); break;
    case MotorTestCommand::kMark:
        stop(now);
        calibration.mark(encoder_snapshot()[1].counts);
        printf("MARKED; motor stopped/disarmed; hand-turn motor shaft exactly 10 revolutions\n");
        break;
    case MotorTestCommand::kCalibrate10:
        if (test.running()) {
            stop(now);
            printf("ERROR: calibration rejected during powered pulse; repeat mark and hand-turn\n");
            calibration.cancel_mark();
        } else if (!calibration.finish10(encoder_snapshot()[1].counts)) {
            printf("ERROR: calibration rejected; missing mark/edges, mixed direction or invalid transitions\n");
        } else {
            printf("CALIBRATED motor_cpr_x4=%.2f assumed_manual_turns=10 sign=HALL_CONVENTION\n",
                   calibration.counts_per_rev());
        }
        break;
    case MotorTestCommand::kHelp: help(); break;
    case MotorTestCommand::kInvalid:
        stop(now);
        printf("ERROR: unknown command; stopped/disarmed; type help\n");
        break;
    }
}
} // namespace

int main()
{
#if !WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED
    gpio_init(kStandbyPin);
    gpio_put(kStandbyPin, false);
    gpio_set_dir(kStandbyPin, GPIO_OUT);
#endif
    for (const auto pin : kMotorInputPins) {
        gpio_init(pin);
        gpio_put(pin, false);
        gpio_set_dir(pin, GPIO_OUT);
    }
    auto config = pwm_get_default_config();
    pwm_config_set_wrap(&config, kPwmTop);
    pwm_config_set_clkdiv(&config, static_cast<float>(clock_get_hz(clk_sys)) / 20000000);
    pwm_init(pwm_gpio_to_slice_num(kIn1), &config, true); // 20 kHz, shared slice for GP8/GP9.
    pwm_init(pwm_gpio_to_slice_num(kMotorInputPins[0]), &config, true);
    gpio_set_function(kIn1, GPIO_FUNC_PWM);
    gpio_set_function(kIn2, GPIO_FUNC_PWM);
    gpio_set_function(kMotorInputPins[0], GPIO_FUNC_PWM);
    gpio_set_function(kMotorInputPins[1], GPIO_FUNC_PWM);
    inhibit();
    initialize_encoder_capture(false);
#if WHISKEY_RIGHT_MOTOR_BOOT_PULSE
    const bool watchdog_reset = watchdog_caused_reboot();
    watchdog_enable(200, false);
    const uint64_t boot_ms = time_us_64() / 1000;
    while (time_us_64() / 1000 - boot_ms < 3000) {
        watchdog_update();
        sleep_ms(1);
    }
    // Never restart automatic motion following a watchdog-induced reset.
    if (!watchdog_reset) {
        const uint64_t now = time_us_64() / 1000;
        if (!test.arm(now)) panic("Boot pulse arming failed; PWM inhibited");
#if WHISKEY_RIGHT_MOTOR_BOOT_FULL_DUTY
        constexpr auto duty = RightMotorTest::kFullDutyPermille;
        constexpr auto duration = RightMotorTest::kFullDutyPulseMs;
#else
        constexpr auto duty = RightMotorTest::kDiagnosticDutyPermille;
        constexpr auto duration = RightMotorTest::kLongPulseMs;
#endif
        if (!start_pulse(1, duty, duration, now)) {
            panic("Boot pulse startup failed; PWM inhibited");
        }
        while (test.running() && !pulse_expired) {
            test.update(time_us_64() / 1000);
            watchdog_update();
            sleep_ms(1);
        }
    }
    stop(time_us_64() / 1000);
    while (true) {
        watchdog_update();
        sleep_ms(10);
    }
#endif
    if (!stdio_usb_init()) panic("USB stdio initialization failed; motor inhibited");
    watchdog_enable(200, false);
    bool connected_before = false;
    char line[32] = {};
    unsigned int length = 0;
    bool discard_line = false;
    uint64_t next_status = 0;
    uint64_t next_motion_ms = 0;
    while (true) {
        const uint64_t now = time_us_64() / 1000;
        const bool connected = stdio_usb_connected();
        if (!connected) {
            stop(now);
            length = 0;
            discard_line = false;
        } else if (!connected_before) {
            stop(now);
            help();
            printf("watchdog_reboot=%u; outputs inhibited until arm + direction\n",
                   watchdog_caused_reboot());
        }
        connected_before = connected;
        const bool was_running = test.running();
        test.update(now);
        if (pulse_expired || (was_running && !test.running())) {
            stop(now);
            const auto after = encoder_snapshot()[1];
            printf("PULSE_DONE stop_policy=\"%s\" observed_ms=%llu delta_a=%lu delta_b=%lu "
                   "delta_x4=%lld delta_invalid=%lu delta_positive=%lu delta_negative=%lu\n",
                   kStopDescription,
                   static_cast<unsigned long long>(now - pulse_started_ms),
                   static_cast<unsigned long>(after.counts.a_edges - before_pulse.counts.a_edges),
                   static_cast<unsigned long>(after.counts.b_edges - before_pulse.counts.b_edges),
                   static_cast<long long>(after.counts.tentative_position -
                                          before_pulse.counts.tentative_position),
                   static_cast<unsigned long>(after.counts.invalid_transitions -
                                              before_pulse.counts.invalid_transitions),
                   static_cast<unsigned long>(after.counts.positive_steps -
                                              before_pulse.counts.positive_steps),
                   static_cast<unsigned long>(after.counts.negative_steps -
                                              before_pulse.counts.negative_steps));
        }
        for (unsigned int budget = 0; connected && budget < 32; ++budget) {
            const int input = getchar_timeout_us(0);
            if (input == PICO_ERROR_TIMEOUT) break;
            if (input == '!' || input == 3) {
                stop(time_us_64() / 1000);
                length = 0;
                discard_line = true;
                printf("STOPPED; disarmed\n");
            } else if (input == '\r' || input == '\n') {
                if (!discard_line && length) command({line, length}, time_us_64() / 1000);
                length = 0;
                discard_line = false;
            } else if (!discard_line) {
                if (input < 32 || input > 126 || length == sizeof(line)) {
                    stop(time_us_64() / 1000);
                    length = 0;
                    discard_line = true;
                    printf("ERROR: invalid/overlong line; stopped/disarmed\n");
                } else {
                    line[length++] = static_cast<char>(input);
                }
            }
        }
        if (connected && now >= next_status) {
            status(now);
            print_rate(now);
            next_status = now + 1000;
        }
        if (connected && now >= next_motion_ms) {
            print_motion();
            next_motion_ms = now + 100;
        }
        watchdog_update();
        sleep_ms(1);
    }
}
