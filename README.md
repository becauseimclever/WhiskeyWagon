# WhiskeyWagon

C++17 firmware for a Bluetooth-controlled RC wagon, built with the Raspberry Pi
Pico SDK and currently targeting the Pico W. Planned hardware includes an SG92R
steering servo, two DRV8833-driven motors, and Hall-effect motor encoders.

## First wagon POC (2026-10-31)

The separate `whiskey_wagon_poc` image now implements Bluetooth drive commands,
per-wheel encoder speed regulation with a steering differential for both 6 V
GA25-370 motors, and SG92R steering on GP2. The motor profile records a
**164:1 measured reduction (labeled 171:1) and 35 RPM rated output-shaft speed**;
full demand targets 35 wheel rpm.

Both motors passed the latest powered pulse test after harness repair; left Hall
capture and physical-forward polarity remain unverified. STBY was externally
enabled with GP14 disconnected in those tests; **do not connect GP14 to an
externally driven STBY**. Controlled STBY must be verified before powered POC use.

Build the POC with the normal build configuration:

```sh
cmake --build build --target whiskey_wagon_poc
```

The resulting `build/whiskey_wagon_poc.uf2` is **actuator-inhibited by default**,
ready for tomorrow's controller mapping/cadence tests. The original
`whiskey_wagon` and manual motor-test images remain separate.
See [POC behavior and configuration](docs/firmware.md#open-loop-wagon-poc-2026-10-31)
and [the first integrated bench procedure](docs/verification.md#first-integrated-poc-bench-test).
Closed-loop speed regulation and geometry-based differential turning remain
required later; this POC deliberately does not claim either.

## Project documentation

The [project source of truth](docs/README.md) indexes requirements, the
[authoritative pin register](docs/hardware.md), firmware design, Bluetooth
dependencies, and verification procedures.

The original bench firmware provides inhibited-only driver GPIO initialization,
output-only UART0 diagnostics, Bluetooth pairing/reconnect, and simulated drive
and steering ramps.
The owner reports the Pico W and DRV8833 wired per plan P1,
with shared ground; initial bring-up had VM, motors, encoders, and batteries
disconnected. Later powered test results are recorded above and in verification.
Electrical acceptance is still pending. Motor output in the original image is
compile-time inhibited; no servo pulses are emitted. Raw encoder capture is
implemented but disabled until its electrical interface is verified. Closed-loop
motor output, RPM conversion and calibrated differential mixing are not enabled.

An optional, separate **right-motor USB bench image** is now available. It uses
manual one-shot pulses, not Bluetooth or closed-loop control. See
[the reusable powered test](docs/verification.md#right-motor-usb-bench-test)
for electrical gates, build commands, console commands and acceptance checks.
The default `whiskey_wagon` image remains inhibited-only with USB stdio disabled.

An optional **servo-only USB bench image** is also available for supervised
bring-up. It starts disarmed and emits no pulses until an explicit command;
the owner reports the SG92R connected; unloaded it sweeps about 180 degrees
over the tested 544-2400 us range, with a working center at the 1472 us
midpoint (mechanical end stops not located). Steering travel limits
must still be set once the linkage is installed.
See the [servo test procedure](docs/verification.md#servo-only-usb-bench-test)
before powering or testing the servo.

## Repository layout

```text
WhiskeyWagon/
    .vscode/                 Shared Pico extension build/debug configuration
    cmake/
        pico_sdk_import.cmake  SDK-provided import helper
        bluetooth.cmake      Pinned Bluepad32 acquisition and SDK integration
    docs/                    Requirements, hardware register, design and verification
    src/
        main.cpp             Firmware entry point
        board_config.hpp     Driver GPIO assignments from plan P1
        bench_logic.hpp      Testable pairing/button/ramp/edge-count logic
        bluetooth_adapter.c  Bluepad32 platform, reports and remembered controller
        bluetooth_adapter.h  C/C++ adapter interface
        encoder_capture.cpp  Gated raw encoder IRQ capture and snapshots
        encoder_capture.hpp  Raw capture interface
        right_motor_test.cpp Standalone opt-in USB right-motor bench test
        right_motor_test_logic.hpp One-shot pulse limits and command matching
        servo_test.cpp       Standalone opt-in USB servo bench test
        servo_test_logic.hpp Servo arming, pulse limits and command matching
        btstack_config.h     SDK BTstack feature/buffer configuration
        sdkconfig.h          Bluepad32 platform configuration
    CMakeLists.txt           Project and firmware target
    tests/                   Compile-time deterministic bench logic tests
    build/                   Generated output (ignored by Git)
```

Keep implementation files and private headers in `src/`. Add
`include/whiskey_wagon/` when there are public library headers, and `tests/` when
there is testable application logic; empty placeholder directories are not needed.
Keep SDK code and toolchains outside the repository. Locally retained motor
bench logs are in `bench-logs/motor/`, which is ignored by Git.

## Prerequisites

- CMake 3.18 or newer, Git, Ninja, Python 3, and an Arm GNU bare-metal toolchain
  (`arm-none-eabi-gcc` and `arm-none-eabi-g++`).
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk), including its
  required submodules (`git submodule update --init --recursive` from the SDK directory).
- Alternatively, install the recommended Raspberry Pi Pico VS Code extension
  and let it install the SDK and tools.

The Pico extension integration selects SDK 2.3.1, toolchain 15_2_Rel1, and
picotool 2.3.1. Without an explicit `PICO_SDK_PATH`, CMake uses that installation
when available. Set `PICO_SDK_PATH` in the environment or CMake cache to use
another SDK installation, and ensure its toolchain is available on `PATH` or set
`PICO_TOOLCHAIN_PATH`.

The first configure fetches Bluepad32 **5.0.0-beta0** at the immutable commit in
`cmake/bluetooth.cmake`; network access is required once per build directory.
Its optional submodules are not fetched. BTstack comes only from the Pico SDK,
with both Classic and BLE enabled. See the [dependency register](docs/dependencies.md)
for exact tested revisions and the beta compatibility decision.

## Build

From a terminal with the prerequisites on `PATH` (the Pico extension's terminal
provides these), run:

```sh
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target whiskey_wagon
```

For an external SDK installation, add `-DPICO_SDK_PATH="<SDK directory>"` to the
configure command. Use `-DCMAKE_BUILD_TYPE=Release` for an optimized build.
Use a separate build directory when switching boards or toolchains.

In VS Code, configure the project with the Pico extension, select the
`whiskey_wagon` target, and run **Compile Project** (the default build task).
Debug and flash configurations resolve the selected target through the extension.

The build produces `whiskey_wagon.elf`, `whiskey_wagon.uf2`, and other firmware
outputs in `build/`, along with `compile_commands.json` for editor tooling.
To flash without a debug probe, hold BOOTSEL while connecting the board over
USB, then copy `whiskey_wagon.uf2` to its mass-storage drive.

## Current behavior and first test

- Before initializing diagnostics, firmware drives STBY (GP14) low, then all four
  bridge inputs (GP6-GP9) low. These outputs never intentionally go high.
- UART0 outputs development logs at 115200 baud, 8N1 on GP0 (3.3 V logic).
  GP1 remains reserved for UART0 RX but is not initialized. There is no input
  parser or menu. UART1/GP4/GP5 are not initialized.
- USB stdio is disabled; USB is used for BOOTSEL firmware upgrades, not a runtime
  console. Normal operation does not require a PC or diagnostic adapter.
- CYW43, SDK BTstack, and Bluepad32 initialize, then BTstack's run loop services
  Bluetooth and the heartbeat timer. No Wi-Fi connection is started.
- With no remembered controller, pairing opens automatically for 60 seconds.
  Otherwise only the remembered address is admitted during reconnect; after
  more than 30 seconds disconnected, a 60-second pairing window opens.
  The previous controller can reconnect during pairing. First ready gamepad
  wins: test with only the intended controller in X-input mode discoverable.
- Hold a dedicated normally-open button between GP15 (header pin 20) and GND
  for 2 seconds to request pairing. Input uses a pull-up and 30 ms debounce.
  Explicit pairing disconnects the current controller. Repeated requests do not
  extend an already-open window. The BLE management service stays disabled.
- Successful HID readiness persists the preferred controller address (versioned
  SDK TLV tag), keeping prior identity until replacement is ready. SDK Classic/BLE
  bonding storage owns keys; keys are never blanket-erased on boot or pairing.
- If pairing expires, enter software sleep: motor signals remain low, servo
  remains off, and Bluetooth powers off. A short button press/release wakes by
  rebooting into the normal lifecycle. Pairing storage survives; uptime resets.
  The Pico remains powered and UART/button timers run; this is not a low-power
  specification or physical power-off.
- Controller reports feed preview-only smoothstep ramps. Connection requires
  released candidate B/X/Y before a fresh drive press; no arm/disarm mechanism.
  Y coasts immediately in the preview, loss releases controls, same-report ties
  resolve Y > B > X, and opposing steering directions center.
- A startup banner and one-second status heartbeat report `mode=INHIBITED_ONLY`,
  uptime, GPIO input readback, and unimplemented features. There is no arm command.
- Bluetooth status starts as `initializing` and becomes `ready` only from the
  Bluepad32 initialization-complete callback. Synchronous initialization errors
  log and halt; HCI failures are explicit. Setup not completing within 15 seconds
  halts with an error, not a readiness success.
- `input` logs report normalized button masks, D-pad, axes, total reports and
  inter-report gaps. Changes are logged at most 10 times/second (coalesced), but
  every report is processed by control. Heartbeats include report age/gap data
  to determine whether held controls repeat reports. No stale timeout is guessed.
- Candidate physical B/X/Y masks are `0x0001` / `0x0008` / `0x0004` in
  `board_config.hpp`, based on Bluepad32's positional normalization. These are
  **unverified for this Micro**. Confirm each physical label before relying on
  the preview; D-pad reports, not emulated stick axes, currently steer it.
- Preview demand is normalized (`permille`), not measured RPM or motor PWM.
  Differential geometry is uncalibrated; no wheel-speed target is invented.
- Logs are emitted without waiting for a listener. If the startup banner is
  missed, wait for the next heartbeat.

Keep VM, motors, encoders, batteries, and actuator supplies disconnected for this
test; power the Pico from USB only. Flash the UF2, then monitor GP0 using a
3.3 V USB-to-UART adapter (adapter RX and shared ground only), and
follow the [baseline checks](docs/verification.md#inhibited-only-baseline).
Follow the [bench test procedure](docs/verification.md#bench-firmware-test-sequence).
GPIO readback is not proof of bridge output state or electrical safety.
Reset/BOOTSEL behavior and STBY compatibility still require measurement.

### Encoder measurements (after interface verification only)

Default builds leave GP10-GP13 untouched and print
`encoder_capture=DISABLED_UNVERIFIED_INTERFACE rpm=UNAVAILABLE`.
Do not connect unknown-voltage encoder outputs to the Pico.

After verifying a 3.3 V-safe interface and appropriate external biasing, configure
a separate build with `-DWHISKEY_ENCODER_CAPTURE_VERIFIED=ON`.
That variant samples both edges with internal pulls disabled and logs separate
A/B edge counts, tentative quadrature x4 position, invalid two-bit transitions,
AB state and last-edge timestamp. Quadrature phase and direction remain
unverified; these are measurements for calibration, not certified position.
Motor output remains inhibited even in this variant.

Hand-turn measured gearbox-output revolutions to establish counts using the
164:1 measured motor-shaft reduction (labeled 171:1). Until the counting convention/resolution are
verified, no RPM or stopped/moving assertion is produced. Powered speed-loop
tuning, stall/feedback-fault detection and calibrated servo output are a later
stage, not accessible through a runtime command or a motor-enable build switch.

## Conventions

- Use `snake_case` for source/header filenames, CMake targets, namespaces,
  functions, and variables; use `PascalCase` for types and `kPascalCase` for constants.
- Use `.cpp` for C++ sources and `.hpp` for project C++ headers.
- The Bluepad32 platform adapter uses `.c`/`.h` for its C callback API; application
  logic remains C++17.
- Use four-space indentation and format C++ with the root `.clang-format`.
- Keep includes and dependencies target-scoped in CMake and list sources explicitly.
- Keep generated files, local build presets, SDKs, and toolchains out of Git.

## Validation

Build the firmware:

```sh
cmake --build build --target whiskey_wagon
```

Repeat with a separate Debug/Release build directory when validating both modes.
`bench_logic_tests` runs C++17 compile-time assertions and is a dependency of the
firmware build. It checks ramp timing/reversal/coast behavior, neutral qualification,
button precedence, pairing deadlines, sleep, debounce/hold behavior, encoder
transition counting, and right-motor arming/pulse/duty/cooldown limits.
Hardware/Bluetooth integration still needs on-board tests.
UART output, GPIO levels, reset behavior, and flashing require a connected
board; a successful build does not verify them.

## License

See [LICENSE](LICENSE). The SDK import helper retains its upstream license notice.
