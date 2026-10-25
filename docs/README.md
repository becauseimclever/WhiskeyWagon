# Project source of truth

These documents describe WhiskeyWagon, a Bluetooth-controlled RC wagon.
They distinguish confirmed requirements, current implementation, and proposed
design. They do not imply that hardware has been wired or firmware implemented.

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
