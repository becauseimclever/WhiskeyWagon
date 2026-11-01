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

## Right-motor USB bench test

This is the optional `right_motor_test` image, not the inhibited-only
`whiskey_wagon` image. It enables USB CDC serial only in this test image; it does
not initialize Bluetooth, UART0 or the servo. GP6/GP7 stay low except for the
explicit `both100` simultaneous test. Only the right
Hall pair GP12/GP13 is initialized; left Hall pins GP10/GP11 remain untouched.

Owner confirmation on 2026-10-30: right motor wired and powered; GP8/GP9 motor
inputs, GP14 STBY and GP12/GP13 Hall signals confirmed; interfaces reported
3.3 V; wheel secured/lifted and disconnect accessible. These are owner reports,
not independent electrical measurements or completed motion acceptance.

Before each session, verify shared ground, appropriate motor supply (6 V target),
3.3 V-safe Hall outputs and STBY interface, external Hall biasing if needed
(firmware disables internal pulls), and hardware STBY reset biasing. Keep wheels
clear, use current limiting, and retain access to a physical power disconnect.
Do not operate the wagon on the ground using this diagnostic image.

Build in a separate directory:

```powershell
cmake -S . -B build-right-motor -G Ninja -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Debug -DWHISKEY_BUILD_RIGHT_MOTOR_TEST=ON -DWHISKEY_RIGHT_MOTOR_BENCH_VERIFIED=ON
cmake --build build-right-motor --target right_motor_test
```

The verification flag confirms the bench prerequisites above; it is OFF by
default and configure fails if the test is requested without it. Flash
`build-right-motor/right_motor_test.uf2` using BOOTSEL or picotool. Flash the normal
`whiskey_wagon.uf2` afterwards to remove the USB test console and powered test.
No default production behavior or Bluetooth button mapping is changed.

For the isolated floating-STBY experiment only, configure with
`-DWHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED=ON`. Disconnect the GP14-to-STBY wire
before powering up. This variant omits every GP14 initialization, read and write;
the pin remains at reset defaults, not an actively controlled enable. Status
reports `stby=UNCONTROLLED gp14=UNTOUCHED`. Stop/timeout clears both right PWM
inputs but **cannot lower STBY**. Floating STBY is not a reliable enabled state;
vendor documentation describes it as disabled. Do not infer driver enable from
this experiment. Keep the wheel lifted and physical disconnect ready.
Omit the option or set it OFF to restore the normal test's STBY control.

For an explicitly supervised no-serial experiment, also configure with
`-DWHISKEY_RIGHT_MOTOR_BOOT_PULSE=ON`. **Automatic motion is attempted on boot:**
after a 3-second delay, one forward 50% pulse runs for at most 5 seconds,
then both PWM inputs remain zero until reset. Motor power must be OFF during
flashing; secure/lift the wheel and keep the physical disconnect ready before
powering the test. The mode never initializes USB stdio or waits for UART/USB
commands; no serial stop command is available. The independent stop alarm
and 200 ms watchdog remain active. Watchdog-caused resets skip the automatic
pulse, preventing repeated motion following a watchdog fault. Other resets
or power cycles can trigger it again. With STBY-disconnected mode, GP14 remains
untouched and the floating driver's enable is uncontrolled. This mode does not
overcome a disabled STBY input. Set `WHISKEY_RIGHT_MOTOR_BOOT_PULSE=OFF` and
reflash to restore the interactive USB test. The normal firmware is unchanged.

For the separately approved full-duty starting check, additionally set
`WHISKEY_RIGHT_MOTOR_BOOT_FULL_DUTY=ON` (requires boot-pulse mode). This replaces
the 50%/5 s boot pulse with **100%/1 s**, still after the 3 s delay. With PWM wrap
999, compare level 1000 holds GP8 continuously HIGH and GP9 LOW during the pulse;
both compare levels return to zero afterward. Compile-time tests reject 100%
at other durations and verify expiry exactly at 1000 ms.  Use only the motor's rated 6 V supply and the secured bench;
cut power immediately if it remains stationary and hums or heats. This is not a
startup-current measurement and does not bypass driver enable/current limiting.
Set the full-duty option OFF to return to the 50% boot pulse; set boot-pulse OFF
to restore the interactive test. No-serial firmware requires BOOTSEL flashing.

Open the Pico USB serial port at 115200 baud with **DTR enabled**, no flow
control, and CR or LF line endings. Baud rate is a terminal convention for CDC,
not a physical UART speed. Each connection prints help and starts disarmed.

| Command (followed by Enter) | Result |
| --- | --- |
| `help` | Repeat commands and limits |
| `status` | Arming/direction, GPIO readbacks, raw Hall A/B edges, tentative x4 count, invalid transitions |
| `arm` | Permit one pulse, expiring after 5 seconds |
| `fwd` | GP8 PWM, GP9 low: 25% duty at 20 kHz for at most 500 ms |
| `rev` | GP8 low, GP9 PWM: same limits, opposite electrical polarity |
| `fwd50` / `rev50` | Explicit 50% diagnostic pulse, same 500 ms limit; use only after reviewing a failed 25% test |
| `fwd50long` / `rev50long` | Explicit 50% diagnostic pulse capped at 5 seconds; use only with the wheel secured/lifted and disconnect ready |
| `fwd100` / `rev100` | Explicit 100% diagnostic pulse capped at 1 second; fresh arming required |
| `both100` | Both motors electrical forward at 100% for 1 second; GP6/GP8 HIGH, GP7/GP9 LOW; fresh arming required |
| `left100` | Left only electrical forward at 100% for 2 seconds; GP6 HIGH/GP7 LOW, right pair zero; fresh arming required |
| `mark` | Stop/disarm, clear RPM calibration and mark initial Hall counts |
| `cal10` | Calibrate x4 counts/revolution after exactly 10 hand-turned motor-shaft revolutions in one direction; motor power OFF, USB/Hall power ON |
| `stop` | Clear PWM and revoke arming; lower STBY only in the default, GP14-controlled build |
| `!` or Ctrl-C (no Enter needed) | Stop/disarm; discard the remainder of that input line |

Send commands individually; there is no echo or interactive line editing.
Unknown, non-printable or overlong command lines stop/disarm with an error.
An independent alarm callback clears PWM (and lowers STBY in the default build)
at pulse expiry, even if a
diagnostic write blocks. The loop also checks expiry; a 200 ms running watchdog
reboots on a stalled loop (not paused for debugging). USB DTR loss disconnects
and disarms on the next loop. Neither USB disconnect detection nor a watchdog
reset substitutes for the physical disconnect or verified hardware reset biasing.
The host can remain connected without reading, so the pulse limit, not USB
presence, is the primary motion bound. Every pulse consumes arming and requires
500 ms cooldown before re-arming; no queued direction change/reversal is applied.
Stopping inhibits the bridge, not a guaranteed mechanical brake.

Reusable procedure:

1. With the image running, expect `RIGHT_MOTOR_TEST`, `armed=0`, `direction=0`,
   `stby=0` and `left=0,0`. `fwd` without arming must be rejected.
2. Hand-turn the right wheel if mechanically possible. Both `a_edges` and
   `b_edges` should increase. Check both channels, not only the tentative count.
3. Send `arm`, then `fwd` within 5 seconds. Observe a brief pulse and
   `PULSE_DONE` with the build's `stop_policy`, `delta_a`, `delta_b`,
   `delta_x4`, `delta_invalid`.
   Check actual right-wheel motion and that the left motor does not move.
4. Wait at least 500 ms after stopping. Send `arm`, then `rev`. Verify the
   opposite physical direction; record which polarity is wagon-forward.
5. In both tests, require both Hall edge deltas to be positive and no invalid
   transitions for clean initial evidence. Tentative x4 signs should reverse
   only if these sensors really form a quadrature pair. Nonzero invalid
   transitions or no/missing edges require investigation, not a success claim.
   The pulse summary covers counts through software observation of the stop;
   inertia/coasting edges may also appear in subsequent cumulative status.
6. Verify `stop`, `!`, USB closure, arming expiry, cooldown and re-arm behavior.
   Measure STBY/PWM and actual stop response independently of software logs.
   Do not intentionally stall the motor. If 25% duty cannot start it, stop and
   review supply/load/driver behavior; the test does not automatically raise duty.
   The explicitly named `fwd50` / `rev50` commands are available only for a
   deliberate 50% diagnostic check after that review. For a low-speed gearbox,
   `fwd50long` / `rev50long` explicitly extend the pulse to 5 seconds, not the
   ordinary short commands. The separate `fwd100` / `rev100` commands permit
   100% only for 1 second. No command exceeds 5 seconds. The
   `PULSE_DONE observed_ms` field reports the loop's observation of completion,
   not an oscilloscope measurement of electrical pulse width.
   `pwm_cc` reports slice compare levels (250 or 500 out of 1000), and
   `pwm_counter` samples the running PWM counter; neither proves a waveform
   reaches the breakout or that its output stage is enabled.

Record logs, physical observations and measured pulse timing per the evidence
rules. A build or serial GPIO readback does not establish motion, brake behavior,
motor current, encoder resolution, RPM or full fault acceptance. This test has
no encoder-loss/stall shutdown or closed-loop speed control.

### 2026-10-30 session evidence

Board: owner-reported Pico W, revision unknown; wiring P1 as confirmed above.
Owner measured VM at **5.99 V**. Build: base commit `d9edc1c` plus uncommitted
right-motor-test changes; SDK 2.3.1, Arm GNU 15_2_Rel1, Debug. Both
`right_motor_test` and unchanged `whiskey_wagon` built, including compile-time
assertions. USB CDC was flashed through the BOOTSEL volume because picotool
could request reboot but could not access the Windows BOOTSEL interface.
Console used COM7, 115200, DTR enabled.

Final five-second-capable UF2 SHA-256:
`7D72ACE745B69EFD859488EEFFE7D08A5CE7EBD5248D0A5C3BB8CFF71A69E19B`.
Intermediate short-pulse checks used earlier revisions from the same session.
Generated serial logs are in the ignored `build-right-motor/` directory:
`bench-no-motion.log`, `bench-usb-disarm.log`, `bench-forward.log`,
`bench-reverse.log`, `bench-forward50.log`, `bench-forward50long.log`.

| Executed check | Observed result | Assessment |
| --- | --- | --- |
| USB help/status, unarmed direction, 5 s arming expiry, overlong input | Help/status received; both unarmed/expired requests rejected; malformed line stopped/disarmed | Passed software/serial checks |
| USB close/reopen after arming | Reconnected disarmed, direction 0, STBY readback 0 | Passed disarmed reconnect check; not a running-disconnect timing measurement |
| Forward 25%, 500 ms | Hall deltas A=0/B=0, automatic STBY low; owner observed no movement or sound | Motion not demonstrated |
| Reverse 25%, 500 ms | Hall deltas A=0/B=0; STBY readback high during pulse then low | Motion not demonstrated |
| Forward 50%, 500 ms (explicit owner approval) | Hall deltas A=1/B=1, invalid=0; owner observed no movement or sound | Not evidence of sustained rotation |
| Forward 50%, 5 s (explicit owner approval) | `pwm_cc=500,0`, running counter, STBY readback high; `observed_ms=5000`, Hall deltas A=0/B=0, invalid=0; final STBY and PWM zero | Timed output configuration observed; motor-driven rotation not demonstrated |

Between the initial short tests, both Hall counters changed (A=33/B=31,
invalid=0) without a commanded pulse. This confirms observed input transitions,
not calibrated direction, rotation or absence of electrical noise. Counters
reset on subsequent flashing. No oscilloscope, driver-output voltage/current,
mechanical stopping-time or reset-bias measurements were captured.

The final image was left running **disarmed**, STBY low, PWM compare levels zero,
with the serial port closed. Powered-motor acceptance remains incomplete.
Next investigate the actual breakout labels/enable wiring and signals at its
inputs/outputs; do not infer working motor drive from Pico GPIO/PWM telemetry.

### Manual right-Hall check, 2026-10-30

Using the same flashed image and COM7, explicitly stopped/disarmed before each
snapshot; STBY and PWM compare readbacks remained zero. Owner manually turned
the motor shaft; no powered motion commands were sent.

| Manual action reported | A edges (before -> after) | B edges (before -> after) | Tentative x4 delta | Invalid delta |
| --- | --- | --- | --- | --- |
| Five turns right | 29 -> 123 (+94) | 29 -> 121 (+92) | +172 | 0 |
| Different number of turns left; motor shaft confirmed | 123 -> 269 (+146) | 121 -> 268 (+147) | +79 | 0 |

Both Hall inputs detected transitions in both observation windows, with no
observed invalid two-bit transitions. This demonstrates signal activity during
manual movement, not calibrated counts per revolution or reliable direction.
The tentative position delta remained positive in the reported reverse test,
so consistent quadrature direction is **not verified**. A controlled,
single-direction capture in each direction is needed before relying on signed
position or RPM. The reverse turn count was unspecified; do not derive encoder
resolution from these measurements. Logs: `build-right-motor/bench-hall-manual.log`
and `build-right-motor/bench-hall-manual-reverse.log`. Serial port closed after
the check; motor left disarmed.

### Post-bridge repair check, 2026-10-30

Owner confirmed continuity GP8 -> BIN1, GP9 -> BIN2, both BO outputs to motor
leads, and shared ground. Owner found and cleared a bridge involving GP14/STBY
and GND, then confirmed GP14 -> STBY continuity and no remaining STBY-to-GND
short with power disconnected. Secured bench and one 5 s/50% forward pulse
were explicitly approved.

The same flashed image completed that pulse with `observed_ms=5000`,
`pwm_cc=500,0`, and changing PWM counter/GP8 pad samples. **STBY pad readback
remained 0 throughout the pulse despite firmware commanding it high.**
Hall counts stayed A=271/B=269 (both deltas zero, invalid delta zero).
This differs from the earlier high STBY readbacks and requires electrical
investigation before further powered tests. Neither a residual load/short nor
GPIO damage is established by this log.

Log: `build-right-motor/bench-post-bridge-forward50long.log`.
Final explicit stop confirmed disarmed, STBY readback 0 and PWM compare levels
zero; COM7 was closed. No motor-driven rotation demonstrated.

### Floating-STBY check, 2026-10-30

Owner reported STBY-to-GND resistance of 515 kOhm with power off, clarified
that GP14-to-GND read OL with USB disconnected, and measured GP14 at 0 V while
disarmed. Earlier powered continuity readings are not evidence of a short;
GPIO damage is not established.

Owner explicitly approved disconnecting GP14 from STBY and one forward
5 s/50% pulse with the wheel secured and physical disconnect ready.
Built/flashed with `WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED=ON`; this test image
does not initialize, read or write GP14. Normal firmware retains its original
STBY behavior.

Telemetry confirmed `stby=UNCONTROLLED gp14=UNTOUCHED`, active
`pwm_cc=500,0`, then completion at `observed_ms=5000`. Hall deltas were
A=0/B=0, tentative x4=0, invalid=0: rotation was not demonstrated.
Floating STBY may leave the driver disabled; its actual voltage was not measured.
Owner subsequently clarified that motor power was still disconnected during
this first floating-STBY pulse, so it is not evidence about powered driver
behavior.
Log: `build-right-motor/bench-floating-stby-forward50long.log`.
Final explicit stop confirmed disarmed with both PWM compare levels zero;
COM7 closed. STBY is uncontrolled, not asserted low, in this flashed variant.

Owner requested a repeat after reporting the disconnected supply. The same
5 s/50% forward pulse was repeated without firmware changes or GP14 access.
Completion was observed at exactly 5000 ms; Hall deltas remained A=0/B=0,
tentative x4=0, invalid=0. PWM compare was 500,0 during the pulse and 0,0
afterward. Supply voltage and floating STBY voltage were not measured during
this repeat, and physical motion was not independently observed.
Log: `build-right-motor/bench-floating-stby-powered-repeat.log`.
Final explicit stop confirmed disarmed with PWM zero; COM7 closed.

### No-serial boot-pulse check, 2026-10-30

Owner explicitly selected a 3 s boot delay, one 5 s/50% forward pulse and
automatic cutoff, with STBY still floating and GP14 disconnected. Owner
confirmed the secured bench and motor power OFF for flashing.
Built/flashed `WHISKEY_RIGHT_MOTOR_BOOT_PULSE=ON` together with
`WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED=ON`. Both test and normal firmware
targets built, including compile-time pulse-limit tests.

After instructions to fully power-cycle the Pico and run the supervised test,
owner reported **no motion or sound**; no voltages were measured. This
observation does not support serial command handling as the sole explanation
for earlier failures, but it does not verify driver enable or electrical PWM
timing. No serial telemetry is available in this mode.

The boot-pulse image remains flashed: it can attempt motion on subsequent
non-watchdog boots, then clears PWM after its bounded pulse. GP14 remains
untouched and STBY uncontrolled. Restore the interactive image with
`WHISKEY_RIGHT_MOTOR_BOOT_PULSE=OFF` and reflash when finished; BOOTSEL copying
is required if USB software reboot is unavailable.

### Full-duty boot-pulse check, 2026-10-30

Owner reported VM remained 5.98 V during the earlier 50% pulse (5.99 V idle)
and about 272 mV across the motor only during its 5 s window. Owner connected
STBY externally to 3V3 with GP14 disconnected; an actual HIGH STBY voltage
measurement remains unrecorded.

Owner explicitly approved one **100%/1 s** forward boot test after a 3 s delay,
with secured/lifted wheel, 6 V motor supply, disconnect ready and motor power OFF
for flashing. Built with boot-pulse, boot-full-duty and STBY-disconnected options
ON. Both firmware targets built and compile-time full-duty duration tests passed.
UF2 SHA-256:
`2DC89C277CE987F58A50E55F23C27F1050D0C6357BAA136AE13FEB6EDDEB79D7`.
Flashed through the identified RPI-RP2 BOOTSEL volume.

After instructions to fully remove Pico power and run the supervised test,
owner reported **no motion or sound**; motor terminal voltage was not captured.
Thus full commanded duty did not demonstrate rotation, but this does not
establish actual driver output voltage or rule out startup-current limiting.
Next verify actual STBY/input voltages and identify the driver board before
increasing test duration or making further powered attempts.

The full-duty boot image remains flashed: normal boots can attempt one 1 s
full-duty pulse after 3 s, then clear PWM. GP14 remains untouched; no USB serial
or serial stop is available. Keep motor power disconnected outside a supervised
test. Restore the interactive image with both boot options OFF and reflash via
BOOTSEL when finished.

### Successful powered rotation after restoring STBY, 2026-10-30

Owner found that the external STBY connection had become disconnected,
reconnected it, and repeated the full-duty boot test. Owner reported **the motor
spun**. No additional firmware changes or agent-initiated pulses were made.
This demonstrates owner-observed powered rotation with the external enable
connection restored; the preceding no-motion full-duty result is not evidence
that a correctly enabled driver cannot start the motor.

The test does not establish that 100% duty is necessary: lower-duty tests must
be repeated with enable confirmed before drawing a starting-duty conclusion.
Physical forward polarity, powered Hall counts, reverse operation, measured
current and actual stop timing remain unverified. The full-duty boot image
remains flashed with the automatic-start precautions above.

### Manual USB Hall direction/speed tests, 2026-10-30

Restored the interactive USB image with **both boot options OFF**, leaving
GP14 untouched and external STBY securely on 3.3 V per owner confirmation.
There is no automatic motion at startup in this image.

Added positive/negative transition totals, pulse deltas and one-second
`rate` diagnostics. `signed_x4_per_s` uses actual elapsed time and is not RPM.
`mark` followed by exactly 10 hand-turned motor-shaft revolutions and `cal10`
can establish motor counts/revolution in RAM. Calibration rejects missing
channels, invalid transitions, any mixed signs, or a missing mark.
Starting a powered pulse cancels a pending hand calibration.
RPM is `signed_steps * 60000 / (elapsed_ms * motor_cpr_x4)`, and is withheld
if uncalibrated or if a rate window contains invalid/mixed-direction steps.
Sign is the Hall convention, not a physical wagon-forward label.
Calibration is lost on reboot. No gearbox/wheel RPM is automatically inferred.

Owner reported exactly 10 clockwise motor-shaft turns, then a separately marked
10 counterclockwise turns, with motor supply OFF and USB/Hall power ON:

| Reported hand rotation | A edge delta | B edge delta | Positive steps | Negative steps | Invalid |
| --- | --- | --- | --- | --- | --- |
| 10 clockwise turns | 60 | 61 | 119 | 2 | 0 |
| 10 counterclockwise turns | 48 | 52 | 2 | 98 | 0 |

Signs reverse as expected, but counts/revolution are not repeatable, and both
windows contain a small number of opposite steps. Both calibrations were
explicitly rejected. Do not adopt a guessed 12 counts/revolution from these
observations. Logs: `hall-calibration-clockwise.log` and
`hall-calibration-counterclockwise.log` in `build-right-motor/`.

Owner approved separate forward/reverse 5 s/50% pulses. Forward registered only
one negative step and reverse four positive steps, then zero sustained rate;
owner reported no physical motion. Log: `hall-powered-direction-speed.log`.

Owner then approved separate forward/reverse 1 s/100% manual pulses. Fresh
arming and a 1.5 s stopped interval preceded each pulse:

| Electrical command | Pulse observation | A edge delta | B edge delta | Signed x4 delta | Invalid |
| --- | --- | --- | --- | --- | --- |
| `fwd100` (GP8 HIGH, GP9 LOW) | 1000 ms | 2127 | 2126 | -4253 | 0 |
| `rev100` (GP8 LOW, GP9 HIGH) | 1000 ms | 2156 | 2156 | +4312 | 0 |

During each pulse, all steps had the expected single sign. **Powered direction
is distinguishable:** electrical forward is negative (consistent with the
owner's hand-counterclockwise convention), electrical reverse is positive.
Viewing end and wagon-forward polarity still need physical confirmation.
Pulse-average observed rates are approximately -4253/+4312 x4 counts/s;
these include acceleration and the software snapshot boundary, not a
steady-state speed measurement. Subsequent logs contain coast-down edges.
Absolute motor/output RPM remains uncalibrated, not zero or guessed.
Log: `build-right-motor/hall-full-duty-direction-speed.log`.

Final explicit stop confirmed PWM zero and disarmed; USB port closed. The
interactive image remains flashed, with external STBY uncontrolled by firmware.
Motor supply should remain OFF for the next known-revolution calibration.

### Reference-marked Hall calibration investigation, 2026-10-30

Owner confirmed the exposed motor shaft/encoder magnet before the gearbox,
with motor supply OFF and USB/Hall supply ON. No powered pulses or firmware
changes were made during this calibration investigation.

| Reported motion | A edge delta | B edge delta | Positive steps | Negative steps | Net x4 | Invalid |
| --- | --- | --- | --- | --- | --- | --- |
| 10 marked clockwise revolutions | 326 | 324 | 649 | 1 | +648 | 0 |
| 1 counterclockwise revolution, directly turning magnet | 26 | 26 | 0 | 52 | -52 | 0 |
| 1 clockwise revolution (owner corrected requested direction) | 32 | 30 | 61 | 1 | +60 | 0 |
| 1 counterclockwise revolution, magnet/housing reference marks aligned | 22 | 22 | 0 | 44 | -44 | 0 |

The 10-turn calibration was rejected due to mixed-direction steps. The
single-turn observations independently show non-repeatable resolution; even
the two clean counterclockwise turns differ by eight x4 counts (52 versus 44).
A five-second stationary observation between samples had no new edges or
steps. Zero invalid transitions does not establish that no edges were missed,
or that all counted transitions correspond to intended shaft motion.

Direction sign remains consistent with observed hand rotation, but **no
counts/revolution value was accepted or persisted**. RPM remains explicitly
unavailable. Do not average these counts or substitute a guessed CPR.
Before more manual calibration, inspect magnet/shaft coupling, reference-mark
geometry and the actual sensor electrical interface; use a logic analyzer
on both channels during a reference-marked revolution or an independent
tachometer to distinguish acquisition/signal issues from turn-count issues.
Do not force or loosen the encoder magnet.

Logs in `build-right-motor/`: `hall-calibration-repeat1-start.log`,
`hall-calibration-repeat1-result.log`, `hall-single-magnet-ccw1.log`,
`hall-single-magnet-cw1.log`, `hall-calibration-stationary.log`, and
`hall-single-magnet-ccw-aligned.log`.
Final state was disarmed, PWM compare 0,0, GP14 untouched; COM7 closed.

## Hall-only rotation and direction feedback

Throttle integration was explicitly deferred by the owner; no throttle image
was flashed, and its unflashed source/target changes were removed. The existing
manual motor bench image now uses `hall_motion.hpp` to report Hall-only motion
states every 100 ms while USB serial is connected. It does not depend on RPM
calibration or commanded motor direction:

- `HALL_POSITIVE` / `HALL_NEGATIVE`: latest clean sample window's direction.
- `UNCERTAIN_MIXED_OR_INVALID`: both signs or invalid transitions in a window;
  direction is withheld until a clean directional window or the no-edge timeout.
- `NO_RECENT_EDGES`: no edge ever observed, or last edge is at least 250 ms old.
  This is not proof of mechanical stop: missing sensor power/wiring can look
  identical, and sufficiently slow motion can produce gaps longer than 250 ms.

Logs include edge age in milliseconds (`-1` if no edges yet). Since reporting
is sampled at 100 ms, timeout is visible at the next sample, normally 250-350 ms
after the last edge. Physical CW/CCW depends on viewing end; owner observations
from the exposed encoder end associate positive with clockwise and negative
with counterclockwise. No calibration constant was accepted.

Compile-time tests cover both signs, retained direction between edges, timeout
at exactly 250000 us, no-edge startup, mixed/invalid states and recovery.
Both normal and manual test firmware built; the normal entry point is unchanged.

2026-10-30 live manual validation: motor supply OFF, USB/Hall supply ON, no
powered commands. Owner turned clockwise, stopped, then counterclockwise and
stopped. Captured state transitions:

| Uptime ms | State | Last edge age ms |
| --- | --- | --- |
| 20789 | HALL_POSITIVE | 50 |
| 23489 | NO_RECENT_EDGES | 282 |
| 24389 | HALL_NEGATIVE | 56 |
| 26790 | NO_RECENT_EDGES | 333 |
| 26990 | HALL_POSITIVE (brief final opposite-sign activity) | 64 |
| 27190 | NO_RECENT_EDGES | 264 |

Both intended directions and subsequent timeout states were observed. A brief
positive activity after the negative run was also captured; its mechanical
cause was not established, so this is not evidence of perfectly noise-free
direction at rest. Final cumulative invalid transitions were zero.
Log: `build-right-motor/hall-motion-manual-live.log`. Final state disarmed,
PWM compare 0,0, GP14 untouched; COM7 closed. Automatic startup remains OFF.
No powered tests were authorized or executed as part of this feedback check.

## Simultaneous two-motor bench test

Owner reported both motors secured without wheels on the test bed, confirmed
GP6/GP7 to AIN1/AIN2, AO outputs to left motor, shared grounds, and external
STBY on 3.3 V with GP14 disconnected. Owner approved one 1 s/100% simultaneous
electrical-forward pulse. Supply capacity/current limit was not supplied;
verify supply/driver capacity for combined starting current before powering.

`arm` then `both100` drives both input pairs at full duty for at most 1 second.
Both PWM pairs are cleared by all stop paths, including the independent alarm,
USB disconnect and malformed commands. There is no automatic startup, throttle
integration or simultaneous reverse command. Other drive commands remain
right-only. `bridge_pwm` reports left/right compare pairs (1000,0 during the
simultaneous pulse; 0,0 after). The two writes are consecutive within the same
interrupt-masked startup, not a phase-synchronized PWM claim.

Left Hall inputs remain unconfigured because their interface/wiring has not
been reconfirmed; Hall motion and pulse counts still describe the right motor
only. Verify both motors physically spin together and stop after the pulse;
PWM readback alone cannot establish motion. Monitor supply sag, unexpected
heating or stationary humming and cut motor power immediately if needed.

Executed 2026-10-30 after owner confirmed supply/driver suitable for combined
startup, motor power OFF for flashing, then motor power ON/STBY connected for
the authorized single pulse. Unarmed `both100` was first rejected with both
compare pairs zero. During the armed test both compare pairs read 1000,0 and
GP6/GP8 HIGH, GP7/GP9 LOW; completion was observed at exactly 1000 ms, followed
by explicit stop and both compare pairs zero. Right Hall deltas were A=2054,
B=2054, net x4=-4108, invalid=0. Left Hall was not captured.

**Owner observed only the right motor spin**, with no other reported symptoms.
Simultaneous two-motor rotation is therefore NOT verified. Left PWM configuration
readback is not proof of a signal at the driver or voltage across its output.
Next investigate the left input/output path or test that channel separately,
with further powered tests requiring explicit approval.
Logs: `build-right-motor/dual-motor-unarmed.log` and
`build-right-motor/dual-motor-full-duty.log`.
Owner requested one repeat while measuring voltage. Both bridge compare pairs
again read 1000,0 during the 1000 ms pulse and zero after explicit stop. Right
Hall delta was -4206 x4, with zero invalid transitions. Across left AO1/AO2
(motor terminals), owner saw a brief jump of several volts but could not read
the value; neither motor's physical motion was observed for this repeat.
This establishes an owner-observed voltage change, not measured output
amplitude, current or left-motor rotation.
Log: `build-right-motor/dual-motor-voltage-repeat.log`.
Manual image left flashed, automatic startup OFF, both PWM pairs zero, disarmed,
GP14 untouched; COM7 released.

### Left-only voltage test, 2026-10-30

Owner explicitly selected a 2 s/100% left-only pulse so a basic autoranging
meter could settle. Motor supply was confirmed OFF for flashing, then ON with
STBY connected, shafts clear and disconnect ready before the single pulse.
`left100` was first rejected unarmed with both PWM pairs zero.
During the authorized pulse left compare read 1000,0 and right remained 0,0;
completion was observed at exactly 2000 ms. Both pairs returned to zero.
The existing one-second right/both commands remain unchanged. Compile-time
tests cover the new command and two-second full-duty expiry boundary.

Owner measured **5.99 V across the left motor terminals during the pulse**,
but reported **no movement or sound**. This points investigation toward the
motor/lead/contact or mechanical path; voltage alone does not establish current
flow, and an unloaded/high-resistance connection can still show full voltage.
Confirm the probes were at the motor's actual winding terminals rather than
only the driver's outputs before concluding the motor itself is faulty.
No current measurement or left Hall capture was made. Right Hall zero counts
are expected in this left-only test, not evidence about the left motor.

Log: `build-right-motor/left-only-voltage-test.log`. Final state disarmed,
both PWM pairs zero, GP14 untouched, automatic startup OFF, COM7 closed.
Before more powered attempts, disconnect USB and motor supply and inspect
left motor leads/terminals and shaft freedom without forcing the gearbox;
compare winding resistance with the known-working motor if accessible.

Owner subsequently found one left motor lead lacked continuity, checked/repaired
the harness, and confirmed continuity/restored supply/STBY and secured test-bed
readiness. The explicitly approved repeat used `left100`, 100% for 2000 ms;
left compare was 1000,0 and right remained 0,0, followed by both pairs zero.
Owner observed **left motor spun and coasted to a stop**. No voltage reading
was captured. This confirms left-only powered rotation after the harness fix;
simultaneous rotation still requires a separate repeat. Left Hall capture remains
disabled. Log: `build-right-motor/left-only-post-harness-repair.log`.
Final state disarmed, PWM zero on both bridges, COM7 closed.

Owner requested the next simultaneous test after the successful left-only
repair check. One `both100` pulse ran for an observed 1000 ms, with both
compare pairs 1000,0 during the pulse and 0,0 after explicit stop. Right Hall
deltas were A=2049/B=2049, net x4=-4098, invalid=0, all negative during
the pulse. A mixed-direction sample appeared during coast-down; this is not
evidence of a commanded reversal.
Owner confirmed **both motors spun together and coasted to a stop**, with no
other reported symptoms. Thus simultaneous unloaded 100% operation has
owner-observed confirmation following the harness repair; left Hall feedback,
loaded operation, current and supply sag were not measured.
Log: `build-right-motor/dual-motor-post-harness-repair.log`.
Final state disarmed, both PWM pairs zero, GP14 untouched, automatic startup
OFF, COM7 released.

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

## First integrated POC bench test

The first POC is **open-loop and supervised-bench-only**, not the final
encoder-regulated wagon. Preserve the existing manual motor-test image/procedure.
Do not flash a powered POC onto the current externally enabled STBY arrangement.

### Tomorrow: controller tests without actuator power

1. Disconnect motor and servo power. Keep unknown/unverified Hall interfaces
   disconnected. Use `whiskey_wagon_poc` with actuator verification OFF:

   ```sh
   cmake -S . -B build -DWHISKEY_BUILD_POC=ON -DWHISKEY_POC_ACTUATORS_VERIFIED=OFF
   cmake --build build --target whiskey_wagon whiskey_wagon_poc
   ```

   Flash `build/whiskey_wagon_poc.uf2` through BOOTSEL with actuator power off.
   Verify the banner says `actuators=INHIBITED`, heartbeat `enabled=0`, and all
   five motor signals stay low; GP2 emits no servo pulses.
2. Monitor UART0 TX GP0 at 115200/8N1 with a 3.3 V-compatible adapter/shared
   ground. No USB runtime console or serial arm/stop exists. Select Micro
   D-input mode and pair with only the intended controller discoverable.
3. Verify physical B/X/Y against raw `buttons`, D-pad against raw `dpad`.
   Candidate masks are B=0x0001, X=0x0008, Y=0x0004, left=0x08, right=0x04.
   Correct board configuration if observation differs before any powered build.
4. Verify held-at-connect cannot drive; neutral then fresh B/X can. Exercise
   overlaps, same-report ties, winning-button release, Y while B/X remains held,
   a fresh restart, steering centering and opposing D-pad directions.
   Confirm requested full-scale drive ramps take 1.0 s up / 0.7 s down and
   steering takes 0.5 s center-to-endpoint. Full-scale duty is the configured cap,
   not 35 RPM. Reversal must reach zero then dwell before the opposite ramp.
5. Hold B, X, and D-pad separately for at least 30 s. Record report counts,
   `max_gap_ms`, `age_ms`, and actual timestamps with unchanged controls.
   Require a measured margin below the configured 500 ms lease. If unchanged
   reports are suppressed or the controller is incompatible, keep power inhibited
   and investigate transport/report servicing; a connection flag is not a lease.
6. Power off the controller mid-demand. Verify lease expiry or disconnect revokes
   qualification, drive releases to zero, and steering centers. Reconnect while
   holding B/X, then verify neutral/fresh-press qualification again.
   Test pairing persistence, the GP15 hold request and pairing-expiry sleep/wake.

### Before enabling powered outputs

- Secure/lift both wheels, remove bottles/load, and keep an accessible physical
  power disconnect. Verify regulated 6 V motor / 4.8 V servo rails and current
  budgets independently. USB upgrade/backfeed isolation must be resolved.
- With all power removed, remove the external STBY-to-3V3 connection. Verify
  GP14 electrical health/3.3 V enable compatibility before reconnecting it to
  STBY. Measure low disable/high enable, coast truth table and reset/brownout
  inhibition. Never tie a driven-high external STBY directly to GP14 driven low.
- Verify BOTH Hall interfaces, external biasing and consistent A/B edges on
  both motors. Enable raw capture only after 3.3 V acceptance. Test the left
  channels first; latest motor tests captured only the right. Record forward
  electrical polarity separately for each wheel and select inversion flags.
- With servo linkage disconnected, establish safe SG92R center, physical-left/
  right pulses, signal compatibility and rail stability. Align horn at center,
  then verify linkage travel without binding. Defaults 1400/1500/1600 us are
  provisional, not permission to force the linkage.
- Record measured controller cadence and approve the lease; record duty/startup
  behavior, Hall edge rates and provisional silence/invalid thresholds. A
  stationary/humming motor at the 25% default needs an explicit stop and
  investigation, not an automatic increase or full-duty startup boost.

After all gates pass, use a **separate build directory** and explicitly set
`WHISKEY_ENCODER_CAPTURE_VERIFIED=ON`, `WHISKEY_POC_ACTUATORS_VERIFIED=ON`,
and the measured settings listed in [firmware configuration](firmware.md#poc-configuration-and-verification-gate).
Build `whiskey_wagon_poc`; inspect its startup configuration before applying power.
There is no unattended startup drive and no operator arming command.

Test each direction briefly with lifted wheels, then Y, release, reversal,
disconnect, pairing sleep and watchdog/reset behavior. Verify actual outputs,
not just requested demands. At a controlled nonzero duty >=100 permille,
loss of either Hall channel must coast both motors within the provisional
500 ms monitoring silence threshold (+ one control tick), after startup grace.
Inject faults with an appropriate controlled fixture/power-off rewiring, never
short a live GPIO. Confirm a fault disables servo pulses and cannot be cleared
by reconnect or new button reports. Servo cannot provide commanded centering
after a latched fault because its pulses are off.

This draft does not validate loaded travel, stopping distance, encoder CPR,
measured wheel RPM, speed regulation, calibrated differential turns or electrical
protection. No-edge reversal qualification does not prove the wheels stopped.
Keep powered tests supervised and bench-only until these remaining gates pass.

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
convenient value in firmware. The POC's explicitly provisional thresholds are
software test candidates, not accepted hardware limits.

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
| 2026-10-31 | First open-loop POC software verification | SDK 2.3.1, Arm 15_2_Rel1; Release default + POC, Debug existing USB motor diagnostic, separate powered compile check with inverted motor/servo and 1000-permille cap; 100-permille cap / 100 ms lease boundary build also passed | Existing bench and seven POC constexpr suites passed, no compiler warnings; powered build without encoder verification rejected as expected. Validation images restored to inhibited outputs/capture OFF. No flash or hardware test |
| 2026-10-31 | Inhibited POC artifact identity | `build/whiskey_wagon_poc.uf2`, 1,228,288 bytes, SHA-256 `095701F0A17113B44685FC39C1A9A67D6E647DE05FA7313FA81A7003641B6C7D` | POC init/watchdog symbols present; inhibited image contains no motor/servo PWM init/set or USB stdio init symbols. Original image also retains static PWM/USB inhibition. Static checks do not validate electrical levels or physical stop behavior |
| TBD | Bluetooth library readiness | Current bench sequence above; all actuator power disconnected, automatic pairing/reconnect expected | Not run |
| TBD | Output-only UART baseline and electrical checks | Procedure above; owner-reported P1 wiring, VM disconnected; USB stdio and commands removed | Pending UART TX logs, operation without a listener, physical GPIO measurements, and reset observations |
| TBD | Bluetooth/controller compatibility | Dependencies pinned, 8BitDo Micro D-input selected; actual pairing, mapping, report timing and persistence evidence pending | Not run |
| TBD | Hardware and motion verification | P1 driver wiring owner-reported; electrical interface and powered operation not verified | Not run |
