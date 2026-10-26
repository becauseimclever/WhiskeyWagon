# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.18...3.27)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep the version variables used by the Raspberry Pi Pico VS Code extension.
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
file(TO_CMAKE_PATH "${USERHOME}" USERHOME)
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
set(picoVscodeSdkPath "${USERHOME}/.pico-sdk/sdk/${sdkVersion}")
if(PICO_SDK_PATH)
    file(TO_CMAKE_PATH "${PICO_SDK_PATH}" picoRequestedSdkPath)
else()
    file(TO_CMAKE_PATH "$ENV{PICO_SDK_PATH}" picoRequestedSdkPath)
endif()
if (EXISTS "${picoVscode}"
        AND (NOT picoRequestedSdkPath OR picoRequestedSdkPath STREQUAL picoVscodeSdkPath))
    include("${picoVscode}")
endif()
set(PICO_BOARD pico_w CACHE STRING "Board type")

# The SDK import must precede project() to select the cross compiler.
include(cmake/pico_sdk_import.cmake)

project(whiskey_wagon VERSION 0.1.0 LANGUAGES C CXX ASM)

pico_sdk_init()

include(cmake/bluetooth.cmake)

add_executable(whiskey_wagon
    src/main.cpp
    src/bluetooth_adapter.c
    src/encoder_capture.cpp
)

option(WHISKEY_ENCODER_CAPTURE_VERIFIED
    "Enable raw GP10-GP13 capture ONLY after verifying 3.3 V encoder interface and biasing" OFF)
target_compile_definitions(whiskey_wagon PRIVATE
    WHISKEY_ENCODER_CAPTURE_VERIFIED=$<BOOL:${WHISKEY_ENCODER_CAPTURE_VERIFIED}>
)
add_library(bench_logic_tests OBJECT tests/bench_logic_tests.cpp)
target_include_directories(bench_logic_tests PRIVATE src)
target_compile_features(bench_logic_tests PRIVATE cxx_std_17)
set_target_properties(bench_logic_tests PROPERTIES CXX_EXTENSIONS OFF)
add_dependencies(whiskey_wagon bench_logic_tests)

set_target_properties(whiskey_wagon PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_options(whiskey_wagon PRIVATE
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
)
set_source_files_properties(src/bluetooth_adapter.c PROPERTIES
    COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
)
target_link_libraries(whiskey_wagon PRIVATE
    pico_stdlib
    hardware_watchdog
    bluepad32
)
target_compile_definitions(whiskey_wagon PRIVATE
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
    PICO_DEFAULT_UART_BAUD_RATE=115200
)

pico_set_program_name(whiskey_wagon "WhiskeyWagon")
pico_set_program_version(whiskey_wagon "${PROJECT_VERSION}")

pico_enable_stdio_uart(whiskey_wagon 1)
pico_enable_stdio_usb(whiskey_wagon 0)
pico_add_extra_outputs(whiskey_wagon)
