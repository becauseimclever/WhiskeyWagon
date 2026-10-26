# Project source of truth

These documents describe WhiskeyWagon, a Bluetooth-controlled RC wagon.
They distinguish confirmed requirements, current implementation, and proposed
design. They distinguish owner-reported wiring from electrical verification and
implemented behavior from features still planned.

## Document ownership

| Document | Authoritative for |
| --- | --- |
| [Requirements](requirements.md) | Scope, confirmed hardware, open product decisions |
| [Hardware and pin register](hardware.md) | GPIO allocation, wiring, electrical constraints, calibration |
| [Firmware design](firmware.md) | Current behavior, proposed modules, control and fault handling |
| [Dependencies](dependencies.md) | SDK, Bluepad32, BlueKitchen BTstack, example references and integration plan |
| [Bring-up and verification](verification.md) | Evidence required before wiring, enabling motion, and accepting features |

The root [README](../README.md) owns repository structure and build instructions.
The build configuration and source files describe what is implemented; these
documents must not be used to claim a feature exists before it is verified.

## Status vocabulary

- **Confirmed**: stated project requirement or verified observation.
- **Current**: implemented behavior observed in the repository.
- **Proposed**: recommendation requiring approval and verification.
- **TBD / unassigned**: deliberately unresolved; not a default or permission to guess.
- **Verified**: supported by a dated test result for identified hardware/firmware.

## Updating the record

1. Record hardware and pin changes in the hardware register before wiring.
2. Record design decisions in the owning document with date and rationale.
3. Update affected firmware, build configuration, and verification steps together.
4. Record actual results, including failures; do not turn a proposed value into
   a confirmed value without evidence.
5. Link to the owning table instead of maintaining duplicate pin or calibration lists.

## Initial decision record

| Date | Decision | Status / rationale |
| --- | --- | --- |
| 2026-10-24 | SG92R steering servo, two drive motors, DRV8833, Hall-effect motor encoders | Confirmed project requirements |
| 2026-10-24 | Bluetooth controller support through Bluepad32, with BlueKitchen examples as integration references | Confirmed direction; controller model TBD |
| 2026-10-24 | Initially no wiring or GPIO assignments existed | Initial baseline; superseded for allocation by plan P1 below, physical wiring still unconfirmed |
| 2026-10-24 | Document dependencies without adding them to the build | Confirmed scope for this documentation change |
| 2026-10-24 | Use Pico W as the initial board baseline | Current CMake default; physical board revision still to be recorded |
| 2026-10-24 | Reserve GP0/GP1 for UART0 TX/RX; DRV8833 A = left wheel, B = right wheel | Confirmed project-owner direction |
| 2026-10-24 | Each motor encoder has two Hall-effect switches and two channels | Confirmed; output levels/type and quadrature phase remain TBD |
| 2026-10-24 | Allocate driver and encoder wiring plan P1 in the hardware register | Documentation only; physical wiring/electrical validation and firmware implementation pending |
| 2026-10-24 | Breakout exposes STBY and VM, not nSLEEP/nFAULT; VM is the motor supply | Confirmed by project owner; GP14 conditionally reserved for STBY, GP15 released; STBY electrical behavior remains unverified |
| 2026-10-24 | Selected breakout is HiLetgo DRV8833, Amazon ASIN B00UYIFYCW | Owner-supplied listing identifies STBY low/floating = disabled, high = enabled; actual board and 3.3 V interface validation pending; see hardware register for source caveats |
| 2026-10-24 | Both drive motors are 6 V GA24-370 units with 171:1 reduction and 35 rpm gearbox output | Owner-specified; load conditions, current ratings, and encoder shaft location remain unverified |
| 2026-10-24 | Battery selected: Amazon ASIN B0FB93HWMJ, 2S1P 7.4 V nominal 1200 mAh LiPo | Specifications supplied by owner; buck converter and power/protection design remain TBD |
| 2026-10-24 | Motor reference supplied: Amazon ASIN B0H7MZVZ3J | Listing specifies GA25-370 (earlier owner identification GA24-370), 35 rpm +/-15% no-load, 8 W; model discrepancy and current ratings remain unresolved |
| 2026-10-24 | Steering servo confirmed as standard TowerPro SG92R micro servo | Manufacturer specifies 4.8 V operation; separate 4.8 V servo rail proposed, current and steering calibration pending |
| 2026-10-24 | Allocate steering signal to GP2, physical pin 4, PWM slice 1A | Documentation-only allocation; avoids UARTs and motor PWM slices; servo supply, signal compatibility, and timing validation pending |
| 2026-10-24 | Servo factory leads confirmed: brown = ground, yellow = signal, orange (or red) = positive supply | Owner-confirmed; brown connects to black project ground harness; not attributed to TowerPro's published color documentation |
| 2026-10-24 | Latest power proposal: 6 V motor rail plus shared 4.8 V servo/Pico rail | Separate wiring branches, Pico USB isolation, current sizing and transient validation required; no converters selected and battery-direct motor operation not approved |
| 2026-10-25 | Owner confirms Pico W; driver wired per P1, shared ground, VM disconnected; motors, encoders, batteries disconnected | Owner-reported, not electrically measured; board revision and STBY interface remain unverified |
| 2026-10-25 | Implement inhibited-only driver GPIO and USB/UART diagnostics baseline | Approved scope; no arm/PWM/motion commands, Bluetooth deferred, servo and encoder pins left untouched |
| 2026-10-25 | Replace the interactive baseline with output-only UART0 development logs | Owner clarification: no serial menu or runtime PC connection; command parser/tests removed, USB stdio disabled, BOOTSEL upgrades retained |
| 2026-10-25 | Set up Bluepad32 and SDK BTstack, initialize radio with discovery/pairing disabled | Owner-approved bring-up scope; no controller selected, driver inhibition unchanged; heartbeat moved to BTstack timer |
| 2026-10-25 | Pin Bluepad32 5.0.0-beta0 rather than 4.2.0 | Owner approved beta after 4.2.0 failed against renamed BLE APIs in SDK 2.3.1's BTstack; immutable commit and build evidence in dependency register |
| 2026-10-25 | Select 8BitDo Micro Bluetooth gamepad, Amazon ASIN B0CDG5HCCH | Owner-confirmed selection; exact-model compatibility, firmware, mode, transport and mapping unverified; pairing remains disabled |
| 2026-10-25 | Physical B forward, X reverse, Y immediate drive-stop; D-pad steering; smooth normal movement and centering | Owner-approved behavior; last new B/X/Y press wins, no user arm/disarm or Y latch; old held buttons do not resume; reversal passes through zero |
| 2026-10-25 | Provisional command ramps: 1.0 s acceleration, 0.7 s release deceleration, 0.5 s center/endpoint steering travel | Owner-approved bench starting targets, not measured mechanical response or permission to power actuators; avoid jerks with glass bottles aboard |
| 2026-10-25 | Pair automatically if none stored, reconnect to last paired controller, reopen pairing after more than 30 s disconnected; add wagon-mounted pairing button | Owner-approved policy; explicit pairing disconnects current controller but retains old credentials until successful replacement; wiring and pairing-window details TBD |
| 2026-10-25 | No arm/disarm mechanism; require B/X/Y released then fresh B/X after connection/reconnection | Owner-approved input qualification, not arming; pairing timeout is not a drive-stop timeout; behavior not yet implemented |
| 2026-10-25 | Require per-wheel closed-loop speed control and differential turns | Owner-approved: slow inside wheel, outside remains at requested speed, no pivot turns; encoder/geometry calibration and control tuning pending |
| 2026-10-25 | Confirm GA25-370, 6 V motors, 0.15 A running / 0.5 A stall each; owner quotes DRV8833 2 A maximum | Owner-supplied specifications, not measured wagon currents or verified continuous breakout capability |
| 2026-10-25 | Assume ideal 6 V and 4.8 V rails from two bucks for software; battery-only normal operation, external charging | Owner clarified "5 V" means 4.8 V; physical converter/protection/current validation and USB upgrade isolation remain pending |
| 2026-10-25 | Controller loss acts as all-button release; Y coasts immediately; encoder failure stops both motors | Owner-approved; fault thresholds, encoder-fault electrical stop and recovery remain to be defined |
| 2026-10-25 | Pair/wake momentary button: 2 s hold pairs; short press wakes; one-minute pairing window permits previous-controller reconnect | On window expiry enter software sleep: coast, stop Bluetooth/servo pulses, keep Pico powered; GPIO/debounce/wake backend pending |
| 2026-10-25 | Micro D-input mode; simultaneous fresh drive presses resolve Y > B > X; opposing D-pad directions center smoothly | Owner-confirmed tie interpretation; normal last-new-press behavior unchanged |
| 2026-10-25 | Encoders are on motor shafts before 171:1 gearboxes | Owner-confirmed location; electrical interface, phase and counts per revolution require tests |
| 2026-10-25 | Implement inhibited bench firmware; allocate GP15 button to GND with pull-up | Owner confirms motors/encoders/battery still disconnected; Bluetooth pairing, reconnect, input previews and optional raw encoder diagnostics approved; no motor/servo output |
| 2026-10-25 | Initial encoder-fault policy: immediately coast both motors, require reboot | Owner-approved; detection thresholds and powered regulator are not enabled in the bench build |
| 2026-10-25 | Bench pairing admission/persistence and software wake | First ready gamepad wins, HID readiness persists preferred identity, repeated holds do not extend window; short-press wake reboots while retaining SDK flash storage |
