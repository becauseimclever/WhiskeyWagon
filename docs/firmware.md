# Firmware design

## Current implementation

The target is `whiskey_wagon`, C++17, with `pico_stdlib` and `hardware_uart`.
The firmware initializes stdio, sends one UART1 greeting, then prints a heartbeat
once per second. USB stdio is disabled. The build does not currently initialize
the wireless chip, BTstack, Bluepad32, PWM, encoders, or motor control.
The [hardware register](hardware.md) now reserves UART0 and allocates driver and
encoder plan P1; those allocations do not change the current UART-only firmware.

## Proposed architecture

Keep the first implementation small, with target-scoped dependencies and explicit
ownership. The following are responsibilities, not a requirement to create a
class, thread, library, or empty directory for each one.

| Responsibility | Owns | Must not do |
| --- | --- | --- |
| Board configuration | Approved pin map, PWM ownership, calibration constants | Invent defaults for unassigned hardware |
| Bluetooth adapter | Bluepad32 platform callbacks, controller selection, report validation and timestamps | Drive motors directly from arbitrary callbacks |
| Control/safety coordinator | Arming, freshness checks, command limits, stop policy | Reuse the last command indefinitely |
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

Encoder acquisition is required; feeding it into closed-loop motor control is a
separate unresolved decision. Steering is servo-based; differential mixing is
not implied by having two motors.
Each motor has two Hall switch channels. Verify their electrical interface and
phase relationship before choosing a quadrature decoder or deriving direction.

### Input contract to finalize

Define selected controller identity, axis ranges, neutral/deadband, inversion,
throttle/reverse semantics, steering mapping, arm/disarm action, and stop action.
Track freshness with a monotonic time source and valid reports, not just a
connection flag. Decide how controllers that suppress unchanged reports affect
freshness before setting a timeout. Record numerical values with their rationale.

### Proposed safety states

| State | Propulsion intent | Exit condition |
| --- | --- | --- |
| Boot / disarmed | Inhibited | Initialization succeeds, approved arming conditions met |
| Armed | Validated demand within approved limits | Disarm, stale data, disconnect, fault |
| Fault / lost control | Stop using the approved coast/brake policy | Cause cleared plus deliberate re-arming |

This is a proposal, not a firmware guarantee. Steering behavior in each state,
timeout bounds, stop semantics, and STBY control behavior remain TBD.
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
