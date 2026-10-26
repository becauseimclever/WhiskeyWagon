#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t buttons;
    uint8_t dpad;
    uint8_t misc;
    int32_t axes[4];
    uint32_t reports;
    uint32_t last_report_ms;
    uint32_t max_gap_ms;
} BluetoothInput;

// NULL clears control history on connection/disconnection; reports run on the BTstack loop.
typedef void (*BluetoothInputHandler)(const BluetoothInput* input);

typedef enum {
    BLUETOOTH_RECONNECT,
    BLUETOOTH_PAIRING,
    BLUETOOTH_CONNECTED,
    BLUETOOTH_SLEEPING,
} BluetoothMode;

bool bluetooth_initialize(BluetoothInputHandler handler);
const char* bluetooth_status(void);
bool bluetooth_ready(void);
bool bluetooth_connected(void);
bool bluetooth_remembered(void);
const BluetoothInput* bluetooth_input(void);
void bluetooth_set_mode(BluetoothMode mode);

#ifdef __cplusplus
}
#endif
