# Bring-up and verification

## Evidence rules

Owner-provided serial heartbeat/help evidence from the earlier interactive
baseline is recorded below. Updated Bluetooth firmware has not been tested on
hardware. A successful firmware build is not evidence of safe wiring,
controller compatibility, or working motion.

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

## Inhibited-only baseline

This is a USB-powered diagnostic test, not stage 6 powered-motor acceptance.
The owner reports P1 driver wiring and shared ground, with VM, motors, encoders,
and batteries disconnected. Keep all actuator supplies disconnected.

1. Build `whiskey_wagon` in Debug and Release.
2. Flash the desired UF2 via BOOTSEL. Power only the Pico from USB. USB stdio is
   disabled; the Pico will not expose a runtime USB serial console.
   For development logs, connect GP0 TX to the RX input of a 3.3 V USB-to-UART
   adapter and connect shared ground. Leave the adapter TX and supply pins
   disconnected. Never connect RS-232 or 5 V signals.
   Open the adapter's serial port at 115200 baud, 8N1, no flow control.
3. Expect one-second `status` lines with increasing `uptime_ms`,
   `mode=INHIBITED_ONLY`, and all five GPIO readbacks zero. Startup output may
   precede opening the port; wait for the next heartbeat. There are no commands,
   menu, or input parser.
4. Disconnect the diagnostic adapter and confirm the Pico continues running
   without a serial listener; reconnect to observe increasing uptime.
5. With VM still disconnected, measure STBY and all four bridge input signals
   relative to ground. Expect low after initialization. Observe reset/BOOTSEL
   behavior separately; firmware cannot guarantee levels before it executes.
   Do not infer H-bridge output state from GPIO readback.

Record actual measurements and serial logs before marking this test passed.
Do not connect batteries or motors based on a successful serial/GPIO test.
STBY compatibility/biasing, power design, and motion policies remain blocked.

## Bluetooth library bring-up

The earlier stack-initialization-only firmware has been superseded by the bench
test below. Automatic pairing is now expected, not a failure of radio inhibition.
All actuator output remains inhibited; radio/controller verification is separate
from powered motion acceptance.

## Bench firmware test sequence

### 1. Disconnected-hardware controller test

Keep VM, motors, encoders, batteries and actuator supplies disconnected. Power
the Pico over USB. Connect only the 3.3 V UART adapter RX to GP0 plus GND.
USB stdio stays disabled. Add a normally-open momentary button from GP15 (header
20) to GND; no external voltage. The internal pull-up supplies the input bias.

1. Build/flash `whiskey_wagon`; capture UART0 at 115200, 8N1, no flow control.
   Record build/dependency identity, Micro firmware version and D-input mode.
2. Expect `BENCH P1; INHIBITED_ONLY`, Bluetooth readiness, then `link=pairing`
   with no remembered controller, or `link=reconnect` with one stored.
   All five driver readbacks must remain zero. Setup not ready after 15 seconds
   halts explicitly; retain the full boot log on failure.
3. Put only the intended Micro into pairing/discovery. First ready gamepad wins
   during bench pairing; this is not exact-model filtering.
   Expect candidate name/address and ready VID/PID/name, then `link=connected`.
4. Release controls, then press and release physical B, X, Y, and each D-pad
   direction separately (hold each at least one second). Save the `input` lines:
   `buttons`, `dpad`, four `axes`, report count, age and max gap.
   Candidate B/X/Y masks are 0x0001/0x0008/0x0004; verify, do not assume them.
   D-pad steering preview currently uses D-pad bits; emulated-stick modes must
   be identified before changing the mapping. Input logs coalesce at 10 Hz;
   preview control processes every report.
5. Once the candidate mapping is confirmed, inspect `preview` demand ramps:
   B forward, X reverse through zero, release decay, Y zero/coast, D-pad smooth
   center return, and neutral left+right. These are normalized requests only,
   NOT measured speed, motor PWM or physical steering output.
6. Test overlaps: last fresh press wins, simultaneous Y > B > X, no fallback to
   old held buttons, and fresh press after Y. Reconnect while holding drive:
   preview must stay unqualified until B/X/Y release and a fresh drive press.
7. Hold B unchanged and observe report count/age/gaps, then release. Determine
   whether neutral/held reports repeat before choosing any stale-data timeout.
   Turn off the controller; expect preview release ramps, not a delayed 30-second
   motor-stop allowance. The 30-second timer governs pairing fallback only.
8. Power cycle to verify remembered-controller reconnect without pairing.
   Exercise the pairing/sleep/wake matrix below. Wake reboot resets uptime.
9. Measure STBY and all four motor input voltages low, including startup/reset
   observations. Logs alone do not establish electrical driver behavior.

No values in this test qualify powered motion or demonstrate an encoder fault
response. The driver remains hard-inhibited and no closed-loop outputs are enabled.

### 2. Raw encoder characterization, only after electrical checks

Before connecting encoder outputs, determine supply requirements/output voltage
and output type using component documentation or suitable bench measurements.
Verify a 3.3 V-safe Pico interface and any required external pull-ups. Do not use
firmware to discover an unknown output's voltage by connecting it to a GPIO.

Only then configure a separate build with
`-DWHISKEY_ENCODER_CAPTURE_VERIFIED=ON`; internal pulls are disabled.
Motor output remains inhibited. The default UF2 is NOT this variant.

Hand-turn each unloaded wheel/gearbox shaft through known revolutions in both
directions and record per-motor A/B edges, AB sequence, tentative x4 position,
invalid transitions and last-edge timestamp. Confirm electrical phase before
calling the tentative position quadrature or inferring measured direction.
Repeat enough rotations to characterize repeatability/backlash; use a scope or
logic analyzer for phase/noise/edge-rate evidence. IRQ counts alone do not prove
that high-rate edges cannot be missed.

With motor-shaft encoders and 171:1 reduction, if N x4 counts occur per gearbox
output revolution then counts per motor revolution = N / 171, using the same
counting convention. Verify wheel/output coupling separately. No RPM conversion
or stopped/moving claim is made until resolution and phase are calibrated.
Powered motor speed-loop/servo tuning requires a later firmware stage and the
remaining power, interface, output-limit and fault-detection gates.

## Fault and recovery matrix

Expected behaviors below are proposed requirements pending approval of the
[open policies](requirements.md). They are not implemented today.

| Scenario | Required observation to define and verify |
| --- | --- |
| Boot with controller absent | Propulsion remains inhibited, including during GPIO initialization |
| Controller connects with a drive button already held | No motion until B/X/Y are released and a fresh B/X press occurs; no arm/disarm mechanism |
| Controller disconnects under demand | Treat as all buttons released: normal drive deceleration and smooth steering centering; measure detection and physical stopping time |
| Link stays connected but valid reports cease | Freshness timeout inhibits propulsion; approved bound is TBD |
| Controller reconnects with old/held drive input | Require B/X/Y released then fresh B/X; no reuse of old demand and no arm/disarm mechanism |
| Unexpected second controller | Approved ownership policy enforced, no command takeover |
| Invalid/out-of-range input | Rejected and diagnosed without uncontrolled outputs |
| STBY control and reset behavior | After board identification, verify polarity, logic levels, biasing, and motor inhibition; GP14 remains conditional until verified |
| Driver internal fault | nFAULT is not exposed; direct firmware observation is unavailable. Document this limitation and validate any separately approved detection method without destructive fault injection |
| Encoder failure | Immediately coast both motors, require reboot; verify detection thresholds before enabling regulator output; do not confuse legitimate slow/no motion with failed feedback |
| Reset, brownout, or watchdog restart | Hardware and initialization keep propulsion inhibited |
| Steering demand beyond calibrated range | Output remains within approved mechanical limits |
| Forward/reverse transition | Approved ramp/reversal policy followed without exceeding validated limits |
| B/X/Y overlapping presses and releases | Last fresh press wins; same-report ties Y > B > X; Y coasts without ramp or zero-speed correction; old held buttons cannot restart or become fallback demand; fresh B/X restarts with acceleration ramp |
| Normal drive release | Requested full-to-zero ramp meets provisional 0.7 s target; confirm loaded physical deceleration separately |
| Acceleration | Requested zero-to-full ramp meets provisional 1.0 s target without exceeding calibrated output limits |
| Steering release and Y stop | Steering uses provisional 0.5 s center/endpoint travel without jumps; Y stops drive only and does not interrupt steering smoothing |
| D-pad opposing left/right | Neutral steering with normal smooth return to center |
| Closed-loop wheel speed | Both wheels track independent targets within approved error/latency; record measured rather than commanded motion, including coasting at zero demand |
| Differential turning | In forward and reverse, correct inside wheel slows, outside does not exceed requested speed; return to equal speeds smoothly at center; no pivot turns |
| Feedback missing or implausible | Do not infer stopped solely from absent edges or allow unchecked output escalation; approve detection thresholds and response before powered testing |

## Pairing lifecycle acceptance (implemented, hardware tests pending)

Test without actuator power using the approved
[pairing policy](requirements.md#confirmed-pairing-behavior-2026-10-25):

- No stored controller: pairing opens automatically.
- Stored controller: power cycle and reconnect without repeating pairing.
- Stored controller absent at boot or disconnected later: no fallback pairing
  at or before 30000 ms of continuous disconnection; pairing opens on the first
  lifecycle update after 30000 ms. Successful connection resets the interval.
- Physical pairing request while connected: a 2-second hold disconnects and opens pairing.
  Verify old credentials remain usable if no replacement successfully pairs.
- Previous controller may reconnect during the one-minute pairing window;
  successful connection ends the window.
- Without connection, a 60000 ms pairing window expires into software sleep:
  motors coast, Bluetooth stops, servo pulses stop, Pico stays powered.
  Test immediately before/at/after expiry and verify no automatic 30-second
  fallback reopens pairing during sleep.
- Short physical-button press wakes from sleep into the normal startup lifecycle;
  pairing data survives, held drive controls do not restart motion.
- Successful replacement: remember the new controller across reboot.
- On every connection, held B/X cannot drive until B/X/Y are released and a new
  B/X press occurs. Verify there is no separate arm/disarm action.

Pairing fallback does not define drive-stop latency. Independently test the
approved controller-loss release behavior before enabling any powered motion.

## Thresholds required before integrated motion

Record numerical values and their justification in the owning documents:

- Input freshness timeout and measured worst-case stop latency.
- Control update interval and watchdog interval, if used.
- Steering center/endpoints and servo timing.
- Motor duty/speed limits, acceleration and reversal limits.
- Encoder maximum expected edge rate and acceptable decoding error.
- Speed-estimation resolution/window, zero-speed qualification, tracking error,
  feedback-loss/stall detection and regulator output limits.
- Steering geometry and inside/outside wheel target relationship.
- Supply voltage/current limits and acceptable voltage drop.

An unset threshold is a blocked acceptance criterion, not permission to choose a
convenient value in firmware.

## Results register

| Date | Test | Configuration / evidence | Result |
| --- | --- | --- | --- |
| 2026-10-24 | Existing UART firmware Debug and fresh Release builds | SDK 2.3.1, Arm toolchain 15_2_Rel1; performed during repository setup, prior to Bluetooth integration | Passed build only; no hardware validation |
| 2026-10-25 | Inhibited baseline Debug/Release builds and compile-time parser tests | Dirty worktree; SDK 2.3.1, Arm toolchain 15_2_Rel1, Pico W; `build-debug` (Debug), `build` (Release); `whiskey_wagon` and `console_tests` targets | Passed builds/assertions only; USB, UART, GPIO levels and reset behavior not tested |
| 2026-10-25 | Inhibited-only serial heartbeat | Owner-provided COM7 log: 10 consecutive status lines, uptime 110571-119699 ms, intervals 1014-1015 ms; all five GPIO readbacks zero, `mode=INHIBITED_ONLY`; exact flashed build/revision not recorded | Passed observed heartbeat/readback portion only; commands, separate UART test, physical voltage measurements, and reset behavior not verified |
| 2026-10-25 | Earlier interactive baseline help command | Owner-provided `help` response matches expected command listing and inhibited-only notice | Passed help response on earlier firmware; command interface subsequently removed by owner direction |
| 2026-10-25 | Output-only UART baseline Debug/Release builds | Dirty worktree; SDK 2.3.1, Arm toolchain 15_2_Rel1, Pico W; `whiskey_wagon` in `build-debug` and `build`; Release ELF contains `stdout_uart_init`, no USB stdio initialization or command parser symbols | Passed builds and static symbol checks; updated firmware not yet tested on hardware |
| 2026-10-25 | Bluetooth dependency and inhibited runtime Debug/Release builds | Dirty worktree; Pico W; SDK 2.3.1 and Bluepad32 5.0.0-beta0 commits in dependency register; both build checkouts match pinned commit; compile commands use only SDK BTstack; ELF contains `uni_init`, Bluetooth adapter, run loop and heartbeat, with no USB stdio init or BTstack stdin setup symbols | Passed builds with no compiler warnings in final build logs and static integration checks; radio readiness, pairing inhibition and updated UART behavior not tested on hardware |
| 2026-10-25 | Inhibited bench firmware build verification | Dirty worktree based on `cce5cce8a6e5b256f8504d243ed60a9518e7bb52`; dependency pins in [register](dependencies.md#reproducibility-register); Release/Debug with capture OFF, separate Debug capture-ON compile check | All three builds passed without compiler warnings; six constexpr suites passed for lifecycle, button debounce/hold, drive precedence/ramps, reconnect/reset, independent steering/Y coast, and encoder transitions; no flash or hardware test |
| 2026-10-25 | Default bench artifact identity/static gates | `build/whiskey_wagon.uf2`, 1,225,728 bytes, SHA-256 `37CCA9FA7DBA1BD313E813E54A3878FF1F8DE35037DE738D30AD6EE2C97B95C1`; ELF text 616,916 bytes, BSS 38,872 bytes | UART/controller/timer symbols present; no USB stdio, input menu, PWM init or default encoder edge ISR symbols; ISR present only in capture-ON Debug build; static checks do not prove electrical behavior |
| TBD | Bluetooth library readiness | Current bench sequence above; all actuator power disconnected, automatic pairing/reconnect expected | Not run |
| TBD | Output-only UART baseline and electrical checks | Procedure above; owner-reported P1 wiring, VM disconnected; USB stdio and commands removed | Pending UART TX logs, operation without a listener, physical GPIO measurements, and reset observations |
| TBD | Bluetooth/controller compatibility | Dependencies pinned, 8BitDo Micro D-input selected; actual pairing, mapping, report timing and persistence evidence pending | Not run |
| TBD | Hardware and motion verification | P1 driver wiring owner-reported; electrical interface and powered operation not verified | Not run |
