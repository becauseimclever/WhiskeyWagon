# Hardware and pin register

## Wiring status

**Owner update, 2026-11-01:** The SG92R is now connected, with a reported
regulated 4.8 V servo supply and the linkage detached. Motor and servo power
were off for flashing. The owner considers the 3.3 V signal likely to work,
but signal compatibility, actual voltage and PWM timing remain unverified.
During one explicitly approved unloaded 1500 us burst, the owner observed the
output shaft rotate and switched servo power off. The owner later fitted an
unloaded horn; a second 1500 us burst left it stationary and perpendicular to
the servo body. A subsequent 1550 us burst produced a slight counterclockwise
movement viewed from the horn face. A later 1450 us burst moved the horn
clockwise to nearly perpendicular. At 1400 us, the horn was slightly off
perpendicular and the movement direction was uncertain. A widened sweep then
reached 544 and 2400 us (Arduino Servo defaults) quietly, about 180 degrees
total, with lower pulses clockwise. These are the tested range limits, not
confirmed mechanical end stops; the working center is the 1472 us midpoint. The
owner reported servo power on and motor power off afterwards. Wagon steering
mapping and linkage travel remain to be set after installation.

**Owner update, 2026-10-31:** Both GA25-370 drive motors have 171:1 reduction
gearboxes and are rated at 35 RPM on the output shaft. Steering is an SG92R.
The nominal implied motor-shaft speed is 35 * 171 = 5985 RPM; this is a rating
calculation, not a measured speed or encoder calibration. Encoder output-shaft
counts/revolution still require measured motor-shaft counts/revolution * 171.
The latest simultaneous full-duty pulse passed after left harness repair.
Those tests used external STBY enable with GP14 disconnected; controlled GP14
enable and left Hall capture remain unverified. The integrated POC defaults to
inhibited outputs and does not supersede electrical acceptance.

**Owner update, 2026-10-30:** Right motor is now wired and powered, with both
right Hall channels connected. Owner confirmed GP8/GP9, GP14 and GP12/GP13
assignments, reported 3.3 V interfaces, and confirmed a secured/lifted wheel with
disconnect access. Left motor/encoder wiring was not reconfirmed. These reports
supersede the disconnected-right-motor status below, not the outstanding
measurement/calibration requirements. The optional
[right-motor test](verification.md#right-motor-usb-bench-test) can drive GP8/GP9
and enable GP14; the normal image still holds all motor outputs low.

**Owner-reported wiring, 2026-10-25:** Pico W; GP6/GP7 to AIN1/AIN2,
GP8/GP9 to BIN1/BIN2, GP14 to STBY, shared ground, VM disconnected.
Motors, encoders, and batteries remain disconnected. Board revision, electrical
levels, STBY compatibility, and reset biasing have not been measured.
GP0/GP1 are reserved for UART0. The inhibited-only firmware implements the driver
pin assignments as constant-low outputs; it does not configure servo or encoder
pins. Keep VM disconnected until the electrical and power gates are resolved.

Use RP2040 GPIO numbers (`GPn`) and physical Pico header pin numbers in separate
columns. Record connector position, wire label, and electrical interface when
assigning a signal. Never substitute a header number for a GPIO number.

## Hardware inventory

| Component | Quantity | Confirmed | Still required |
| --- | --- | --- | --- |
| Pico-family wireless controller | 1 | Owner-confirmed Pico W, 2026-10-25 | Physical revision |
| 8BitDo Micro Bluetooth gamepad | 1 | Owner-selected 2026-10-25, Amazon ASIN B0CDG5HCCH | Firmware version, selected operating mode, observed Bluetooth transport, Bluepad32 compatibility and button mapping |
| TowerPro SG92R micro steering servo | 1 | Owner-confirmed model; manufacturer specifies 4.8 V operation | Peak/stall current, signal compatibility, pulse range, linkage limits |
| Geared DC motor with Hall encoder | 2 | Owner confirms GA25-370, 6 V, 0.15 A running / 0.5 A stall each (2026-10-25); 171:1; listing B0H7MZVZ3J: 35 rpm +/-15% no-load, 8 W; A = left, B = right | Torque, forward polarity, current under actual load and encoder specifications |
| HiLetgo DRV8833 dual H-bridge breakout | 1 from a 3-pack | Owner-specified product: Amazon ASIN B00UYIFYCW; STBY and VM exposed, nSLEEP/nFAULT not exposed | Actual PCB revision/IC marking, schematic, 3.3 V STBY compatibility, thermal limits, current-limit configuration |
| Motor Hall encoders | 2 sensors per motor, 4 total | Hall-effect switches; two signal channels per motor | Model, supply/output voltage, electrical output type, phase relationship, counts |
| LiPo battery pack | 1 | Owner-selected Amazon ASIN B0FB93HWMJ; owner-specified 2S1P, 7.4 V nominal, 1200 mAh | Discharge rating, connector/polarity, protection, charger, actual full-charge/cutoff specifications |
| Buck converters, protection, disconnect | 2 converters planned | Approved rail targets: 6 V motors, 4.8 V shared Pico/servo; ideal rails assumed for software | Actual converter selection, load budget, thermal validation, low-voltage protection, fuse/disconnect |

## Authoritative GPIO allocation

The UART reservation and motor/channel identity are confirmed requirements.
The selected driver, encoder, and servo GPIOs form wiring plan **P1**; they are not
electrically verified wiring. Driver and servo wiring are owner-reported as above;
unconfirmed encoder connections remain plans. GP14 was previously reported wired
to STBY but was disconnected for the latest externally enabled motor tests.
Controlled enable remains blocked on electrical verification.
nSLEEP and nFAULT are not exposed; their
previous reservations are superseded. GP15 is now allocated to the approved
pairing/wake momentary button, not a driver fault signal.
`TBD` means no pin is reserved.

| Feature / signal | Direction at Pico | GPIO | Header pin | Interface / allocation constraint |
| --- | --- | --- | --- | --- |
| Steering servo signal | Output | GP2 | 4 | PWM slice 1, channel A; dedicated servo timing, separate from motor slices 3 and 4 |
| Left motor / DRV8833 AIN1 | Output | GP6 | 9 | PWM slice 3, channel A |
| Left motor / DRV8833 AIN2 | Output | GP7 | 10 | PWM slice 3, channel B |
| Right motor / DRV8833 BIN1 | Output | GP8 | 11 | PWM slice 4, channel A |
| Right motor / DRV8833 BIN2 | Output | GP9 | 12 | PWM slice 4, channel B |
| Breakout STBY | Output, low only | GP14 | 19 | Previously reported connected; disconnected for latest motor tests; compatibility and reset biasing unverified |
| Left encoder channel A (sensor 1) | Input | GP10 | 14 | Hall switch output; electrical interface TBD |
| Left encoder channel B (sensor 2) | Input | GP11 | 15 | Hall switch output; verify phase relationship |
| Right encoder channel A (sensor 1) | Input | GP12 | 16 | Hall switch output; electrical interface TBD |
| Right encoder channel B (sensor 2) | Input | GP13 | 17 | Hall switch output; verify phase relationship |
| Diagnostic UART0 TX | Output | GP0 | 1 | Reserved; 3.3 V logic UART, not RS-232 |
| Diagnostic UART0 RX | Input | GP1 | 2 | Reserved; 3.3 V logic UART, not RS-232 |
| Pairing / wake button | Input | GP15 | 20 | Approved normally-open momentary to GND, internal pull-up, 30 ms debounce; 2 s hold pairs, short press/release wakes by reboot; physical wiring unverified |

Left/right are defined looking forward in the wagon's normal direction of travel.
DRV8833 channel A drives the left wheel; channel B drives the right wheel.
Encoder channel A/B identifies sensors on a motor, not DRV8833 bridge identity.
Bluetooth uses the Pico W's on-board wireless hardware, not an external UART
Bluetooth module; the radio needs no user header GPIO. The required pairing
button is a separate user input on GP15.

### Bench encoder capture gate

Encoder interface remains unidentified. Default firmware leaves GP10-GP13
unconfigured; do not connect unknown-voltage sensor outputs.
Only after verifying a 3.3 V-safe interface and any required external pull-ups,
enable `WHISKEY_ENCODER_CAPTURE_VERIFIED` in a separate build. Capture enables
both-edge GPIO IRQs with internal pulls disabled. Logs provide raw A/B edge
counts and tentative x4 transitions without asserting quadrature or RPM.
Motor inputs and STBY remain low in both variants.

### Breakout control and supply pins

| Breakout label | Recorded function | Wiring status |
| --- | --- | --- |
| VM | Motor voltage supply input | Connect only to the approved motor supply; voltage/current design remains TBD |
| STBY | Vendor-described enable: low/floating disables outputs, high enables | Owner reports GP14 connected; baseline drives low; 3.3 V compatibility and board biasing unverified |
| nSLEEP | Not exposed | No direct connection planned |
| nFAULT | Not exposed | No fault GPIO available; GP15 released |

VM is not a GPIO signal and must not be connected to a Pico GPIO or the blue
encoder 3.3 V wire. Its wire color is not yet assigned. The motor supply return
uses black ground wiring under the common-ground/return-path constraints below.

### Identified breakout and source quality

Owner-selected product: [HiLetgo 3pcs DRV8833 Dual Motor Driver, compatible with
TB6612, Amazon ASIN B00UYIFYCW](https://www.amazon.com/dp/B00UYIFYCW).
The listing was reviewed on 2026-10-24; its information is vendor-described,
not a schematic or a measurement of the actual board.

- The listing describes STBY low or floating as disabled/no output and high as
  enabled. It mentions 5 V for enable but does not give a minimum high threshold
  proving that the breakout works with a 3.3 V GPIO.
- It identifies VM as the chip/motor supply and specifies 2.7-10.8 V. Elsewhere
  it says "within 12V"; do not use that inconsistent wording to authorize 12 V.
  Retain the DRV8833 recommended 2.7-10.8 V range and any stricter component limits.
- It mentions PWMB despite describing input-pair control, and misspells some
  input labels. Do not allocate an extra PWM pin or copy a TB6612 wiring diagram
  based on that text. Confirm labels and connections on the actual PCB.

Record the actual PCB revision and readable IC marking and obtain a schematic
if available. GP14 is now owner-reported as wired; verify 3.3 V STBY compatibility
and any on-board pull-ups before powered testing. Never apply 5 V to a Pico GPIO.
The intended future policy is to hold
STBY low while disarmed and enable it only after initialization and deliberate
arming; reset behavior must be tested before adopting this policy.
No direct hardware-fault reporting is available through this breakout; software
must not claim to observe nFAULT.

## Wire color standard

Apply these colors consistently to both motor/encoder harnesses. Black is reserved
exclusively for ground: never use black for a signal or either motor winding lead.
Label each harness LEFT or RIGHT; color alone does not identify the wheel.

| Color | Assigned function | Connection / label |
| --- | --- | --- |
| Black | Ground only | Encoder ground and other ground wiring; route motor-current returns separately |
| Red | Motor positive lead (nominal M+) | Left: AOUT1; right: BOUT1 |
| White | Motor negative lead (nominal M-) | Left: AOUT2; right: BOUT2; never ground |
| Blue | Encoder 3.3 V supply | Both encoder assemblies' VCC; supply source and current budget still to be verified |
| Green | Encoder channel A (sensor 1) | `LEFT_ENC_A` / `RIGHT_ENC_A`; GPIOs in the allocation table |
| Yellow | Encoder channel B (sensor 2) | `LEFT_ENC_B` / `RIGHT_ENC_B`; GPIOs in the allocation table |

Red/white are motor lead identifiers, not fixed rail voltages: the H-bridge
reverses polarity during reverse drive. They do not establish which rotation
is wagon-forward; verify direction and record inversion for each wheel.
Do not swap in a black wire when a motor reverses.

Blue designates the planned 3.3 V encoder supply, not approval to energize an
unidentified sensor. Verify the Hall switches support 3.3 V operation and determine
their output type and pull-up requirements first. Do not connect blue to motor
power or a 5 V rail. Connector positions and other signal-wire colors remain TBD;
do not repurpose the assigned harness colors for unrelated functions.

### Driver-to-motor connections

Connect only with power removed, after confirming the actual breakout labels.
These are logical terminal connections, not a power-supply approval.

| DRV8833 terminal pair | Destination | Connection rule |
| --- | --- | --- |
| AOUT1 / AOUT2 (sometimes AO1 / AO2) | Left motor's two winding leads | AOUT1 to red M+; AOUT2 to white M-; forward polarity TBD |
| BOUT1 / BOUT2 (sometimes BO1 / BO2) | Right motor's two winding leads | BOUT1 to red M+; BOUT2 to white M-; forward polarity TBD |

Do not connect either winding lead directly to ground or a supply rail: each
motor is connected across its H-bridge output pair. Do not parallel the bridges.
Route encoder power, ground, and sensor outputs separately from motor winding
leads; encoder outputs never connect to H-bridge outputs.

Label signal wires `LEFT_ENC_A`, `LEFT_ENC_B`, `RIGHT_ENC_A`, and `RIGHT_ENC_B`.
Use the wire color standard above; connector positions remain TBD. Two Hall switches provide
two channels, but do not prove quadrature timing; verify phase and direction
before enabling a quadrature decoder. Confirm whether outputs are open-collector,
open-drain, or push-pull. Specify sensor power and any 3.3 V pull-ups or level
translation from the actual part data before connecting outputs to the Pico.
Do not assume these electronic switches behave like passive dry contacts.

### Diagnostic UART usage

The [current source](../src/main.cpp) initializes UART output only:

| Diagnostic function | GPIO | Pico W header pin | Status |
| --- | --- | --- | --- |
| UART0 stdio TX | GP0 | 1 | Output-only development logs, 115200 baud, 8N1, 3.3 V logic |
| UART0 RX reservation | GP1 | 2 | Reserved, not initialized; no command input |
| UART1 TX / RX | GP4 / GP5 | 6 / 7 | Not initialized |

Diagnostic adapter wiring is not confirmed. Connect Pico GP0 to a 3.3 V adapter's
RX and share ground; leave adapter TX and supply pins disconnected.
UART0 agrees with the reservation above. GP4/GP5 are unused by this baseline.
Steering uses GP2 on PWM slice 1, without conflicting
with either UART or motor slices 3 and 4.

## Allocation rules

- Use only exposed GPIOs on the confirmed board. Keep SWD accessible and account
  for board-internal wireless connections. Consult the official board pinout.
- RP2040 PWM channels share a frequency within a slice; GPIO mappings also repeat.
  Record slice/channel ownership when choosing PWM pins. Do not put the servo
  and motor PWM on the same slice unless their timing requirements permit it.
- GPIO interrupt decoding and PIO encoder decoding are alternatives, not decisions.
  Select the method after calculating the maximum encoder edge rate.
- Verify the two Hall channels' phase relationship before deriving direction;
  commanded motor direction is not proof of actual wheel direction.
- Confirm all inputs are compatible with Pico 3.3 V GPIO limits. Do not connect
  unverified encoder outputs or pull-ups to GPIO.

## Steering servo specifications

### Steering connections

| Servo function | Planned connection | Wire / status |
| --- | --- | --- |
| Control signal | GP2, Pico W physical pin 4, PWM slice 1A | Yellow factory lead; label `STEERING_PWM` |
| Supply positive | Proposed regulated 4.8 V servo rail | Orange (or red) factory lead; regulator TBD; never connect to GP2 or Pico 3V3 |
| Ground | Common signal ground with a separate servo-current return to the supply | Brown factory lead to black project ground harness; Pico ground connection point TBD |

The owner confirmed these factory lead functions on 2026-10-24. The reported
connector wire order is brown, orange, yellow, with supply in the center.
This confirmation is owner-supplied, not a wire-color specification published
on TowerPro's product page.

Reserve PWM slice 1 timing for steering. GP3 shares that slice, and GP18/GP19
alias its channels; do not allocate them to incompatible PWM timing or duplicate
servo-channel output. PWM frequency, pulse limits, and 3.3 V signal
compatibility still require verification before routine use. The owner reports
the servo wiring connected and approved one unloaded exploratory burst on the
unverified interface; that observation does not establish electrical acceptance.

### Manufacturer specifications

The owner identifies the steering servo as a standard **TowerPro SG92R micro
servo**. The [manufacturer product page](https://towerpro.com.tw/product/sg92r-7/),
reviewed 2026-10-24, specifies:

| Parameter | Manufacturer value |
| --- | --- |
| Type / mass | Digital servo / 9 g |
| Operating voltage | 4.8 V; no wider operating range stated on the page |
| Stall torque | 2.5 kgf cm at 4.8 V |
| Speed | 0.1 s per 60 degrees at 4.8 V |
| Dimensions | 23 x 12.2 x 27 mm |
| Gears | POM with carbon fiber |

Stall torque is not a current rating. The page does not establish peak/stall
current, accepted signal-high voltage, frame frequency, or safe pulse endpoints.
Those remain to be verified for the actual unit and steering linkage.
Do not assume 6 V operation or a full 180-degree mechanical range.
The project harness rule that black is ground still applies: the servo's
factory brown ground lead connects to black ground wiring. Factory servo colors
are component-specific; yellow here is steering signal, not an encoder channel.
Use the confirmed steering connection table rather than inferring servo functions
from the motor/encoder wire color standard.

### Servo timing references

[Adafruit's SG92R guide](https://www.adafruit.com/product/169) recommends the
Arduino Servo library and describes approximately 1000, 1500 and 2000 us as
nominal end, middle and opposite-end commands. Those values are useful starting
references, not measured limits for this servo or its steering linkage.
Adafruit's example identifies an orange control lead; the owner reports a
yellow control lead on this unit. Verify the actual connector rather than
transferring colors between examples.

The [Arduino Servo 1.3.0 common header at revision
`440b319`](https://github.com/arduino-libraries/Servo/blob/440b319ec11ea2664df8ce201c271c935de28a2d/src/Servo.h)
defines a 1500 us default pulse and a 20000 us minimum refresh interval.
The [generic attach API](https://github.com/arduino-libraries/Servo/blob/440b319ec11ea2664df8ce201c271c935de28a2d/docs/api.md#attach)
describes configurable limits with 544 and 2400 us defaults. These are library
defaults. On this unloaded SG92R, 544-2400 us produced about 180 degrees of
travel with no stall at either end (see the
[endpoint sweep](verification.md#unloaded-endpoint-sweep-2026-11-01));
its 1472 us midpoint is the working center. Pulses beyond that range were not
tested, so mechanical end stops are unconfirmed. Linkage limits are still required.

Our Pico SDK test independently uses a nominal 20000 us frame and accepts
544-2400 us for a one-second, manually armed burst. Unlike an attached Arduino
Servo instance, it does not continuously refresh the servo after the burst.
The Arduino library is a timing reference only, not a dependency or copied code.
The servo responds reliably to the Pico's 3.3 V signal in these bench tests;
the waveform itself has not been measured.

## Power and wiring constraints

The software power model assumes regulated 6 V and 4.8 V rails from two buck
converters. Physical implementation and approval remain pending. Battery is the
only normal operating source and charging is external to the wagon. USB is for
firmware updates (and the separate existing USB-only bench procedure), not normal
operation. Do not power motors or the servo from Pico GPIO or assume the Pico 3V3
rail can supply them.

### Selected motor listing

Owner-supplied reference: [motor with Hall encoder, Amazon ASIN
B0H7MZVZ3J](https://www.amazon.com/dp/B0H7MZVZ3J), reviewed 2026-10-24.

| Specification | Listing value / qualification |
| --- | --- |
| Model | GA25-370; owner confirmed 2026-10-25, superseding earlier GA24-370 identification |
| Rated voltage | DC 6 V |
| Gear reduction | 171:1 |
| No-load gearbox output speed | 35 rpm +/-15% (29.75-40.25 rpm); not a loaded speed guarantee |
| Power | 8 W advertised; electrical input versus mechanical output and operating conditions unspecified |
| Output shaft | 4 mm diameter, 12 mm length |
| Motor diameter / total length | 25 mm / 74.8 mm |
| Running and stall current | Owner-supplied 2026-10-25: 0.15 A running / 0.5 A stall per motor; not measured wagon values |
| Encoder supply, output interface, resolution | Not provided; tests required |
| Encoder shaft location | Owner-confirmed motor shaft, before 171:1 gearbox (2026-10-25) |

Do not interpret 8 W / 6 V (about 1.33 A) as a specified running or stall
current. That calculation assumes electrical input power at a defined operating
point, which the listing does not establish, and does not cover startup/stall
transients. Converter sizing and DRV8833 suitability remain unresolved.

Using the owner-supplied motor ratings, both motors total 0.30 A running or
1.0 A stalled at their rated voltage; these arithmetic totals are not converter
ratings or permission to stall the motors. Validate actual current, transient
margin, current limiting and breakout cooling before powered acceptance.
The owner quotes a 2 A DRV8833 maximum; a maximum/peak specification does not
establish continuous output capability for the actual breakout.

### Battery and regulator selection

Selected pack: [Amazon ASIN B0FB93HWMJ](https://www.amazon.com/dp/B0FB93HWMJ).
The owner identifies it as a **2S1P, 7.4 V nominal, 1200 mAh LiPo**.
The listing's specifications could not be retrieved; these values are
owner-supplied, not independently verified supplier data.

A standard 4.2 V-per-cell 2S LiPo reaches **8.4 V fully charged**; verify the
actual pack/charger specification. Nominal voltage is not the converter's maximum
input requirement. Capacity in mAh does not establish continuous or peak current
capability; discharge rating and protection behavior remain TBD.

Two buck converters are the approved software power assumption, with regulated
6 V and 4.8 V outputs. No converter model is selected yet. The following are
physical selection requirements, not an approved power schematic:

| Rail / load | Intended supply | Requirements still to resolve |
| --- | --- | --- |
| DRV8833 VM and both 6 V motors | Proposed regulated 6 V buck output | Combined running/startup/stall current, overload policy, driver limits, ripple, cooling, braking energy handling |
| TowerPro SG92R servo | Proposed shared 4.8 V buck with Pico, separate servo wiring branch | Converter tolerance/ripple, combined peak current, signal compatibility; do not use the 6 V motor rail |
| Pico W VSYS (physical pin 39) | Same proposed 4.8 V buck, via its own isolated supply branch | Stay within VSYS 1.8-5.5 V, validate servo/wireless transients, implement USB/external supply isolation |
| Encoders | Planned 3.3 V rail | Verify sensor compatibility and available current; never use VM as encoder power |

The rail targets are 6 V for propulsion and 4.8 V shared by servo and Pico
("5 V" is owner shorthand for 4.8 V). Branch the servo and Pico wiring at the supply, not through the
Pico board; keep servo-current returns out of the Pico ground wiring.
Provide suitable decoupling and validate rail stability under steering load.
Use the Pico datasheet's external supply isolation arrangement (for example,
a Schottky diode in the Pico VSYS branch) so USB cannot feed the servo rail or
backfeed the converter. Do not connect this supply to VBUS or 3V3.
A separate logic converter remains an option if transient testing requires it.

- Do not connect the raw 2S pack directly to the 6 V motors, servo, Pico VSYS,
  or encoder supply. Even the pack's nominal 7.4 V exceeds the motor rating and
  the Pico VSYS maximum.
- Short-duration battery-direct motor operation is not approved: a fully charged
  standard 2S pack is 40% above the 6 V rating, and allowable duration/current
  limits are unknown. A PWM duty cap alone does not establish safe current or
  replace verified current limiting and motor specifications.
- Select a converter input range covering full charge and the intended discharge
  range, with appropriate transient margin. A buck needs input headroom; if
  battery voltage approaches 6 V plus its dropout requirement, a 6 V rail will
  lose regulation. Set the battery cutoff and accepted end-of-discharge behavior,
  or evaluate buck-boost if regulated 6 V over a wider range is required.
- Size continuous and transient output current from both motors' actual current
  data, then validate at the real input voltage and cooling conditions. Do not
  choose by advertised peak amperage or battery capacity alone.
- Verify how the supply handles motor braking/regeneration; do not assume a buck
  can absorb returned energy or prevent motor-rail overvoltage.
- Specify a suitable 2S LiPo balance charger, per-cell undervoltage protection or
  monitoring, short-circuit protection, fuse, and accessible disconnect. Do not
  assume the pack includes protection; converter undervoltage lockout alone may
  not protect each cell.

- Verify battery voltage, regulator capacity, wiring, and protection against
  combined motor startup/stall load and servo load using actual specifications.
- The selected motors are specified as 6 V units. The driver's
  wider VM operating range does not authorize applying that range to the motors.
  Running/stall ratings are now owner-supplied, but actual load/transient current
  and breakout thermal capacity remain unverified, so motor/driver current
  compatibility and the physical power design are not yet approved.
- The DRV8833 IC specifies a 2.7-10.8 V operating motor supply range; the selected
  breakout, motors, and thermal environment may impose stricter limits. Its
  headline peak current is not a guaranteed continuous breakout rating.
- Use the manufacturer's 4.8 V servo specification for regulator selection;
  verify actual unit compatibility, current, and signal requirements before
  approving a power rail or pulse range.
- Establish a common signal ground with a deliberate return path; keep motor
  current out of Pico/encoder signal returns. Specify decoupling, noise suppression,
  connector polarity, and USB/external-power interaction before wiring.
- Determine hardware biasing so reset, boot, and an unpowered Pico cannot
  unintentionally command a motor. Inspect STBY and motor-input defaults on the
  breakout; do not assume that an unconnected STBY holds the motors disabled.

## Calibration register

| Parameter | Value / status |
| --- | --- |
| Servo frequency, minimum/center/maximum pulse, inversion | TBD; actual unit and linkage validation required |
| Motor PWM frequency and maximum duty | TBD |
| Motor A/B wheel identity | Confirmed: A = left, B = right |
| Motor model and nominal voltage | Owner-confirmed GA25-370, 6 V |
| Motor current ratings | Owner-supplied 0.15 A running / 0.5 A stall each; actual load/transient validation pending |
| Gear reduction | Owner-specified: 171:1 (motor shaft to gearbox output) |
| Gearbox output speed | Listing: 35 rpm +/-15% no-load; not a measured wagon speed or control limit |
| Motor forward polarity and inversion | TBD per motor |
| Encoder channel count | Confirmed: two Hall switch channels per motor |
| Encoder output interface, pull-ups, and phase relationship | TBD per motor |
| Encoder counts per motor-shaft revolution and counting convention | TBD; distinguish pulses, edges, and quadrature x1/x2/x4 |
| Encoder shaft location | Owner-confirmed motor shaft, before 171:1 gearbox |
| Wheel circumference, output-to-wheel coupling, counts per wheel revolution | Coupling/circumference TBD; gearbox output counts per revolution = measured counts per motor revolution x 171, using the same counting convention |
| Measured motor and servo current; approved power limits | TBD |

## References

- [Pico W datasheet and pinout](https://datasheets.raspberrypi.com/picow/pico-w-datasheet.pdf)
- [RP2040 datasheet: GPIO, PWM, PIO](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [TI DRV8833 product page and datasheet](https://www.ti.com/product/DRV8833)
- [Selected HiLetgo breakout listing, ASIN B00UYIFYCW](https://www.amazon.com/dp/B00UYIFYCW)
- [Selected battery pack listing, ASIN B0FB93HWMJ](https://www.amazon.com/dp/B0FB93HWMJ) (specifications supplied by owner)
- [Selected motor listing, ASIN B0H7MZVZ3J](https://www.amazon.com/dp/B0H7MZVZ3J)
- [TowerPro SG92R manufacturer specifications](https://towerpro.com.tw/product/sg92r-7/)
- Detailed servo electrical/timing data, motor/encoder datasheets and breakout schematic: TBD for the actual parts.
