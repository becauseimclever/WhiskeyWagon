#include <cstdio>

#include "hardware/uart.h"
#include "pico/stdlib.h"

namespace {
constexpr unsigned int kUartBaudRate = 115200;
constexpr unsigned int kUartTxPin = 4;
constexpr unsigned int kUartRxPin = 5;
constexpr unsigned int kHeartbeatIntervalMs = 1000;
}  // namespace

int main()
{
    stdio_init_all();

    // UART1 is separate from the default UART0 used by stdio.
    uart_init(uart1, kUartBaudRate);
    gpio_set_function(kUartTxPin, GPIO_FUNC_UART);
    gpio_set_function(kUartRxPin, GPIO_FUNC_UART);
    uart_puts(uart1, " Hello, UART!\n");

    while (true) {
        std::printf("Hello, world!\n");
        sleep_ms(kHeartbeatIntervalMs);
    }
}
