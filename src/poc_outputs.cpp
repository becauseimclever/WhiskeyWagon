#include "poc_outputs.hpp"

#include "board_config.hpp"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

namespace whiskey_wagon {
void inhibit_poc_outputs()
{
    gpio_put(kStandbyPin, false);
#if WHISKEY_POC_ACTUATORS_VERIFIED
    for (unsigned int motor = 0; motor < 2; ++motor)
        pwm_set_both_levels(pwm_gpio_to_slice_num(kMotorInputPins[motor * 2]), 0, 0);
    pwm_set_gpio_level(kServoPin, 0);
#endif
}

void initialize_poc_outputs()
{
#if WHISKEY_POC_ACTUATORS_VERIFIED
    for (unsigned int motor = 0; motor < 2; ++motor) {
        const auto pin = kMotorInputPins[motor * 2];
        auto config = pwm_get_default_config();
        pwm_config_set_clkdiv(&config, static_cast<float>(clock_get_hz(clk_sys)) / 20000000);
        pwm_config_set_wrap(&config, 999);
        const auto slice = pwm_gpio_to_slice_num(pin);
        pwm_init(slice, &config, false);
        pwm_set_both_levels(slice, 0, 0);
        gpio_set_function(pin, GPIO_FUNC_PWM);
        gpio_set_function(pin + 1, GPIO_FUNC_PWM);
        pwm_set_enabled(slice, true);
    }
    auto servo_config = pwm_get_default_config();
    pwm_config_set_clkdiv(&servo_config, static_cast<float>(clock_get_hz(clk_sys)) / 1000000);
    pwm_config_set_wrap(&servo_config, 19999);
    const auto servo_slice = pwm_gpio_to_slice_num(kServoPin);
    pwm_init(servo_slice, &servo_config, false);
    pwm_set_gpio_level(kServoPin, 0);
    gpio_set_function(kServoPin, GPIO_FUNC_PWM);
    pwm_set_enabled(servo_slice, true);
#endif
}

void apply_poc_outputs(PocOutputs outputs)
{
#if WHISKEY_POC_ACTUATORS_VERIFIED
    const bool driving = outputs.motor_permille[0] || outputs.motor_permille[1];
    if (!driving) gpio_put(kStandbyPin, false);
    for (unsigned int motor = 0; motor < 2; ++motor) {
        const int duty = outputs.motor_permille[motor];
        pwm_set_both_levels(pwm_gpio_to_slice_num(kMotorInputPins[motor * 2]),
                            duty > 0 ? duty : 0, duty < 0 ? -duty : 0);
    }
    pwm_set_gpio_level(kServoPin, outputs.servo_us);
    if (driving) gpio_put(kStandbyPin, true);
#else
    (void)outputs;
#endif
}
} // namespace whiskey_wagon
