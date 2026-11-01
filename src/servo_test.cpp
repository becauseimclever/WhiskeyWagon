#include <cstdio>
#include <string_view>

#include "board_config.hpp"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "servo_test_logic.hpp"

namespace {
using namespace whiskey_wagon;
constexpr uint16_t kPwmTop = 19999;
ServoTest test;
alarm_id_t stop_alarm = 0;
volatile bool burst_expired = false;

void initialize_low_output(unsigned int pin)
{
    gpio_init(pin);
    gpio_put(pin, false);
    gpio_set_dir(pin, GPIO_OUT);
}

void inhibit_signal()
{
    gpio_put(kServoPin, false);
    gpio_set_function(kServoPin, GPIO_FUNC_SIO);
    const auto slice = pwm_gpio_to_slice_num(kServoPin);
    pwm_set_enabled(slice, false);
    pwm_set_gpio_level(kServoPin, 0);
}

int64_t on_burst_timeout(alarm_id_t, void*)
{
    inhibit_signal();
    burst_expired = true;
    return 0;
}

void stop(uint64_t now)
{
    const uint32_t interrupts = save_and_disable_interrupts();
    inhibit_signal();
    if (stop_alarm > 0) cancel_alarm(stop_alarm);
    stop_alarm = 0;
    burst_expired = false;
    restore_interrupts(interrupts);
    test.stop(now);
}

void help()
{
    printf("SERVO_TEST USB CDC; motor power OFF; GP14/STBY UNCONTROLLED.\n"
           "Disconnect linkage; a pulse can move the servo immediately.\n"
           "Commands + Enter: arm, pulse NNNN, stop, status, help.\n"
           "One 0544-2400 us pulse train at nominal 50 Hz for 1 s; fresh arm each time.\n"
           "Arm expires in 5 s; 500 ms cooldown; USB DTR required.\n"
           "! or Ctrl-C stops immediately; send Enter to clear the input line.\n"
           "Signal LOW is not servo power removal or a mechanical brake.\n");
}

void status(uint64_t now)
{
    const bool active = test.running() && !burst_expired;
    printf("status armed=%u active=%u requested_us=%u gp2_command=%s "
           "motor_inputs=LOW gp14=UNTOUCHED stby=UNCONTROLLED\n",
           static_cast<unsigned int>(test.armed(now)),
           static_cast<unsigned int>(active),
           static_cast<unsigned int>(test.pulse_us()),
           active ? "PWM" : "LOW");
}

void start_burst(uint16_t pulse_us, uint64_t now)
{
    if (!stdio_usb_connected() || !test.start(pulse_us, now)) {
        stop(now);
        printf("ERROR: request rejected; check DTR, fresh arm, cooldown and 0544-2400 us limit\n");
        return;
    }

    const uint32_t interrupts = save_and_disable_interrupts();
    burst_expired = false;
    stop_alarm = add_alarm_in_ms(ServoTest::kBurstMs, on_burst_timeout, nullptr, true);
    if (stop_alarm > 0) {
        const auto slice = pwm_gpio_to_slice_num(kServoPin);
        pwm_set_counter(slice, 0);
        pwm_set_gpio_level(kServoPin, pulse_us);
        gpio_set_function(kServoPin, GPIO_FUNC_PWM);
        pwm_set_enabled(slice, true);
    }
    restore_interrupts(interrupts);

    if (stop_alarm <= 0) {
        stop(now);
        printf("ERROR: stop alarm unavailable; servo signal inhibited\n");
        return;
    }
    printf("BURST requested_us=%u frame_us=20000 limit_ms=%lu; waveform not measured\n",
           static_cast<unsigned int>(pulse_us), static_cast<unsigned long>(ServoTest::kBurstMs));
}

void command(std::string_view line, uint64_t now)
{
    if (!stdio_usb_connected()) {
        stop(now);
        return;
    }
    const auto request = servo_test_command(line);
    switch (request.command) {
    case ServoTestCommand::kArm:
        if (test.arm(now)) {
            printf("ARMED for one burst; expires in 5 s\n");
        } else {
            stop(now);
            printf("ERROR: active or cooling down; signal inhibited and disarmed\n");
        }
        break;
    case ServoTestCommand::kPulse: start_burst(request.pulse_us, now); break;
    case ServoTestCommand::kStop:
        stop(now);
        printf("STOPPED; signal LOW, disarmed; servo power may still be on\n");
        break;
    case ServoTestCommand::kStatus: status(now); break;
    case ServoTestCommand::kHelp: help(); break;
    case ServoTestCommand::kInvalid:
        stop(now);
        printf("ERROR: invalid command; signal inhibited and disarmed; type help\n");
        break;
    }
}
} // namespace

int main()
{
    initialize_low_output(kServoPin);
    for (const auto pin : kMotorInputPins) initialize_low_output(pin);

    const uint32_t system_hz = clock_get_hz(clk_sys);
    if (!servo_clock_supported(system_hz)) panic("Unsupported servo PWM clock; signal inhibited");
    const uint32_t divider16 = system_hz / 62500;
    auto config = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac4(&config, divider16 / 16, divider16 % 16);
    pwm_config_set_wrap(&config, kPwmTop);
    pwm_init(pwm_gpio_to_slice_num(kServoPin), &config, false);
    inhibit_signal();

    if (!stdio_usb_init()) panic("USB stdio initialization failed; servo signal inhibited");
    watchdog_enable(200, false);
    bool connected_before = false;
    bool draining = false;
    char line[32] = {};
    unsigned int length = 0;
    bool discard_line = false;

    while (true) {
        const uint64_t now = time_us_64() / 1000;
        const bool connected = stdio_usb_connected();
        if (!connected) {
            if (connected_before || test.running() || test.armed(now)) stop(now);
            length = 0;
            discard_line = false;
            draining = false;
        } else if (!connected_before) {
            stop(now);
            length = 0;
            discard_line = false;
            draining = true;
        }
        connected_before = connected;

        const bool was_running = test.running();
        const uint16_t previous_pulse = test.pulse_us();
        test.update(now);
        if (burst_expired || (was_running && !test.running())) {
            stop(now);
            printf("BURST_DONE requested_us=%u; signal LOW; servo power may still be on\n",
                   static_cast<unsigned int>(previous_pulse));
        }

        for (unsigned int budget = 0; connected && budget < 32; ++budget) {
            if (!stdio_usb_connected()) {
                stop(time_us_64() / 1000);
                break;
            }
            const int input = getchar_timeout_us(0);
            if (input == PICO_ERROR_TIMEOUT) {
                if (draining) {
                    draining = false;
                    help();
                    printf("READY disarmed; watchdog_reboot=%u\n",
                           static_cast<unsigned int>(watchdog_caused_reboot()));
                }
                break;
            }
            if (draining) continue;
            if (input == '!' || input == 3) {
                stop(time_us_64() / 1000);
                length = 0;
                discard_line = true;
                printf("STOPPED; signal LOW, disarmed; servo power may still be on\n");
            } else if (input == '\r' || input == '\n') {
                if (!discard_line && length) command({line, length}, time_us_64() / 1000);
                length = 0;
                discard_line = false;
            } else if (!discard_line) {
                if (input < 32 || input > 126 || length == sizeof(line)) {
                    stop(time_us_64() / 1000);
                    length = 0;
                    discard_line = true;
                    printf("ERROR: invalid/overlong line; signal inhibited and disarmed\n");
                } else {
                    line[length++] = static_cast<char>(input);
                }
            }
        }
        watchdog_update();
        sleep_ms(1);
    }
}
