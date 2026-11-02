# Firmware design

## Open-loop wagon POC (2026-10-31)

`whiskey_wagon_poc` is a separate target sharing the existing Bluetooth adapter,
10 ms BTstack control timer, pairing persistence/lifecycle and button precedence.
The original `whiskey_wagon` target retains its inhibited-only assertion and
does not configure motor or servo PWM. No image was flashed for this draft.

The owner selected an open-loop first POC pending calibration. Two motor
profiles in `poc_config.hpp` record 6 V, 150 mA running / 500 mA stall,
164:1 reduction (tachometer-derived; labeled 171:1) and 35 RPM rated output.
The Hall encoders give 44 x4 counts per motor revolution; the `hall` status line
reports live `motor_rpm` and `wheel_rpm` (one decimal) from count deltas between
status prints. They are measurement only, not a speed setpoint or regulator.
Wheel coupling and loaded speed remain unknown. Both motors receive equal bounded duty, with
independent polarity inversion. There is no speed regulator or differential
mixing until encoder/steering geometry calibration supports them.

`poc_logic.hpp` wraps the existing smoothstep demand ramps and adds:

- B/X/Y release qualification and fresh-press precedence, unchanged from preview.
  Y immediately sets both drive demands to zero; steering continues normally.
- Configurable smoothstep acceleration from zero to the duty cap, default 1000 ms.
  The owner-selected next bench image uses 300 ms and a 1000-permille (100%) cap.
  Release deceleration remains 700 ms and steering travel remains 500 ms.
- A provisional 500 ms valid-report lease using the same wrapping 32-bit BTstack
  clock as report timestamps. Expiry/disconnect synthesizes release, so drive
  ramps down and steering centers. A late held report cannot renew old demand.
  A fresh neutral report then fresh drive press is required. The maximum
  nominal full-demand timeout-to-zero is lease + 700 ms + one 10 ms tick.
  This is electrical demand latency, not physical stopping time.
- Connected-link hold (owner-approved, Exterminate-style): the 8BitDo Zero 2
  reports only changes, so while the controller stays connected the control
  tick renews the lease of the last fresh report (`link_alive`). This never
  creates, qualifies or revives input. Disconnect still releases immediately,
  but radio loss is only detected by the controller-side Bluetooth supervision
  timeout (default 20 s; owner-measured about 20 s and accepted 2026-11-01);
  the physical motor cutoff is the backstop. Attempts
  to shorten supervision from the Pico (write/readback, master role switch)
  did not take effect on this controller and were removed.
- Quiet-link neutral: the Zero 2 sends nothing at connect, so qualification would
  consume the first press. If no report arrives within 500 ms
  (`kPocQuietNeutralMs`) after HID readiness, the controller is treated as at
  rest and the first B/X press drives. A non-neutral report inside that window
  still blocks driving until released.
- A 250 ms zero-duty reversal dwell; when Hall monitoring is enabled, both
  motors must also have no edges for 250 ms. Continued edges blocking reversal
  for 2000 ms latch a fault. Demand ramps stay at zero while waiting, with
  steering still updating. No edges is not proof of mechanical rest.
- Powered-only Hall guards: any newly observed invalid transition at nonzero
  drive duty faults; at duty >=100 permille, a 500 ms absence of either channel
  on either motor faults, after a 500 ms monitoring startup grace. These are
  conservative provisional bench thresholds, not validated fault coverage.
  Low-duty silence is not diagnosed. Single-channel toggling is not treated as
  healthy feedback. Both motors coast and servo pulses stop on a latched fault.
  Only reboot clears a fault; new controller reports cannot clear it.
- A control tick elapsed time >100 ms while drive demand is nonzero latches an
  overrun fault. While idle, a stall cannot leave stale motor output, so it is only
  counted (`idle_stalls`, `max_idle_stall_ms` in the status line); Bluetooth
  startup/pairing stalled the run loop under 200 ms on 2026-11-01. A 200 ms hardware
  watchdog, not paused by a debugger, resets a stalled run loop. It starts after
  synchronous Bluetooth initialization while outputs are still zero. Reset-time
  inhibition requires hardware biasing; watchdog reset is not a mechanical brake.

`poc_outputs.cpp` owns motor PWM slices 3/4 at nominal 20 kHz (TOP=999).
Nonzero commands use DRV8833 slow decay: forward holds xIN1 high and PWMs xIN2
at 1-duty, reverse mirrors it, so the off phase brakes rather than coasts.
The first integrated test (2026-11-01) faulted Hall-silent with fast-decay PWM at
the 25% cap and zero Hall edges on both motors; fast decay also failed to start the
right motor at 50% on 2026-10-30. Zero
sets both low. STBY is low when drive duty is zero, high only after applying
nonzero duty. There is no stop braking. SG92R uses slice 1 on GP2, nominal
50 Hz with 1 us counts. Boot starts with all outputs low. In a powered image,
the first control tick starts the verified center pulse; sleep/fault stops pulses.
Y output changes are applied in the input callback as well as on the next tick.
PWM compare writes are buffered by the SDK until the next PWM wrap.

### POC configuration and verification gate

`WHISKEY_BUILD_POC` defaults ON, but both powered-output options default OFF:
all motor signals stay low and no servo PWM is configured. Raw Hall capture
remains separately gated by `WHISKEY_ENCODER_CAPTURE_VERIFIED`.
Powered configuration is rejected unless both-motor encoder capture is enabled.
`WHISKEY_POC_ACTUATORS_VERIFIED` is an explicit physical-bench attestation,
not auto-detection; do not set it until the
[POC procedure](verification.md#first-integrated-poc-bench-test) passes its
wiring, reset, mapping, cadence, polarity and servo gates. The separate
external-STBY option acknowledges a supervised test with a physical cutoff; it
does not attest reset-time inhibition or the other unverified integration gates.

| CMake setting | Default | Meaning / gate |
| --- | --- | --- |
| `WHISKEY_POC_EXTERNAL_STBY_BENCH` | OFF | Separate supervised bench mode with external STBY enable and GP14 untouched. Mutually exclusive with `WHISKEY_POC_ACTUATORS_VERIFIED`; both motor Hall interfaces must be verified. Firmware cannot disable STBY; a physical motor-power cutoff is required |
| `WHISKEY_POC_MAX_DUTY_PERMILLE` | 250 | 25% cap, allowed 100-1000; not a calibrated starting duty. If stationary, stop and investigate; never add an automatic full-duty kick |
| `WHISKEY_POC_ACCELERATION_MS` | 1000 | Zero-to-cap smoothstep ramp, allowed 100-1000 ms; changing acceleration does not change release, steering, reversal dwell or input freshness |
| `WHISKEY_POC_REPORT_TIMEOUT_MS` | 500 | Allowed 100-2000 ms; report lease, renewed by the connected-link hold while the controller stays connected. Do not disable freshness protection |
| `WHISKEY_POC_SERVO_LEFT_US` | 1400 | Provisional physical-left endpoint, not a manufacturer-approved linkage limit |
| `WHISKEY_POC_SERVO_CENTER_US` | 1500 | Provisional center; establish horn alignment with linkage disconnected |
| `WHISKEY_POC_SERVO_RIGHT_US` | 1600 | Provisional physical-right endpoint; endpoints can be reversed |
| `WHISKEY_POC_LEFT_INVERTED`, `WHISKEY_POC_RIGHT_INVERTED` | OFF | Change independently after measuring physical-forward polarity with lifted wheels |
| `WHISKEY_POC_USB_CONSOLE` | OFF | Bench only: route the same output-only diagnostics to USB CDC instead of UART0 when no UART adapter is available. No commands; output requires DTR and is dropped without a host. USB baud-rate/vendor resets are disabled, so use BOOTSEL to reflash |

The 2026-11-01 held-button test in the controller's observed Xbox-compatible
mode exceeded the lease. A separate inhibited probe received only handshake
acknowledgments to repeated input `GET_REPORT` requests, with no continued
held-state reports. Neither acknowledgments nor cached state establish fresh
input; sustained drive in this mode remains unverified.

Servo values must be distinct, center between endpoints, each 1000-2000 us.
This validation bounds configuration, not actual servo safety; linkage acceptance
remains mandatory. The normal powered POC requires GP14 to control STBY with
verified reset-time inhibition. The separate externally enabled bench mode
does not provide this protection; its boot banner and heartbeat report
`stby=UNCONTROLLED gp14=UNTOUCHED`. PWM inputs are commanded low on stop
or fault, but cannot provide a hardware STBY disable or guarantee reset-time
inhibition. Follow the [external-STBY procedure](verification.md#externally-enabled-stby-bench-test)
and keep motor power disconnected outside a supervised test. UART0 on GP0 remains
the default output-only diagnostic
interface; `WHISKEY_POC_USB_CONSOLE=ON` substitutes an output-only USB CDC console
for bench use. Status separates requested duty/servo pulses
from raw Hall counts and explicitly withholds RPM. Fault numbers:
0=none, 1=Hall invalid, 2=Hall silent, 3=reversal timeout, 4=control overrun.
Existing raw input logs support tomorrow's physical mapping check.
Logs are coalesced to 10/second plus heartbeat; UART timing and Bluetooth stalls
still need on-board measurement against overrun/watchdog bounds.

Deterministic compile-time tests cover output caps/endpoints/inversion, profile
shape, input qualification/loss/late reports, timestamp wrap, stop, normal and
Y-triggered reversal/dwell, continued-edge timeout, Hall grace/exact silence
threshold, missing single channel, invalid transitions and latched fault recovery.

## Isolated USB servo test (2026-11-01)

The opt-in `servo_test` image is separate from the Bluetooth POC and motor test.
It initializes GP2 and the four motor inputs low, leaves GP14 untouched, and
requires motor power to be disconnected. STBY is therefore uncontrolled, not
asserted low. USB CDC with DTR is the only command interface.
USB baud-rate and vendor-interface resets are disabled in this test image;
use BOOTSEL with actuator power disconnected when changing firmware.

The image starts disarmed without servo pulses. A fresh `arm` permits one
explicit `pulse NNNN` command for a nominal one-second, 50 Hz pulse train.
The image accepts 0544-2400 us (four digits, so `pulse 0544`), matching the
Arduino Servo library's default `attach()` range. The unloaded SG92R swept that
full range quietly; see the [endpoint test](verification.md#unloaded-endpoint-sweep-2026-11-01).
The linkage will restrict usable steering travel further. Arming expires after five seconds, and
a stopped burst requires a 500 ms cooldown before re-arming. The nominal timing
is compared with the [Adafruit and Arduino Servo references](hardware.md#servo-timing-references);
the Arduino library is not used by this firmware.

An independent alarm switches GP2 to its low SIO output at burst expiry rather
than relying on the PWM's buffered compare update. Stop, invalid input and USB
DTR loss also inhibit the signal and disarm; a 200 ms watchdog resets a stalled
loop. These mechanisms do not remove servo power, guarantee a mechanical stop,
or establish reset-time pin behavior. PWM is configured only when the system
clock supports nominal 1 us counts. Requested pulse timing and the timeout
still need physical measurement. Compile-time tests cover parsing, boundaries,
arming, expiry, cooldown and clock configuration; no hardware test is implied.
See the [servo-only procedure](verification.md#servo-only-usb-bench-test).

## Current implementation

The original target is `whiskey_wagon`, C++17, with `pico_stdlib`, UART stdio,
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
  An already-running inquiry stays active when switching between these modes;
  stopping and immediately restarting it can race BTstack's asynchronous cancel.
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

## Standalone right-motor diagnostic image

The opt-in `right_motor_test` CMake target uses USB CDC instead of Bluetooth or
UART. Its separate entry point holds the left bridge inputs low and uses the
right bridge's shared PWM slice at 20 kHz. A verified-bench configure flag is
required; the default firmware and its motor-inhibition assertion are unchanged.
`right_motor_test_logic.hpp` owns one-shot arming (5 s), default 25%/500 ms pulses,
explicit 50% diagnostic commands (500 ms short or 5 s long) and 500 ms cooldown.
Compile-time tests cover pulse boundaries, duration/duty validation, reversal
rejection, stop/re-arm behavior and exact command matching.

The main loop bounds input processing and checks USB DTR, while an independent
alarm callback inhibits STBY/PWM at the pulse deadline. A 200 ms watchdog resets
a stalled loop without pausing for a debugger; reset-time electrical inhibition
still requires hardware biasing. Unknown/invalid commands inhibit with an error.
Only right Hall capture is enabled; encoder snapshots reuse the existing
IRQ-masked capture implementation. No direction, CPR, RPM or stall detection is
inferred from raw Hall edges. See the
[bench procedure](verification.md#right-motor-usb-bench-test) for operational limits.

The diagnostic-only `WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED` option omits all
GP14 access in this target, including status reads. It requires the STBY wire
disconnected and reports an uncontrolled enable; stop/timeout then clears PWM
only. Default test builds retain GP14 control; the normal firmware is unaffected.

Hall diagnostics now include positive/negative x4 steps and actual-time count
rates. Manual `mark`/`cal10` motor-shaft calibration gates signed motor RPM;
mixed-direction or invalid rate windows withhold RPM. Calibration is RAM-only.
Manual `fwd100`/`rev100` use the tested one-second full-duty limit and require
fresh arming. Both boot options OFF restores the normal interactive workflow.

`hall_motion.hpp` classifies recent Hall movement independently of duty/RPM.
The manual test reports every 100 ms; direction expires at 250 ms without
edges. Mixed/invalid windows withhold direction, and no recent edges is not a
claim of a mechanically stopped motor or healthy wiring. No Bluetooth throttle
integration is enabled.

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
opposing D-pad left/right means smooth centering. The left-stick X axis
(`axes[0]`, -512..511, ±64 deadzone, scaled to full lock) also steers when no
D-pad direction bit is set, so modes that report the D-pad as the left stick
(the Micro paired as Xbox Wireless on 2026-11-01) still steer with left/right.
Y immediately selects coast,
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
