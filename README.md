# WhiskeyWagon

C++17 firmware for a Bluetooth-controlled RC wagon, built with the Raspberry Pi
Pico SDK and currently targeting the Pico W. Planned hardware includes an SG92R
steering servo, two DRV8833-driven motors, and Hall-effect motor encoders.

## Project documentation

The [project source of truth](docs/README.md) indexes requirements, the
[authoritative pin register](docs/hardware.md), firmware design, Bluetooth
dependencies, and verification procedures.

UART0 is reserved and the driver/encoder GPIO wiring plan is recorded in the
hardware register; physical wiring is not yet verified. Bluepad32 and BlueKitchen
BTstack integration and the wagon pin configuration are documented but not
implemented; the current firmware is still the UART demonstration described below.

## Repository layout

```text
WhiskeyWagon/
    .vscode/                 Shared Pico extension build/debug configuration
    cmake/
        pico_sdk_import.cmake  SDK-provided import helper
    docs/                    Requirements, hardware register, design and verification
    src/
        main.cpp             Firmware entry point
    CMakeLists.txt           Project and firmware target
    build/                   Generated output (ignored by Git)
```

Keep implementation files and private headers in `src/`. Add
`include/whiskey_wagon/` when there are public library headers, and `tests/` when
there is testable application logic; empty placeholder directories are not needed.
Keep SDK code and toolchains outside the repository.

## Prerequisites

- CMake 3.13 or newer, Ninja, Python 3, and an Arm GNU bare-metal toolchain
  (`arm-none-eabi-gcc` and `arm-none-eabi-g++`).
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk), including its
  required submodules (`git submodule update --init --recursive` from the SDK directory).
- Alternatively, install the recommended Raspberry Pi Pico VS Code extension
  and let it install the SDK and tools.

The Pico extension integration selects SDK 2.3.1, toolchain 15_2_Rel1, and
picotool 2.3.1. Without an explicit `PICO_SDK_PATH`, CMake uses that installation
when available. Set `PICO_SDK_PATH` in the environment or CMake cache to use
another SDK installation, and ensure its toolchain is available on `PATH` or set
`PICO_TOOLCHAIN_PATH`.

## Build

From a terminal with the prerequisites on `PATH` (the Pico extension's terminal
provides these), run:

```sh
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target whiskey_wagon
```

For an external SDK installation, add `-DPICO_SDK_PATH="<SDK directory>"` to the
configure command. Use `-DCMAKE_BUILD_TYPE=Release` for an optimized build.
Use a separate build directory when switching boards or toolchains.

In VS Code, configure the project with the Pico extension, select the
`whiskey_wagon` target, and run **Compile Project** (the default build task).
Debug and flash configurations resolve the selected target through the extension.

The build produces `whiskey_wagon.elf`, `whiskey_wagon.uf2`, and other firmware
outputs in `build/`, along with `compile_commands.json` for editor tooling.
To flash without a debug probe, hold BOOTSEL while connecting the board over
USB, then copy `whiskey_wagon.uf2` to its mass-storage drive.

## Current behavior

- UART stdio is enabled; USB stdio is disabled.
- UART1 uses GPIO4 (TX) and GPIO5 (RX) at 115200 baud and sends a startup greeting.
- The firmware prints `Hello, world!` once per second through the board's default
  stdio UART (UART0 on Pico W, TX GPIO0 / RX GPIO1).

## Conventions

- Use `snake_case` for source/header filenames, CMake targets, namespaces,
  functions, and variables; use `PascalCase` for types and `kPascalCase` for constants.
- Use `.cpp` for C++ sources and `.hpp` for project C++ headers.
- Use four-space indentation and format C++ with the root `.clang-format`.
- Keep includes and dependencies target-scoped in CMake and list sources explicitly.
- Keep generated files, local build presets, SDKs, and toolchains out of Git.

## Validation

Configure and build the firmware before submitting changes. There are no automated
application tests yet; UART behavior and flashing require a connected board.

## License

See [LICENSE](LICENSE). The SDK import helper retains its upstream license notice.
