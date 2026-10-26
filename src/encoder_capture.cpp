#include "encoder_capture.hpp"

#include "board_config.hpp"
#include "hardware/sync.h"
#include "pico/stdlib.h"

namespace whiskey_wagon {
namespace {
std::array<EncoderSnapshot, 2> counters = {};

#if WHISKEY_ENCODER_CAPTURE_VERIFIED
uint8_t pin_state(unsigned int motor)
{
    return (gpio_get(kEncoderPins[motor * 2]) ? 2 : 0) |
           (gpio_get(kEncoderPins[motor * 2 + 1]) ? 1 : 0);
}

void on_edge(unsigned int pin, uint32_t events)
{
    (void)events;
    const unsigned int motor = pin < kEncoderPins[2] ? 0 : 1;
    counters[motor].counts.sample(pin_state(motor));
    counters[motor].last_edge_us = time_us_64();
}
#endif
} // namespace

void initialize_encoder_capture()
{
#if WHISKEY_ENCODER_CAPTURE_VERIFIED
    for (const auto pin : kEncoderPins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_disable_pulls(pin);
    }
    for (unsigned int motor = 0; motor < counters.size(); ++motor) {
        counters[motor].counts.previous = pin_state(motor);
    }
    for (const auto pin : kEncoderPins) {
        gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                          true, on_edge);
    }
#endif
}

std::array<EncoderSnapshot, 2> encoder_snapshot()
{
    const uint32_t interrupts = save_and_disable_interrupts();
    const auto result = counters;
    restore_interrupts(interrupts);
    return result;
}
} // namespace whiskey_wagon
