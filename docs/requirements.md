# Requirements

## Confirmed scope

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

## Current implementation boundary

The repository currently builds a UART demonstration, not a working RC controller.
Steering, motor drive, encoder acquisition, Bluetooth, and motion failsafes are
not implemented. Physical wiring is not confirmed; the UART reservation and
driver/encoder wiring plan are recorded in the [hardware register](hardware.md).
The firmware does not yet implement that plan.
See [firmware status](firmware.md) for the baseline.

## Open decisions

Resolve these before implementing or testing the affected feature.

| ID | Decision | Status / consequence |
| --- | --- | --- |
| DEC-01 | Physical Pico board model and revision | TBD; build defaults to Pico W |
| DEC-02 | Controller model, firmware, Bluetooth transport, and button/axis mapping | TBD; compatibility must be checked against the pinned Bluepad32 revision |
| DEC-03 | Remaining motor specifications and forward polarity | Listing B0H7MZVZ3J specifies GA25-370, 6 V, 171:1, 35 rpm +/-15% no-load, 8 W; earlier owner model was GA24-370. Resolve actual model; running/stall current, power-rating meaning, torque, and polarity TBD; driver/power suitability unverified |
| DEC-04 | Hall switch model, electrical output type/levels, channel phase, and counts per revolution | Two channels per motor confirmed; electrical interface and quadrature suitability TBD |
| DEC-05 | Regulators, battery limits, protection, disconnect, and power distribution | Pack selected: ASIN B0FB93HWMJ, owner-specified 2S1P 7.4 V 1200 mAh LiPo; converter, discharge rating, charging/protection, and rail/load design TBD; required before energized hardware testing |
| DEC-06 | Breakout circuit, STBY interface, and current-limit arrangement | HiLetgo ASIN B00UYIFYCW selected; vendor describes STBY low/floating = disabled, high = enabled. Actual PCB/IC verification, 3.3 V compatibility, biasing, and schematic remain TBD; no direct fault GPIO |
| DEC-07 | Steering endpoints, center, inversion, pulse timing, and loss-of-control behavior | TBD; calibrate to actual servo and linkage |
| DEC-08 | Motor PWM frequency, inversion, limits, acceleration/reversal policy | TBD; validate against motor/driver specifications |
| DEC-09 | Coast versus brake on stop/fault, command timeout, watchdog timing, and arming policy | TBD; required before motion is enabled |
| DEC-10 | Equal motor demand versus differential mixing, and open-loop versus encoder feedback | TBD; encoders are required, closed-loop speed control is not yet specified |
| DEC-11 | Pairing authorization, reconnect behavior, and maximum active controllers | TBD; proposed initial limit is one active controller |
| DEC-12 | Exact dependency revisions and acquisition method | TBD; no Bluetooth dependencies are currently integrated |

## Proposed acceptance requirements

These are design proposals, not implemented guarantees. Approve the policies and
numerical thresholds before accepting powered motion:

- Start with propulsion inhibited and require deliberate arming with valid,
  neutral controller input.
- Reject malformed/out-of-range input and do not apply stale commands.
- Inhibit propulsion on disconnect, stale reports, detected faults, or watchdog
  reset. The breakout does not expose nFAULT; direct driver-fault detection is
  unavailable, and any alternative detection method remains TBD.
- Do not resume motion automatically after reconnect or fault clearance.
- Clamp steering to mechanically verified limits and motor demand to validated limits.
- Keep a physical power-disconnect method accessible; a gamepad button is not a
  substitute for removing propulsion power.

Out of initial scope unless separately approved: autonomous navigation, a custom
phone application/protocol, Wi-Fi control, and a specific closed-loop control algorithm.
