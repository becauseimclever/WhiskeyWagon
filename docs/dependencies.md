# Dependencies and Bluetooth integration

## Current versus planned

This is an integration record, **not an installation**. No Bluepad32 submodule,
download step, or Bluetooth link dependency has been added to this repository.

| Component | Role | Current status / version policy |
| --- | --- | --- |
| Raspberry Pi Pico SDK | Board support, peripherals, CYW43 and Bluetooth integration | SDK 2.3.1 selected by current extension integration; external SDK override supported |
| Arm GNU toolchain | Bare-metal C/C++ compilation | Extension selects 15_2_Rel1 |
| picotool | Firmware output processing / loading | Extension selects 2.3.1 |
| BlueKitchen BTstack | Bluetooth protocol stack beneath Bluepad32 | SDK includes a BTstack submodule; not linked by current firmware |
| Bluepad32 | Controller discovery, pairing, HID parsing and normalized gamepad data | Required future dependency; revision and acquisition method TBD |
| BlueKitchen / Pico Bluetooth examples | Isolated stack bring-up references | Required reference material, not additional runtime stacks |

SDK/tool versions above are the current project configuration, not a claim that
Bluepad32 has already been tested with this exact combination.

## Verified upstream references

Reviewed on 2026-10-24. Links to moving branches are discovery references, not
reproducible dependency pins.

- [Bluepad32 repository](https://github.com/ricardoquesada/bluepad32)
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

## Stack ownership decision proposal

Start with **BTstack supplied by the selected Pico SDK**, following Bluepad32's
Pico W example. Do not also compile Bluepad32's bundled BTstack into the executable.
Switching to another BTstack tree requires an explicit compatibility decision;
the upstream example documents version-specific SDK changes for that alternative.

At the reviewed upstream revision, the Pico W example links:

- `pico_stdlib`
- `pico_cyw43_arch_none`
- `pico_btstack_classic`
- `pico_btstack_cyw43`
- `bluepad32`

It also supplies application-visible `btstack_config.h` and `sdkconfig.h` and a
custom Bluepad32 platform implementation. Treat this as an upstream integration
reference, not a final target list: controller transport and pinned versions may
require additional configuration, including BLE support.

The wagon's application remains C++17; C platform callbacks must use compatible
C linkage where required. Keep dependency-specific APIs behind a small adapter.
Do not blindly copy example policy, such as deleting pairing keys at every boot,
into the wagon's pairing behavior.

## Planned integration sequence

1. Choose and record controller model/firmware and required Classic/BLE transport.
2. Select an immutable Bluepad32 release/commit compatible with SDK 2.3.1.
   Record the SDK commit and its BTstack submodule commit, not just a directory name.
3. Choose a reproducible dependency method. A pinned Git submodule or pinned
   CMake fetch are candidates; do not fetch moving `main`/`master` in normal builds.
4. Initialize the required SDK submodules. Build a relevant Pico-wrapped
   BlueKitchen example separately, using the same SDK and toolchain as the wagon.
   BlueKitchen's raw examples are not standalone Pico projects.
5. Build the pinned Bluepad32 Pico W example separately; pair the selected
   controller and log connection, disconnection, and input reports.
6. Integrate the custom platform/configuration and CMake target into the wagon,
   retaining one BTstack tree. Preserve UART diagnostics unless deliberately changed.
7. Validate reconnect and stale-input behavior before connecting actuator power.
8. Record resolved commits, licenses/notices, and tested controller evidence below.

## Reproducibility register

| Item | Resolved value |
| --- | --- |
| Pico SDK commit used for Bluetooth validation | TBD |
| SDK BTstack submodule commit | TBD |
| Bluepad32 release and commit | TBD |
| Bluepad32 acquisition method and repository path | TBD |
| Pico examples revision / selected example target | TBD |
| Controller model, firmware, transport | TBD |
| Matching stack/configuration test evidence | TBD |

## Licensing

Bluepad32 identifies its license as Apache-2.0. BTstack has its own licensing
terms; the Pico W example points to Raspberry Pi's platform-specific coverage:
[LICENSE.RP](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_btstack/LICENSE.RP).
Review the actual pinned versions and applicability to the selected board before
distribution. Preserve upstream notices and review example files individually
before copying; examples are references, not a reason to relicense dependencies.
The wagon's existing [LICENSE](../LICENSE) remains unchanged.
