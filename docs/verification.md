# Bring-up and verification

## Evidence rules

No connected-hardware tests have been recorded. A successful firmware build is
not evidence of safe wiring, controller compatibility, or working motion.

For each executed test, record date, firmware commit (or explicit dirty-tree
identifier), board revision, dependency commits, wiring revision, procedure,
expected result, measured result, pass/fail, and relevant logs/measurements.
Do not mark an unexecuted procedure as passed.

## Staged bring-up

| Stage | Preconditions | Checks / acceptance evidence |
| --- | --- | --- |
| 1. Baseline build | Installed SDK/toolchain | Debug and Release builds produce firmware; record exact versions |
| 2. Hardware identification | Actual components available; unpowered | Resolve part numbers, encoder interface, power budget, breakout schematic |
| 3. Wiring review | Pin register completed; approved power design | Confirm GPIO/header numbers, wire colors and LEFT/RIGHT labels against the hardware register, signal levels, PWM slices, grounds, reset biasing, disconnect access; black must be ground only and white motor leads must not be grounded |
| 4. Bluetooth only | Actuator power disconnected | Build relevant BTstack example, then Bluepad32 example; pair selected controller, inspect neutral/full-scale reports, disconnect/reconnect |
| 5. Servo only | Approved servo supply and calibrated test procedure | Test unloaded servo first, then linkage; verify center, endpoints, direction, no binding; record pulse range |
| 6. Motor outputs | Wheels clear of ground, current-limited supply, verified disconnect; arming/stop policies implemented | Test one motor at a time at limited demand; verify polarity, A/B identity, PWM, stop truth table, driver fault behavior |
| 7. Encoders | Verified Hall switch output type, supply, pull-ups/level interface, and mechanics | Both sensor channels per motor toggle; verify phase before quadrature decoding, known rotations in both directions, count convention, expected count/rate, noise and missed-edge checks |
| 8. Integrated faults | Safe motion setup; approved limits and measurable thresholds | Execute the fault matrix below; record time to propulsion inhibition |
| 9. Low-speed wagon trial | All prior gates pass | Confirm mapping, steering limits, controlled speed, encoder readings, stable power, accessible disconnect |

Do not intentionally stall a motor, short a driver, or inject overvoltage to
exercise faults. Use a safe simulation or controlled test method appropriate to
the actual hardware and its ratings.

## Fault and recovery matrix

Expected behaviors below are proposed requirements pending approval of the
[open policies](requirements.md). They are not implemented today.

| Scenario | Required observation to define and verify |
| --- | --- |
| Boot with controller absent | Propulsion remains inhibited, including during GPIO initialization |
| Controller connects with non-neutral throttle | No unintended arming or startup motion |
| Controller disconnects under demand | Approved stop policy occurs within the documented time bound |
| Link stays connected but valid reports cease | Freshness timeout inhibits propulsion; approved bound is TBD |
| Controller reconnects with old/non-neutral demand | Deliberate re-arm required; no automatic restart |
| Unexpected second controller | Approved ownership policy enforced, no command takeover |
| Invalid/out-of-range input | Rejected and diagnosed without uncontrolled outputs |
| STBY control and reset behavior | After board identification, verify polarity, logic levels, biasing, and motor inhibition; GP14 remains conditional until verified |
| Driver internal fault | nFAULT is not exposed; direct firmware observation is unavailable. Document this limitation and validate any separately approved detection method without destructive fault injection |
| Encoder disconnect/noise | Diagnosed according to selected encoder/control strategy; response TBD |
| Reset, brownout, or watchdog restart | Hardware and initialization keep propulsion inhibited |
| Steering demand beyond calibrated range | Output remains within approved mechanical limits |
| Forward/reverse transition | Approved ramp/reversal policy followed without exceeding validated limits |

## Thresholds required before integrated motion

Record numerical values and their justification in the owning documents:

- Input freshness timeout and measured worst-case stop latency.
- Control update interval and watchdog interval, if used.
- Steering center/endpoints and servo timing.
- Motor duty/speed limits, acceleration and reversal limits.
- Encoder maximum expected edge rate and acceptable decoding error.
- Supply voltage/current limits and acceptable voltage drop.

An unset threshold is a blocked acceptance criterion, not permission to choose a
convenient value in firmware.

## Results register

| Date | Test | Configuration / evidence | Result |
| --- | --- | --- | --- |
| 2026-10-24 | Existing UART firmware Debug and fresh Release builds | SDK 2.3.1, Arm toolchain 15_2_Rel1; performed during repository setup, prior to Bluetooth integration | Passed build only; no hardware validation |
| TBD | Bluetooth/controller compatibility | Pending dependency pins and controller selection | Not run |
| TBD | Hardware and motion verification | Wiring plan P1 documented; physical wiring and electrical interface not verified | Not run |
