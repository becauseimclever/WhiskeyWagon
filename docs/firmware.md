# Firmware design

## Current implementation

The target is `whiskey_wagon`, C++17, with `pico_stdlib`, UART stdio,
Bluepad32, and SDK BTstack Classic/BLE.
The baseline is deliberately **inhibited-only**, not a motion controller:

- `board_config.hpp` owns the P1 driver GPIO assignments.
- Before stdio initialization, STBY is initialized low, then all four motor
  inputs are initialized low. Each output latch is cleared before enabling output.
- UART0 TX (GP0, 115200 baud, 8N1) carries a startup banner and one-second status
  heartbeat for development. `stdout_uart_init()` initializes output only; GP1
  stays reserved for RX but is not configured. UART settings are target
  definitions in CMake. UART1 is not used.
- There is no command parser, input polling, menu, or serial stop/arm interface.
  Status reports GPIO pad readback, not driver-output state or direct fault data.
- USB stdio is disabled. USB BOOTSEL firmware updates remain available.
  Normal operation does not require a PC or diagnostic listener.
- `bluetooth_adapter.c` implements the Bluepad32 C platform callbacks, exposed
  through `bluetooth_adapter.h` with C linkage for the C++ entry point.
  It initializes CYW43 and installs the custom platform before `uni_init`.
  After readiness, the lifecycle enables pairing or remembered-address reconnect,
  with the BLE management service disabled. Pairing admits the first ready
  gamepad, reconnect admits only the remembered address using the SDK allowlist.
- A versioned application TLV record (`0x57574331`, version + six-byte address)
  records the preferred controller at successful HID readiness. Writes are checked
  and read back; unavailable storage, corrupt records or persistence failures
  cause explicit errors. SDK bonding storage retains keys; no blanket key
  deletion is performed.
- Every accepted report goes to `BenchControl`. Normalized masks/axes and
  report counts/gaps are diagnosed with coalesced change logs. Physical Micro
  mappings remain explicitly unverified candidate masks in board configuration.
  No stale-report timeout or RPM measurement is invented.
- `bench_logic.hpp` implements lifecycle/button/control logic: 30 ms debounce,
  one hold event after 2 seconds, reconnect fallback strictly after 30 seconds,
  60-second pairing window, smoothstep demand ramps, last-new-press precedence
  and release qualification. Demand is continuous, but interrupted ramps are
  not a claim of jerk-limited physical motion.
- BTstack owns the run loop. A 10 ms timer updates controls/button/lifecycle and
  emits a one-second heartbeat; there is no blocking sleep loop.
  `bluetooth=ready` is set only by Bluepad32's
  initialization-complete callback, not merely by successful CYW43 initialization.
  Synchronous initialization failure logs and halts. HCI power-on failure/off
  events log errors and halt on the control tick. Setup not ready after 15 seconds
  halts with a boot-log diagnostic; no automatic recovery is attempted.
- Pairing expiry requests HCI power-off; `sleep_requested` becomes `sleeping`
  on the HCI-off event. Button/run-loop/diagnostic timers remain active.
  A short press/release wakes by watchdog reboot, preserving flash pairing data.
  This avoids reinitializing Bluepad32's one-shot setup in place. It is software
  sleep, not physical power removal or certified low-current operation.
- Optional `encoder_capture.cpp` captures GP10-GP13 raw edges, tentative x4
  transitions and invalid transitions. Its interface-verification CMake flag is
  OFF by default. Enabled capture uses no internal pulls and IRQ-masked snapshots
  on the same core. No counts/revolution, direction or RPM is assumed.
- Diagnostic writes can still block. Log volume and worst-case callback time
  must be bounded before implementing motion control.

The build never initializes motor PWM or servo output; all five motor GPIOs stay
low. A compile-time assertion rejects an attempt to enable motor output.
Closed-loop regulator output, encoder-fault detection, physical stop verification,
calibrated differential mixing and servo calibration remain unimplemented.
Reset-time inhibition still depends on unverified hardware biasing.
No arming, running watchdog, controller-freshness timeout or powered stop policy
is claimed by this build. The watchdog API is used only for button wake reboot.
Hardware acceptance remains separate in [verification](verification.md).

## Proposed architecture

Keep the first implementation small, with target-scoped dependencies and explicit
ownership. The following are responsibilities, not a requirement to create a
class, thread, library, or empty directory for each one.

| Responsibility | Owns | Must not do |
| --- | --- | --- |
| Board configuration | Approved pin map, PWM ownership, calibration constants | Invent defaults for unassigned hardware |
| Bluetooth adapter | Bluepad32 platform callbacks, controller selection, report validation and timestamps | Drive motors directly from arbitrary callbacks |
| Control/safety coordinator | Input qualification, freshness checks, command limits, stop policy | Reuse the last command indefinitely |
| Steering output | Servo timing, inversion, calibrated endpoints | Exceed verified mechanical limits |
| Motor output | DRV8833 input truth table, duty, polarity, reversal policy | Assume coast and brake are interchangeable |
| Encoder acquisition | Counts, timestamps, decode errors, direction when available | Treat commanded direction as measured direction |
| Diagnostics | Connection, safety state, faults, counts, configuration identity | Block Bluetooth/control servicing with excessive logging |

Put implementation in `src/`, adding focused `.cpp`/`.hpp` modules as functionality
is introduced. Add public headers only when a real shared interface exists.
Keep pin and calibration constants in a single board configuration surface once
assignments are approved, synchronized with the [hardware register](hardware.md).

## Control flow proposal

```text
Controller -> Bluepad32 / BTstack -> validated, timestamped input
                                          |
                                   control/safety
                                     /         \
                              steering       motor demands
                                                |
                                           DRV8833
                                                |
                                             motors
                                                |
                                           encoder data
```

Independent encoder-based closed-loop wheel-speed control is required, alongside
servo steering. Differential targets reduce the inside wheel's speed and retain
the requested outside-wheel speed, without pivot turns. Base the differential
on the smoothed/calibrated steering state to avoid stepping wheel targets ahead
of steering. The actual geometry-to-speed relationship and regulator algorithm
remain to be selected and tested.
Each motor has two Hall switch channels. Verify their electrical interface and
phase relationship before choosing a quadrature decoder or deriving direction.
Measured motion must remain separate from commanded speed, including at zero
demand and during reversals. Feedback resolution/latency and encoder failure
handling must be explicit: missing counts cannot unconditionally mean stopped,
and the regulator must not ramp toward full output indefinitely on missing feedback.

For software, assume ideal regulated 6 V motor and 4.8 V Pico/servo rails,
battery-only normal operation and external charging. Do not invent voltage/current
measurements or claim software undervoltage protection without sensing hardware.
Physical rail/protection validation remains independent of the ideal software model.

### Input contract

The approved [operator behavior](requirements.md#confirmed-operator-behavior-2026-10-25)
uses physical Micro B/X/Y for forward/reverse/immediate drive-stop and D-pad
left/right for smooth steering with automatic smooth centering. Last new drive
button press wins; held controls cannot override Y without a fresh press.
There is no operator arm/disarm action. The provisional ramp targets are owned
by that requirements section; demand ramps are implemented for preview only,
not physically validated motion.

Use D-input mode. Same-report fresh-press ties resolve Y first, then B, then X;
opposing D-pad left/right means smooth centering. Y immediately selects coast,
bypassing both release ramp and zero-speed regulation so the closed-loop controller
does not fight coasting. Controller loss instead synthesizes all-button release:
normal drive deceleration and smooth steering centering. Encoder failure stops
both wheels by immediate coast, with reboot required after a detected fault.
Detection thresholds remain to be defined; the inhibited build does not enable
a regulator or diagnose disconnected, unpowered encoders as faulty.

Finalize actual report mapping, interpolation
profile, calibrated output limits, reversal dwell, and fault recovery.
On connection/reconnection, require B/X/Y released then a fresh B/X press;
this qualification is not an arm/disarm mechanism.
Keep requested demand separate from ramped output and measured
wheel motion; a ramp reaching zero is not evidence that a wheel has stopped.
Track freshness with a monotonic time source and valid reports, not just a
connection flag. Decide how controllers that suppress unchanged reports affect
freshness before setting a timeout. Record numerical values with their rationale.

### Pairing lifecycle

Follow the [approved pairing requirements](requirements.md#confirmed-pairing-behavior-2026-10-25):
pair automatically with no remembered controller; otherwise reconnect to the
last successfully paired controller, retaining its identity/credentials across
power cycles. Reopen pairing after more than 30 seconds continuously disconnected.
A wagon-mounted momentary button held for 2 seconds explicitly opens pairing, disconnecting any active
controller without erasing the previous remembered controller until a new one
successfully reaches HID readiness. The physical input is GP15 to GND with pull-up.
Pairing stays open for 60 seconds, accepting the previous controller as well.
A successful connection closes the window; expiry without connection coasts
motors, stops Bluetooth and servo pulses, and enters software sleep with the Pico
still powered. A short button press wakes it into the normal startup/reconnect
flow. Sleep/wake must not erase pairing data or bypass fresh-input qualification.

Connection state and pairing state must not authorize actuator motion by
themselves. Clear old drive input and apply the approved release/fresh-press
qualification on every connection. The pairing fallback timer is independent of
motion-loss handling; never continue driving for 30 seconds after controller loss.
Repeated holds do not extend an open window; first ready gamepad wins, and
successful HID readiness is the preferred-identity persistence point.
This lifecycle is implemented for bench testing, not yet hardware-verified.

### Proposed safety states

| State | Propulsion intent | Exit condition |
| --- | --- | --- |
| Boot / inhibited | Inhibited | Initialization succeeds, controller connects, B/X/Y released then fresh B/X press |
| Control available | Validated demand through normal ramps, or Y immediate stop | Stale data, disconnect, fault |
| Lost control | All-button release ramps: drive to zero, steering to center | Reconnection plus approved release/fresh-press qualification |
| Encoder fault | Immediately coast both motors | Reboot required after fault; detection not enabled in bench build |
| Software sleep | Coast, Bluetooth off, servo pulses off | Short physical button press; restart normal connection lifecycle |

There is no user arm/disarm state. Y must not create a latched disarm state.
This is a proposal, not a firmware guarantee. Steering behavior in each state,
timeout bounds, encoder-fault detection, and STBY control behavior remain TBD.
The breakout exposes STBY, not nSLEEP or nFAULT. GP14 is conditionally reserved
for STBY pending electrical verification; there is no direct driver-fault input.
Faults must be visible in diagnostics; recovery must not silently re-enable motion.

## Scheduling and concurrency

Follow the pinned Pico W Bluepad32 example's stack initialization, callback
context, and BTstack run loop. Avoid transplanting ESP32-specific integration.
Do not retain the blocking demonstration loop as the eventual control scheduler.

- Keep callbacks and encoder interrupt handlers short and bounded.
- Use timers/non-blocking work for periodic control and diagnostics.
- Define synchronization and data ownership before adding IRQ, core, or thread sharing.
- Never read multi-field encoder/control snapshots without a consistent handoff.
- Use one BTstack source tree and one owner of stack initialization/run-loop setup.

The actual scheduler, control period, encoder backend, and watchdog policy must
be selected and verified during implementation.
