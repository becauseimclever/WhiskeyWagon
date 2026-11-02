#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bench_logic.hpp"
#include "bluetooth_adapter.h"
#include "board_config.hpp"
#include "btstack_run_loop.h"
#include "encoder_capture.hpp"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#if WHISKEY_WAGON_POC
#include "poc_outputs.hpp"
#endif
#if WHISKEY_WAGON_POC && WHISKEY_POC_USB_CONSOLE
#include "pico/stdio_usb.h"
#else
#include "pico/stdio_uart.h"
#endif

namespace {
using namespace whiskey_wagon;
#if !WHISKEY_WAGON_POC
static_assert(!kMotorOutputsEnabled, "This bench firmware must not energize the motors");
#endif
constexpr unsigned int kControlIntervalMs = 10;
constexpr unsigned int kHeartbeatIntervalMs = 1000;
btstack_timer_source_t control_timer = {};
#if WHISKEY_WAGON_POC
PocControl control;
#else
BenchControl control;
#endif
LinkLifecycle lifecycle;
PairButton pairing_button;
bool lifecycle_started = false;
bool was_connected = false;
uint32_t connected_ms = 0;
uint64_t last_tick_ms = 0;
uint64_t next_heartbeat_ms = 0;
BluetoothInput last_logged_input = {};
bool input_dirty = false;
bool have_logged_input = false;
uint64_t next_input_log_ms = 0;

void initialize_low_output(unsigned int pin)
{
    gpio_init(pin);
    gpio_put(pin, false);
    gpio_set_dir(pin, GPIO_OUT);
}

const char* mode_name(LinkMode mode)
{
    switch (mode) {
    case LinkMode::kReconnect: return "reconnect";
    case LinkMode::kPairing: return "pairing";
    case LinkMode::kConnected: return "connected";
    case LinkMode::kSleeping: return "sleeping";
    }
    panic("Invalid lifecycle mode");
}

void apply_mode()
{
    printf("lifecycle mode=%s uptime_ms=%llu\n", mode_name(lifecycle.mode()),
           static_cast<unsigned long long>(time_us_64() / 1000));
    switch (lifecycle.mode()) {
    case LinkMode::kReconnect: bluetooth_set_mode(BLUETOOTH_RECONNECT); break;
    case LinkMode::kPairing: bluetooth_set_mode(BLUETOOTH_PAIRING); break;
    case LinkMode::kConnected: bluetooth_set_mode(BLUETOOTH_CONNECTED); break;
    case LinkMode::kSleeping:
        control.sleep();
#if WHISKEY_WAGON_POC
        inhibit_poc_outputs();
#endif
        bluetooth_set_mode(BLUETOOTH_SLEEPING);
        printf("sleep: software idle, NOT power removal; GP15 short press wakes by reboot\n");
        break;
    }
}

void on_input(const BluetoothInput* input)
{
    if (!input) {
        control.connection_reset();
        have_logged_input = false;
        input_dirty = false;
        return;
    }
    const DriveInput drive_input = {(input->buttons & kForwardButtonMask) != 0,
                    (input->buttons & kReverseButtonMask) != 0,
                    (input->buttons & kCoastButtonMask) != 0,
                    (input->dpad & (1u << 3)) != 0,
                    (input->dpad & (1u << 2)) != 0,
                    stick_steering(input->axes[0])};
#if WHISKEY_WAGON_POC
    control.report(drive_input, btstack_run_loop_get_time_ms());
    apply_poc_outputs(control.outputs());
#else
    control.report(drive_input);
#endif
    if (!have_logged_input || input->buttons != last_logged_input.buttons ||
        input->dpad != last_logged_input.dpad || input->misc != last_logged_input.misc ||
        std::memcmp(input->axes, last_logged_input.axes, sizeof(input->axes)) != 0) {
        input_dirty = true;
    }
}

void print_input(uint64_t now)
{
    if (!input_dirty || now < next_input_log_ms) {
        return;
    }
    const auto* input = bluetooth_input();
    printf("input t_ms=%lu buttons=0x%04x dpad=0x%02x misc=0x%02x "
           "axes=%ld,%ld,%ld,%ld reports=%lu age_ms=%lu max_gap_ms=%lu mapping=UNVERIFIED\n",
           static_cast<unsigned long>(input->last_report_ms), input->buttons, input->dpad,
           input->misc, static_cast<long>(input->axes[0]), static_cast<long>(input->axes[1]),
           static_cast<long>(input->axes[2]), static_cast<long>(input->axes[3]),
           static_cast<unsigned long>(input->reports),
           static_cast<unsigned long>(static_cast<uint32_t>(now) - input->last_report_ms),
           static_cast<unsigned long>(input->max_gap_ms));
    last_logged_input = *input;
    have_logged_input = true;
    input_dirty = false;
    next_input_log_ms = now + 100; // Coalesce diagnostics, never coalesce control events.
}

void print_status(uint64_t now)
{
#if WHISKEY_WAGON_POC
    const auto outputs = control.outputs();
    const auto* input = bluetooth_input();
    printf("poc t_ms=%llu enabled=%u stby=%s gp14=%s link=%s lost=%u qualified=%u fault=%u "
           "demand_duty=%d,%d demand_servo_us=%u reports=%lu age_ms=%lu max_gap_ms=%lu "
           "encoder=%u idle_stalls=%lu max_idle_stall_ms=%lu duty_cap=%u "
           "acceleration_ms=%lu\n",
           static_cast<unsigned long long>(now), WHISKEY_POC_OUTPUTS_ENABLED,
           WHISKEY_POC_EXTERNAL_STBY_BENCH ? "UNCONTROLLED" : "GP14_OUTPUT",
           WHISKEY_POC_EXTERNAL_STBY_BENCH ? "UNTOUCHED" : "OUTPUT",
           lifecycle_started ? mode_name(lifecycle.mode()) : "starting",
           control.lost(), control.qualified(), static_cast<unsigned int>(control.fault()),
           outputs.motor_permille[0], outputs.motor_permille[1], outputs.servo_us,
           static_cast<unsigned long>(input->reports),
           static_cast<unsigned long>(btstack_run_loop_get_time_ms() - input->last_report_ms),
           static_cast<unsigned long>(input->max_gap_ms), WHISKEY_ENCODER_CAPTURE_VERIFIED,
           static_cast<unsigned long>(control.idle_overruns()),
           static_cast<unsigned long>(control.max_idle_overrun_ms()), kPocMaxDuty,
           static_cast<unsigned long>(kPocAccelerationMs));
#if WHISKEY_ENCODER_CAPTURE_VERIFIED
    const auto snapshots = encoder_snapshot();
    static std::array<int64_t, 2> previous_position = {};
    static uint64_t previous_rpm_ms = 0;
    const auto rpm_elapsed = static_cast<uint32_t>(now - previous_rpm_ms);
    std::array<int32_t, 2> motor_rpm = {};
    std::array<int32_t, 2> wheel_rpm = {};
    for (unsigned int motor = 0; motor < 2; ++motor) {
        const int64_t delta = snapshots[motor].counts.tentative_position - previous_position[motor];
        if (previous_rpm_ms != 0) {
            motor_rpm[motor] = encoder_rpm_tenths(delta, rpm_elapsed);
            wheel_rpm[motor] = encoder_rpm_tenths(
                delta, rpm_elapsed, kDriveMotorProfiles[motor].gearbox_reduction);
        }
        previous_position[motor] = snapshots[motor].counts.tentative_position;
    }
    previous_rpm_ms = now;
    printf("hall left_x4=%lld right_x4=%lld invalid=%lu,%lu edge_us=%llu,%llu "
           "motor_rpm=%ld,%ld wheel_rpm=%s%ld.%ld,%s%ld.%ld\n",
           static_cast<long long>(snapshots[0].counts.tentative_position),
           static_cast<long long>(snapshots[1].counts.tentative_position),
           static_cast<unsigned long>(snapshots[0].counts.invalid_transitions),
           static_cast<unsigned long>(snapshots[1].counts.invalid_transitions),
           static_cast<unsigned long long>(snapshots[0].last_edge_us),
           static_cast<unsigned long long>(snapshots[1].last_edge_us),
           static_cast<long>(motor_rpm[0] / 10), static_cast<long>(motor_rpm[1] / 10),
           wheel_rpm[0] < 0 ? "-" : "", static_cast<long>(std::abs(wheel_rpm[0]) / 10),
           static_cast<long>(std::abs(wheel_rpm[0]) % 10),
           wheel_rpm[1] < 0 ? "-" : "", static_cast<long>(std::abs(wheel_rpm[1]) / 10),
           static_cast<long>(std::abs(wheel_rpm[1]) % 10));
    const auto& regulator = control.regulator();
    const auto tenths = [](float rpm) { return static_cast<long>(rpm * 10); };
    printf("drive target_rpm_x10=%ld,%ld measured_rpm_x10=%ld,%ld wheel_duty=%d,%d "
           "saturated=%u,%u sync_scale_pct=%ld max_rpm=%ld steer_lock_deg=%ld\n",
           tenths(regulator.wheel(0).target()), tenths(regulator.wheel(1).target()),
           tenths(regulator.wheel(0).rpm()), tenths(regulator.wheel(1).rpm()),
           regulator.wheel(0).duty(), regulator.wheel(1).duty(),
           regulator.wheel(0).saturated(), regulator.wheel(1).saturated(),
           static_cast<long>(regulator.scale() * 100), static_cast<long>(kMaxWheelRpm),
           static_cast<long>(kSteerLockDeg));
#endif
#else
    printf("status uptime_ms=%llu mode=INHIBITED_ONLY link=%s bluetooth=%s connected=%u "
           "stby_gp14=%u ain1_gp6=%u ain2_gp7=%u bin1_gp8=%u bin2_gp9=%u "
           "servo=off encoder_capture=%s rpm=UNAVAILABLE\n",
           static_cast<unsigned long long>(now),
           lifecycle_started ? mode_name(lifecycle.mode()) : "starting", bluetooth_status(),
           static_cast<unsigned int>(bluetooth_connected()),
           static_cast<unsigned int>(gpio_get(kStandbyPin)),
           static_cast<unsigned int>(gpio_get(kMotorInputPins[0])),
           static_cast<unsigned int>(gpio_get(kMotorInputPins[1])),
           static_cast<unsigned int>(gpio_get(kMotorInputPins[2])),
           static_cast<unsigned int>(gpio_get(kMotorInputPins[3])),
           WHISKEY_ENCODER_CAPTURE_VERIFIED ? "raw_edges" : "DISABLED_UNVERIFIED_INTERFACE");
    printf("preview drive_permille=%d steering_permille=%d coast=%u input_qualified=%u "
           "physical_mapping=UNVERIFIED differential=UNCALIBRATED speed_loop=NOT_ENABLED\n",
           static_cast<int>(control.drive() * 1000),
           static_cast<int>(control.steering() * 1000), control.coast(), control.qualified());
#if WHISKEY_ENCODER_CAPTURE_VERIFIED
    const auto snapshots = encoder_snapshot();
    for (unsigned int motor = 0; motor < snapshots.size(); ++motor) {
        const auto& sample = snapshots[motor];
        printf("encoder motor=%s a_edges=%lu b_edges=%lu tentative_x4=%lld "
               "invalid=%lu ab=%u last_edge_us=%llu phase=UNVERIFIED cpr=UNKNOWN\n",
               motor == 0 ? "left" : "right", static_cast<unsigned long>(sample.counts.a_edges),
               static_cast<unsigned long>(sample.counts.b_edges),
               static_cast<long long>(sample.counts.tentative_position),
               static_cast<unsigned long>(sample.counts.invalid_transitions),
               sample.counts.previous, static_cast<unsigned long long>(sample.last_edge_us));
    }
#endif
    const auto* input = bluetooth_input();
    if (input->reports) {
        printf("reports total=%lu age_ms=%lu max_gap_ms=%lu (no guessed stale-report timeout)\n",
               static_cast<unsigned long>(input->reports),
               static_cast<unsigned long>(static_cast<uint32_t>(now) - input->last_report_ms),
               static_cast<unsigned long>(input->max_gap_ms));
    }
#endif
}

void on_control_tick(btstack_timer_source_t* timer)
{
    const uint64_t now = time_us_64() / 1000;
    const auto elapsed = static_cast<uint32_t>(now - last_tick_ms);
    last_tick_ms = now;
    const bool sleeping = lifecycle_started && lifecycle.mode() == LinkMode::kSleeping;
    const auto button = pairing_button.update(!gpio_get(kPairingButtonPin), now);
    if (sleeping && button == ButtonEvent::kShortPress) {
        printf("wake: rebooting into normal reconnect/pairing lifecycle\n");
        stdio_flush();
        watchdog_reboot(0, 0, 10);
    }
    if (std::strcmp(bluetooth_status(), "failed") == 0) {
#if WHISKEY_WAGON_POC
        inhibit_poc_outputs();
#if WHISKEY_POC_EXTERNAL_STBY_BENCH
        panic("Bluetooth failed; motor PWM set to zero; STBY uncontrolled");
#endif
#endif
        panic("Bluetooth failed; outputs remain inhibited");
    }
    if (bluetooth_ready()) {
        if (!lifecycle_started) {
            lifecycle.start(bluetooth_remembered(), now);
            lifecycle_started = true;
            apply_mode();
        }
        const auto previous = lifecycle.mode();
        const bool connected = bluetooth_connected();
        if (!sleeping) {
            if (connected && !was_connected) {
                lifecycle.connected();
                connected_ms = btstack_run_loop_get_time_ms();
            }
            if (!connected && was_connected) {
                control.disconnect();
                lifecycle.disconnected(now);
            }
            if (button == ButtonEvent::kHold) {
                control.disconnect();
                lifecycle.pair(now);
            }
            lifecycle.update(now);
            if (lifecycle.mode() != previous) apply_mode();
        }
        was_connected = bluetooth_connected();
    } else if (now > 15000) {
#if WHISKEY_WAGON_POC
        inhibit_poc_outputs();
#if WHISKEY_POC_EXTERNAL_STBY_BENCH
        panic("Bluetooth setup not ready after 15 s; motor PWM zero; STBY uncontrolled");
#endif
#endif
        panic("Bluetooth setup not ready after 15 s; outputs inhibited; inspect UART boot log");
    }
    if (!sleeping && lifecycle.mode() != LinkMode::kSleeping) {
#if WHISKEY_WAGON_POC
        const auto previous_fault = control.fault();
        const uint32_t control_now = btstack_run_loop_get_time_ms();
        // Change-only controllers stay silent while held; disconnect releases via on_input.
        if (bluetooth_connected()) {
            control.quiet_link(control_now, connected_ms);
            control.link_alive(control_now);
        }
        control.tick(control_now, elapsed, encoder_snapshot(),
                     WHISKEY_POC_OUTPUTS_ENABLED != 0);
        apply_poc_outputs(control.outputs());
        if (control.fault() != previous_fault)
            printf("ERROR: latched POC fault=%u motor=%s elapsed_ms=%lu; motor inputs commanded zero, "
                   "servo off; stby=%s; reboot required\n",
                   static_cast<unsigned int>(control.fault()),
                   control.fault_motor() == 0 ? "left" : control.fault_motor() == 1 ? "right" : "-",
                   static_cast<unsigned long>(elapsed),
                   WHISKEY_POC_EXTERNAL_STBY_BENCH ? "UNCONTROLLED" : "GP14_COMMANDED_LOW");
#else
        control.tick(elapsed);
#endif
        print_input(now);
    }
#if WHISKEY_WAGON_POC
    watchdog_update();
#endif
    if (now >= next_heartbeat_ms) {
        print_status(now);
        next_heartbeat_ms = now + kHeartbeatIntervalMs;
    }
    btstack_run_loop_set_timer(timer, kControlIntervalMs);
    btstack_run_loop_add_timer(timer);
}
} // namespace

int main()
{
#if !WHISKEY_WAGON_POC || !WHISKEY_POC_EXTERNAL_STBY_BENCH
    initialize_low_output(kStandbyPin);
#endif
    for (const auto pin : kMotorInputPins) initialize_low_output(pin);
#if WHISKEY_WAGON_POC
    initialize_low_output(kServoPin);
    initialize_poc_outputs();
#endif
    gpio_init(kPairingButtonPin);
    gpio_set_dir(kPairingButtonPin, GPIO_IN);
    gpio_pull_up(kPairingButtonPin);

#if WHISKEY_WAGON_POC && WHISKEY_POC_USB_CONSOLE
    // Output-only bench console; lines are dropped while no host asserts DTR.
    if (!stdio_usb_init()) panic("USB console initialization failed; outputs inhibited");
#else
    stdout_uart_init();
#endif
#if WHISKEY_WAGON_POC
    printf("WhiskeyWagon CLOSED_LOOP_POC; actuators=%s; console=%s; stby=%s; gp14=%s; "
           "per-wheel encoder speed regulation with steering differential.\n"
           "Motors: 6 V, 164:1 measured, rated output 35 rpm; running/stall 150/500 mA each.\n"
           "duty_cap=%u acceleration_ms=%lu report_lease_ms=%lu servo_us=%u,%u,%u; "
           "calibration required.\n",
           WHISKEY_POC_EXTERNAL_STBY_BENCH ? "ENABLED_EXTERNAL_STBY_BENCH_ONLY" :
               WHISKEY_POC_OUTPUTS_ENABLED ? "ENABLED_SUPERVISED_BENCH_ONLY" : "INHIBITED",
           WHISKEY_POC_USB_CONSOLE ? "USB_CDC" : "UART0",
           WHISKEY_POC_EXTERNAL_STBY_BENCH ? "UNCONTROLLED" : "GP14_OUTPUT",
           WHISKEY_POC_EXTERNAL_STBY_BENCH ? "UNTOUCHED" : "OUTPUT",
           kPocMaxDuty, static_cast<unsigned long>(kPocAccelerationMs),
           static_cast<unsigned long>(kPocReportTimeoutMs),
           kServoLeftUs, kServoCenterUs, kServoRightUs);
#if WHISKEY_POC_EXTERNAL_STBY_BENCH
    printf("WARNING: STBY is externally enabled; PWM zero cannot disable the driver. "
           "Reset inhibition is unverified; use the physical motor-power cutoff.\n");
#endif
#else
    printf("WhiskeyWagon BENCH P1; INHIBITED_ONLY; keep VM/motors disconnected.\n"
           "GP15: momentary to GND, 2 s hold pairs, short press wakes software sleep.\n"
           "D-input Micro: mapping unverified, preview only; no motor PWM or servo pulses.\n");
#endif
    initialize_encoder_capture();
    if (!bluetooth_initialize(on_input)) {
#if WHISKEY_WAGON_POC && WHISKEY_POC_EXTERNAL_STBY_BENCH
        panic("Bluetooth startup failed; motor PWM zero; STBY uncontrolled");
#else
        panic("Bluetooth startup failed; motor output inhibited");
#endif
    }
#if WHISKEY_WAGON_POC
    watchdog_enable(200, false);
#endif
    last_tick_ms = time_us_64() / 1000;
    btstack_run_loop_set_timer_handler(&control_timer, on_control_tick);
    btstack_run_loop_set_timer(&control_timer, kControlIntervalMs);
    btstack_run_loop_add_timer(&control_timer);
    btstack_run_loop_execute();
#if WHISKEY_WAGON_POC
    inhibit_poc_outputs();
#if WHISKEY_POC_EXTERNAL_STBY_BENCH
    panic("Bluetooth run loop exited; motor PWM zero; STBY uncontrolled");
#endif
#endif
    panic("Bluetooth run loop exited; outputs inhibited");
}
