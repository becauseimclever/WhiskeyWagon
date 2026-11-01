# Requirements

## Confirmed scope

**First-draft milestone, 2026-10-31:** Owner selected an open-loop Bluetooth/PWM/
SG92R POC with Hall diagnostics and powered outputs gated on bench verification.
This is a temporary bring-up milestone; REQ-11 closed-loop regulation and REQ-12
calibrated differential turning remain final requirements, not implemented POC
features. Motor ratings confirmed today: 171:1 reduction, 35 RPM output shaft.

| ID | Requirement | Verification |
| --- | --- | --- |
| REQ-01 | Build an RC wagon using the Raspberry Pi Pico SDK and C++ | Firmware builds for the selected board |
| REQ-02 | Steer with one standard TowerPro SG92R micro servo | Calibrated steering test without mechanical binding |
| REQ-03 | Drive two motors through one DRV8833 dual H-bridge, channel A = left wheel and channel B = right wheel | Each motor operates independently in both directions on the correct wheel |
| REQ-04 | Read two Hall-effect switch channels per motor (four sensor inputs total) | Known shaft motion produces the expected counts on each channel; verify phase before quadrature decoding |
| REQ-05 | Receive control over Bluetooth using Bluepad32 | Selected controller pairs and produces validated input reports |
| REQ-06 | Use BlueKitchen BTstack examples as Bluetooth bring-up references | Relevant example builds/runs with the chosen SDK stack |
| REQ-07 | Maintain a single authoritative feature-to-pin register | Wiring and firmware agree with the [hardware register](hardware.md) |
| REQ-08 | Reserve GP0 and GP1 for UART0 TX and RX | No wagon actuator or encoder allocation conflicts with UART0 |
| REQ-09 | Physical Micro B = hold for forward, X = hold for reverse, Y = immediate drive-stop request; last new B/X/Y press wins, without user arming/disarming | Verify actual physical-button reports, overlapping holds, stop/restart and release behavior without actuator power first |
| REQ-10 | Smooth normal acceleration, release deceleration, steering movement and return to center to avoid jolting glass bottles | Verify command trajectories, then measure loaded mechanical behavior; low gearbox rpm alone is not proof of smoothness |
| REQ-11 | Independently regulate both wheel speeds using encoder feedback and report measured motion, not just commanded motion | Validate counts, direction, speed tracking, zero-demand/coasting motion, and feedback-failure handling |
| REQ-12 | Differential wheel-speed targets while servo steering: slow the inside wheel, retain requested outside-wheel speed, no pivot turns | Verify left/right and forward/reverse turns against calibrated steering geometry without exceeding requested speed |

## Confirmed motor and power direction (2026-10-25)

- Owner confirms GA25-370, 6 V motors, with supplied ratings of 0.15 A running
  and 0.5 A stall per motor. These are owner-supplied specifications, not measured
  wagon currents. See the hardware register for the listing's no-load speed.
- Closed-loop speed control is required on each wheel. Encoder measurements
  must distinguish commanded demand from observed motion, including movement
  after demand reaches zero. No encoder edges alone do not prove a wheel is
  stopped if feedback has failed or motion is below the measurement resolution.
- Encoders are on the motor shafts, before the owner-specified 171:1 reduction.
  Once counts per motor revolution and counting convention are measured, gearbox
  output counts per revolution are that count multiplied by 171. Wheel coupling
  still needs verification. Electrical interface, channel phase and resolution
  will be determined by tests; encoder failure must stop both motors.
- Differential speed targets slow the inside wheel while retaining the requested
  speed of the outside wheel; no pivot turns. Geometry, steering-to-speed
  relationship, encoder calibration, control-loop tuning and fault handling
  remain to be established.
- Assume ideal regulated 6 V motor and 4.8 V shared Pico/servo rails for software
  development, using two buck converters from the 2S pack (8.4 V fully charged).
  The owner's "5 V" shorthand means 4.8 V, not a changed servo rail.
- Battery is the only normal operating power source and is charged externally
  to the wagon; no onboard charger is required. USB is for firmware upgrades,
  not a normal operating supply. Existing USB-only bench bring-up remains separate.
- This software assumption does not approve physical power hardware. Converter
  models/capacity, protection, disconnect, transient/braking behavior and USB
  upgrade isolation remain hardware-verification items. The stated DRV8833
  2 A maximum is not a verified continuous breakout rating.

## Confirmed operator behavior (2026-10-25)

These are approved behavior requirements, not implemented motion features:

- Use the Micro's D-input mode. B/X/Y refer to its physical labels; validate their mapping to
  Bluepad32 reports rather than assuming normalized button names match.
- Hold B for forward demand or X for reverse demand. Changing direction must
  decelerate to zero before ramping in the other direction; no polarity jump.
- The last new B/X/Y press selects the drive action. Y immediately requests coast
  on both motors, bypassing the normal deceleration ramp and closed-loop zero-speed
  correction, without an arm/disarm latch.
  A fresh B/X press may restart using the normal acceleration ramp. A button
  already held when Y was pressed does not restart motion by remaining held.
- Releasing the winning B/X button ramps drive demand to zero. It does not fall
  back to an older button still held. Repeated unchanged held reports are not
  new presses. For new presses first observed together, Y wins any tie; otherwise
  B (forward) wins over X (reverse). Last-new-press behavior still applies across reports.
- D-pad left/right steers. Normal steering movement and return to center when
  released must both be smooth and quick. Y affects drive only; steering keeps
  its normal smooth behavior. D-pad left and right together mean neutral steering,
  returning toward center using the normal smoothing.
- Initial approved bench-test command-ramp targets: 1.0 s zero-to-full drive,
  0.7 s full-to-zero on release, and 0.5 s center-to-steering-endpoint or back.
  These are provisional tuning targets, not approved actuator limits, measured
  response times, or a guarantee of physical speed/deceleration. The interpolation
  profile and reversal dwell still need definition and loaded testing.
- No operator arm/disarm mechanism is required. On connection or reconnection,
  require B/X/Y released, then a fresh B/X press before driving. This is input
  qualification, not an operator arming action.
- Controller loss is treated as releasing all buttons: normal release deceleration
  to zero and smooth steering return to center. It is not a Y-style immediate coast.
  Detection of stale/lost control still needs measured thresholds.
- Encoder failure must immediately coast both motors and require reboot for
  recovery (approved initial bench policy). Detection thresholds remain to be
  defined; this overrides normal control once powered control is implemented.
- Y requests immediate electrical coasting, not instantaneous physical stopping
  or a substitute for a physical power disconnect. Verify the breakout truth table.

## Confirmed pairing behavior (2026-10-25)

These are approved requirements implemented for inhibited bench testing;
on-board acceptance is still pending:

- If no paired controller is stored, enter pairing mode automatically.
- Remember the last successfully paired controller across power cycles and
  automatically attempt to reconnect, without requiring repeated pairing.
- If no controller connects for more than 30 seconds, enter pairing mode again.
  With a stored controller, measure the disconnected interval from startup or
  the subsequent disconnect; a successful connection ends that interval.
- Provide a wagon-mounted momentary button: hold for 2 seconds while awake to
  explicitly enter pairing; a short press wakes from software sleep.
  GP15 (physical pin 20) to GND, normally-open, internal pull-up. Firmware uses
  30 ms debounce; physical wiring and switch operation remain unverified.
- Explicit pairing while connected disconnects the current controller and opens
  pairing. Retain the previous remembered controller until a new controller
  successfully pairs; merely requesting pairing must not erase its credentials.
- Connection/reconnection must not reuse held drive controls: require B/X/Y
  released and then a fresh B/X press. No arm/disarm mechanism is introduced.
- Each pairing window lasts one minute. The previous controller may reconnect
  during that window. Successful connection ends the window; if it expires
  without connection, enter software sleep.
- Software sleep coasts both motors, stops Bluetooth and servo pulses, and
  leaves the Pico powered. A short press of the same momentary button wakes it;
  then apply the normal remembered-controller/automatic-pairing startup rules.
  This is not physical power removal or a guaranteed low-power specification.
- Initial bench implementation: first ready gamepad wins. Successful HID
  readiness is the preferred-controller persistence point. Repeated pairing
  requests do not extend an open window. Pair only with the intended Micro
  discoverable nearby; this bench admission policy is not exact-model validation.
- Software wake uses a watchdog reboot into the normal startup flow; pairing
  data survives, uptime resets, and no reinitialization of a live stack is assumed.
- The 30-second interval governs reopening pairing only. It is not a permitted
  controller-loss drive duration or a motion-stop timeout.

## Current implementation boundary

The repository builds inhibited-only bench firmware, not a powered RC controller.
Driver GPIOs are held low and UART0 emits output-only development logs.
No serial menu or command interface is required; normal operation is untethered.
USB stdio is disabled; USB is only needed for firmware upgrades.
Bluetooth pairing/reconnect, GP15 pairing/wake input, persistent preferred identity,
controller reports and preview-only smoothstep ramps are implemented.
Actual Micro physical-button mapping is unverified. Encoder raw capture is
implemented behind an interface-verification build flag, OFF by default.
Motor PWM, servo pulses, RPM conversion, speed regulator output, calibrated
differential targets and feedback-fault detection are not enabled/implemented.
This stage has no motor-enable switch; a compile-time assertion enforces inhibition.
On 2026-10-25 the owner confirmed Pico W and driver wiring
per P1, shared ground, and VM disconnected; motors, encoders, and batteries remain
disconnected. Electrical verification is still pending in the
[hardware register](hardware.md).
See [firmware status](firmware.md) for the baseline.

## Open decisions

Resolve these before implementing or testing the affected feature.

| ID | Decision | Status / consequence |
| --- | --- | --- |
| DEC-01 | Physical Pico board model and revision | Owner confirmed Pico W on 2026-10-25; board revision TBD |
| DEC-02 | Controller model, firmware, Bluetooth transport, and button/axis mapping | 8BitDo Micro in D-input mode; physical mapping and simultaneous-input precedence approved above; firmware, observed transport and actual report mapping unverified |
| DEC-03 | Remaining motor specifications and forward polarity | Owner confirms GA25-370, 6 V, 0.15 A running / 0.5 A stall per motor; 171:1 and listing 35 rpm +/-15% no-load; power-rating meaning, torque, polarity and actual load behavior TBD; driver/power suitability unverified |
| DEC-04 | Hall switch model, electrical output type/levels, channel phase, and counts per revolution | Two channels per motor on motor shaft, before 171:1 gearbox; electrical interface, phase and resolution require tests; failure coasts both motors until reboot, detection thresholds TBD |
| DEC-05 | Regulators, battery limits, protection, disconnect, and power distribution | Battery-only normal operation, external charging, two bucks for 6 V motors and 4.8 V Pico/servo approved as software assumptions; pack ASIN B0FB93HWMJ, 2S1P 7.4 V 1200 mAh; actual converters, discharge rating, protection and load validation pending |
| DEC-06 | Breakout circuit, STBY interface, and current-limit arrangement | HiLetgo ASIN B00UYIFYCW selected; vendor describes STBY low/floating = disabled, high = enabled. Actual PCB/IC verification, 3.3 V compatibility, biasing, and schematic remain TBD; no direct fault GPIO |
| DEC-07 | Steering endpoints, center, inversion, pulse timing, and loss-of-control behavior | Smooth movement/centering, provisional 0.5 s travel; controller loss and opposing D-pad directions return smoothly to center; calibration/interpolation TBD |
| DEC-08 | Motor PWM frequency, inversion, limits, acceleration/reversal policy | Provisional 1.0 s acceleration and 0.7 s release deceleration approved; reversals pass through zero; PWM, duty/speed limits, profile, reversal dwell and physical validation TBD |
| DEC-09 | Stop/fault behavior, command timeout, watchdog timing, and input qualification | No arm/disarm; Y coasts, controller loss releases buttons, encoder failure immediately coasts both until reboot; preview ramps/release qualification implemented; detection thresholds, powered behavior and running watchdog TBD |
| DEC-10 | Closed-loop differential wheel-speed control | Required per-wheel encoder feedback; slow inside wheel, outside stays at requested speed, no pivot turns; geometry/mixing law, speed estimator, gains, thresholds and feedback-fault behavior TBD |
| DEC-11 | Pairing authorization, reconnect behavior, and maximum active controllers | Bench implementation: automatic pairing/reconnect, >30 s fallback, 60 s window, GP15 2 s hold/short-press wake reboot, software sleep; first ready gamepad wins, repeated holds do not extend window; one device slot; on-board verification pending |
| DEC-12 | Exact dependency revisions and acquisition method | Resolved for initial build: approved Bluepad32 5.0.0-beta0 pinned via CMake FetchContent and SDK 2.3.1 BTstack; see dependency register; radio/controller validation pending |

## Proposed acceptance requirements

These are design proposals, not implemented guarantees. Approve the policies and
numerical thresholds before accepting powered motion:

- Start with propulsion inhibited; apply the approved B/X/Y-release then
  fresh-B/X-press rule on connection, with no arm/disarm mechanism.
- Reject malformed/out-of-range input and do not apply stale commands.
- Inhibit propulsion on disconnect, stale reports, detected faults, or watchdog
  reset. The breakout does not expose nFAULT; direct driver-fault detection is
  unavailable, and any alternative detection method remains TBD.
- Apply the approved release/fresh-press rule after reconnect. A detected encoder
  fault requires reboot; no operator arm/disarm mechanism is introduced.
- Clamp steering to mechanically verified limits and motor demand to validated limits.
- Keep a physical power-disconnect method accessible; a gamepad button is not a
  substitute for removing propulsion power.

Out of initial scope unless separately approved: autonomous navigation, a custom
phone application/protocol and Wi-Fi control. Closed-loop wheel-speed control is
required; the specific control algorithm and tuning are not yet selected.
