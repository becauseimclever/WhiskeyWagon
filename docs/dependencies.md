# Dependencies and Bluetooth integration

## Current versus planned

Bluepad32 is acquired by CMake FetchContent at an immutable commit and linked
against the Pico SDK's BTstack. Inhibited bench firmware initializes the radio,
opens pairing or remembered-controller reconnect, and logs controller reports.
Controller compatibility has not been tested on hardware.

| Component | Role | Current status / version policy |
| --- | --- | --- |
| Raspberry Pi Pico SDK | Board support, peripherals, CYW43 and Bluetooth integration | SDK 2.3.1 selected by current extension integration; external SDK override supported |
| Arm GNU toolchain | Bare-metal C/C++ compilation | Extension selects 15_2_Rel1 |
| picotool | Firmware output processing / loading | Extension selects 2.3.1 |
| BlueKitchen BTstack | Bluetooth protocol stack beneath Bluepad32 | SDK submodule linked for Classic/BLE and CYW43; only one BTstack source tree |
| Bluepad32 | Controller discovery, pairing, HID parsing and normalized gamepad data | 5.0.0-beta0, pinned commit below; bench platform admits first ready gamepad during pairing, remembered address during reconnect |
| BlueKitchen / Pico Bluetooth examples | Isolated stack bring-up references | Reference material only; no extra runtime stack or independently validated example |

Debug/Release builds pass for the revisions below. This is not evidence of
radio readiness, controller compatibility, or safe motion.

## Acquisition and compatibility

`cmake/bluetooth.cmake` uses `FetchContent_MakeAvailable` with a full Git commit
and `SOURCE_SUBDIR src/components/bluepad32`. CMake 3.18 or newer and Git are
required. The first configure needs network access; sources are cached under
`<build>/_deps/bluepad32-src`. No moving branch is used and no Bluepad32
submodules (including its BTstack) are fetched.

For an existing local checkout at the same pinned commit, an offline configure
can use `-DFETCHCONTENT_SOURCE_DIR_BLUEPAD32="<checkout>"`. This explicit override
is the caller's responsibility: verify its commit and cleanliness.

Bluepad32 4.2.0 was tried first but failed compilation because the SDK BTstack
renamed `hids_client_*` BLE APIs to `hids_host_*`. The owner approved
**5.0.0-beta0**, which includes upstream API compatibility support. No dependency
source patches or warning suppression are used. The beta has build validation
only; controller-specific behavior must still be verified.

## Verified upstream references

Initially reviewed on 2026-10-24; pinned integration checked on 2026-10-25.
Links to moving branches are discovery references, not
reproducible dependency pins.

- [Bluepad32 repository](https://github.com/ricardoquesada/bluepad32)
- [Pinned Pico W example/configuration](https://github.com/ricardoquesada/bluepad32/tree/80d6314e46f871719fbb4b721a2cc02227c4f369/examples/pico_w)
- [Bluepad32 Pico W example and build guidance](https://github.com/ricardoquesada/bluepad32/tree/main/examples/pico_w)
- [Bluepad32 Pico W CMake integration](https://github.com/ricardoquesada/bluepad32/blob/main/examples/pico_w/CMakeLists.txt)
- [Supported controllers](https://github.com/ricardoquesada/bluepad32/blob/main/docs/supported_gamepads.md)
- [BlueKitchen BTstack](https://github.com/bluekitchen/btstack)
- [BlueKitchen profile examples](https://github.com/bluekitchen/btstack/tree/master/example)
- [Pico SDK Bluetooth examples](https://github.com/raspberrypi/pico-examples/tree/master/bluetooth)
- [Pico SDK wrappers for BTstack examples](https://github.com/raspberrypi/pico-examples/tree/master/bluetooth/btstack_examples)

Bluepad32's Pico W guide recommends SDK 2.2 or newer. This is a baseline
recommendation, not proof that every Bluepad32 and SDK revision works together.
Check the actual selected controller model and firmware: Bluetooth support does
not imply support for every controller or an arbitrary phone-control protocol.

## Stack ownership and configuration

Use **BTstack supplied by the selected Pico SDK**, following Bluepad32's
Pico W example. Do not also compile Bluepad32's bundled BTstack into the executable.
Switching to another BTstack tree requires an explicit compatibility decision;
the upstream example documents version-specific SDK changes for that alternative.

The application links `bluepad32`, whose Pico target links:

- `pico_stdlib`
- `pico_cyw43_arch_none`
- `pico_btstack_ble`
- `pico_btstack_classic`
- `pico_btstack_cyw43`
- `bluepad32`

Project-owned `src/btstack_config.h` and `src/sdkconfig.h` are propagated through
the `bluepad32` target's public include directory, including to SDK interface
sources. No global include paths or second BTstack tree are added.
The BTstack feature/buffer settings follow the pinned public-domain Pico W
example, omitting `HAVE_BTSTACK_STDIN` because development diagnostics are
output-only. Bluepad32's USB console is explicitly disabled.
One Bluepad32 device slot and one allowlist slot are configured; the example's
BTstack buffer capacities are retained pending controller validation.

The wagon's application remains C++17. `bluetooth_adapter.c` owns the C platform
callbacks, and its small `.h` interface supplies C linkage to the entry point.
The adapter uses scanning/incoming connections for lifecycle-managed pairing and
reconnect; the optional BLE management service remains disabled. No pairing keys
are deliberately deleted. A versioned application tag `0x57574331` stores the
preferred address in the SDK TLV flash bank, checked and read back at HID readiness.
The SDK owns Classic/BLE bonding keys. These survive normal reboot/UF2 updates
that do not overwrite the SDK flash-bank storage, not a full flash erase.
No Wi-Fi networking, rumble or actuator commands are added. The 10 ms control/
button timer and one-second heartbeat share the BTstack run loop.

## Selected controller

On 2026-10-25 the owner selected the
[8BitDo Micro, Amazon ASIN B0CDG5HCCH](https://www.amazon.com/dp/B0CDG5HCCH).
The [manufacturer product page](https://www.8bitdo.com/micro/) lists Switch,
D-input, and keyboard modes. The pinned Bluepad32 supported-gamepads document
lists 8BitDo family support, including Switch and Android modes, but does not
explicitly list the Micro among tested models. Exact-model compatibility is
therefore unverified, not guaranteed by family support.

Owner-selected operating mode: D-input, superseding the earlier Switch-mode
proposal. This is not a measured transport or verified report mapping.
Record the actual firmware, selected mode, advertised identity, transport, and
reports before choosing a parser/mapping policy.

The [manufacturer FAQ](https://support.8bitdo.com/faq/micro.html) documents
selectable D-pad/left-stick/right-stick reporting using Minus plus a D-pad
direction held for five seconds. Confirm the actual reporting configuration
during input tests; do not assume the physical D-pad always appears in the
`dpad` field. The controls are digital; emulated stick reports do not provide
proportional analog input. Approved physical-button behavior is recorded in
[requirements](requirements.md#confirmed-operator-behavior-2026-10-25);
actual report mapping remains unverified.

Bench firmware now enables lifecycle-managed pairing with raw input logs and
preview-only control. Candidate physical Micro masks are still unverified.
It never drives actuators; test with only the intended gamepad discoverable.

## Remaining integration sequence

1. Record the selected Micro's firmware in the approved D-input mode; verify
   the actual Classic/BLE transport and input reports during bench testing.
2. Run the firmware's inhibited Bluetooth readiness test with actuator power
   disconnected. Record the exact firmware/build and dependency revisions.
3. Verify the implemented automatic pairing/reconnect and GP15 button lifecycle
   from [requirements](requirements.md#confirmed-pairing-behavior-2026-10-25).
   Verify actual physical-button report mapping before any powered control.
4. If isolated stack diagnosis is needed, build a relevant Pico-wrapped
   BlueKitchen example separately, using the same SDK and toolchain as the wagon.
   BlueKitchen's raw examples are not standalone Pico projects.
5. Build the pinned Bluepad32 Pico W example separately; pair the selected
   controller and log connection, disconnection, and input reports.
6. Extend the already-integrated custom platform with validated input handling,
   retaining one BTstack tree and output-only UART diagnostics.
7. Validate reconnect and stale-input behavior before connecting actuator power.
8. Record resolved commits, licenses/notices, and tested controller evidence below.

## Reproducibility register

| Item | Resolved value |
| --- | --- |
| Pico SDK used for build validation | 2.3.1, `079c6f39023649b154152db30f1d781e884879bc` |
| SDK BTstack submodule commit | `eb0bb8b5ea6d234ccb940313b47f7a5c3b4e20ec` |
| SDK CYW43 driver submodule commit | `055d64274b014dd7b1c2fc94d26e8a18face7124` |
| Bluepad32 release and commit | 5.0.0-beta0, `80d6314e46f871719fbb4b721a2cc02227c4f369` |
| Bluepad32 acquisition method and repository path | CMake FetchContent, `<build>/_deps/bluepad32-src`; only its component subdirectory is built |
| Pico examples revision / selected example target | Not independently built; pinned Bluepad32 Pico W example used as integration reference |
| Controller model, firmware, transport | 8BitDo Micro, ASIN B0CDG5HCCH, D-input mode; firmware/observed transport TBD; Classic and BLE compiled; bench pairing/reconnect implemented |
| Matching stack/configuration test evidence | Debug/Release firmware builds passed; radio readiness and controller tests not run |

## Licensing

Bluepad32 identifies its license as Apache-2.0. BTstack has its own licensing
terms; the Pico W example points to Raspberry Pi's platform-specific coverage:
[LICENSE.RP](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_btstack/LICENSE.RP).
Review the actual pinned versions and applicability to the selected board before
distribution. Preserve upstream notices and review example files individually
before copying; examples are references, not a reason to relicense dependencies.
The wagon's existing [LICENSE](../LICENSE) remains unchanged.
