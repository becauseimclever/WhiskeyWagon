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

option(WHISKEY_BUILD_POC "Build the separate closed-loop wagon POC image" ON)
option(WHISKEY_POC_ACTUATORS_VERIFIED
    "Enable POC outputs ONLY after verifying controlled STBY, both Halls, mapping, cadence and servo limits" OFF)
option(WHISKEY_POC_EXTERNAL_STBY_BENCH
    "Enable supervised bench outputs with GP14 disconnected and STBY externally enabled; no hardware inhibit" OFF)
set(WHISKEY_POC_MAX_DUTY_PERMILLE 250 CACHE STRING "POC maximum duty, 100-1000; provisional default 25 percent")
set(WHISKEY_POC_ACCELERATION_MS 1000 CACHE STRING "POC zero-to-cap acceleration, 100-1000 ms")
set(WHISKEY_POC_REPORT_TIMEOUT_MS 500 CACHE STRING "POC report lease, 100-2000 ms; verify held-report cadence")
set(WHISKEY_POC_SERVO_LEFT_US 1400 CACHE STRING "POC physical-left pulse; verify with linkage disconnected")
set(WHISKEY_POC_SERVO_CENTER_US 1500 CACHE STRING "POC center pulse; not a measured SG92R calibration")
set(WHISKEY_POC_SERVO_RIGHT_US 1600 CACHE STRING "POC physical-right pulse; verify mechanical limits")
set(WHISKEY_POC_MAX_WHEEL_RPM 35 CACHE STRING "Closed-loop full-demand wheel speed, 5-37 rpm")
set(WHISKEY_POC_STEER_LOCK_DEG 30 CACHE STRING "Front-beam angle at a servo endpoint, 5-60 degrees; calibrate after assembly")
option(WHISKEY_POC_LEFT_INVERTED "Invert left motor electrical polarity in POC" OFF)
option(WHISKEY_POC_RIGHT_INVERTED "Invert right motor electrical polarity in POC" OFF)
if((WHISKEY_POC_ACTUATORS_VERIFIED OR WHISKEY_POC_EXTERNAL_STBY_BENCH) AND NOT WHISKEY_BUILD_POC)
    message(FATAL_ERROR "POC output options require WHISKEY_BUILD_POC=ON")
endif()
if(WHISKEY_POC_ACTUATORS_VERIFIED AND WHISKEY_POC_EXTERNAL_STBY_BENCH)
    message(FATAL_ERROR "Controlled and externally enabled STBY modes are mutually exclusive")
endif()
if(WHISKEY_BUILD_POC)
    set(WHISKEY_POC_OUTPUTS_ENABLED OFF)
    if(WHISKEY_POC_ACTUATORS_VERIFIED OR WHISKEY_POC_EXTERNAL_STBY_BENCH)
        set(WHISKEY_POC_OUTPUTS_ENABLED ON)
    endif()
    if(WHISKEY_POC_OUTPUTS_ENABLED AND NOT WHISKEY_ENCODER_CAPTURE_VERIFIED)
        message(FATAL_ERROR "Powered POC requires WHISKEY_ENCODER_CAPTURE_VERIFIED=ON for BOTH motors")
    endif()
    add_library(poc_configuration INTERFACE)
    foreach(setting MAX_DUTY_PERMILLE ACCELERATION_MS REPORT_TIMEOUT_MS SERVO_LEFT_US SERVO_CENTER_US SERVO_RIGHT_US
            MAX_WHEEL_RPM STEER_LOCK_DEG)
        if(NOT WHISKEY_POC_${setting} MATCHES "^[0-9]+$")
            message(FATAL_ERROR "WHISKEY_POC_${setting} must be an integer")
        endif()
        target_compile_definitions(poc_configuration INTERFACE
            WHISKEY_POC_${setting}=${WHISKEY_POC_${setting}})
    endforeach()
    target_compile_definitions(poc_configuration INTERFACE
        WHISKEY_POC_LEFT_INVERTED=$<BOOL:${WHISKEY_POC_LEFT_INVERTED}>
        WHISKEY_POC_RIGHT_INVERTED=$<BOOL:${WHISKEY_POC_RIGHT_INVERTED}>)
    add_library(poc_logic_tests OBJECT tests/poc_logic_tests.cpp)
    target_include_directories(poc_logic_tests PRIVATE src)
    target_link_libraries(poc_logic_tests PRIVATE poc_configuration)
    set_target_properties(poc_logic_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(whiskey_wagon_poc
        src/main.cpp src/bluetooth_adapter.c src/encoder_capture.cpp src/poc_outputs.cpp)
    set_target_properties(whiskey_wagon_poc PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_compile_options(whiskey_wagon_poc PRIVATE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>")
    target_compile_definitions(whiskey_wagon_poc PRIVATE
        WHISKEY_WAGON_POC=1
        WHISKEY_POC_ACTUATORS_VERIFIED=$<BOOL:${WHISKEY_POC_ACTUATORS_VERIFIED}>
        WHISKEY_POC_EXTERNAL_STBY_BENCH=$<BOOL:${WHISKEY_POC_EXTERNAL_STBY_BENCH}>
        WHISKEY_POC_OUTPUTS_ENABLED=$<BOOL:${WHISKEY_POC_OUTPUTS_ENABLED}>
        WHISKEY_ENCODER_CAPTURE_VERIFIED=$<BOOL:${WHISKEY_ENCODER_CAPTURE_VERIFIED}>
        PICO_DEFAULT_UART=0 PICO_DEFAULT_UART_TX_PIN=0 PICO_DEFAULT_UART_RX_PIN=1
        PICO_DEFAULT_UART_BAUD_RATE=115200)
    target_link_libraries(whiskey_wagon_poc PRIVATE
        pico_stdlib hardware_pwm hardware_watchdog bluepad32 poc_configuration)
    add_dependencies(whiskey_wagon_poc bench_logic_tests poc_logic_tests)
    pico_set_program_name(whiskey_wagon_poc "WhiskeyWagon closed-loop POC")
    pico_set_program_version(whiskey_wagon_poc "${PROJECT_VERSION}")
    option(WHISKEY_POC_USB_CONSOLE
        "Bench only: send POC diagnostics to USB CDC instead of UART0; output-only, no commands" OFF)
    target_compile_definitions(whiskey_wagon_poc PRIVATE
        WHISKEY_POC_USB_CONSOLE=$<BOOL:${WHISKEY_POC_USB_CONSOLE}>)
    if(WHISKEY_POC_USB_CONSOLE)
        target_compile_definitions(whiskey_wagon_poc PRIVATE
            PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=0
            PICO_STDIO_USB_STDOUT_TIMEOUT_US=1000
            PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0
            PICO_ENABLE_USB_RESET_VIA_BAUD_RATE=0
            PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=0)
        pico_enable_stdio_uart(whiskey_wagon_poc 0)
        pico_enable_stdio_usb(whiskey_wagon_poc 1)
    else()
        pico_enable_stdio_uart(whiskey_wagon_poc 1)
        pico_enable_stdio_usb(whiskey_wagon_poc 0)
    endif()
    pico_add_extra_outputs(whiskey_wagon_poc)
endif()

option(WHISKEY_BUILD_RIGHT_MOTOR_TEST "Build the standalone USB right-motor bench test" OFF)
option(WHISKEY_RIGHT_MOTOR_BENCH_VERIFIED
    "Confirm right motor wiring, 3.3 V Hall/STBY interfaces and secured powered bench" OFF)
option(WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED
    "Leave GP14 untouched in right-motor test; STBY wire must be disconnected" OFF)
option(WHISKEY_RIGHT_MOTOR_BOOT_PULSE
    "DANGER: automatic one-shot 50 percent 5 second pulse after 3 second boot delay, no serial required" OFF)
option(WHISKEY_RIGHT_MOTOR_BOOT_FULL_DUTY
    "DANGER: override boot pulse to 100 percent for 1 second; requires BOOT_PULSE" OFF)
if(WHISKEY_BUILD_RIGHT_MOTOR_TEST)
    if(WHISKEY_RIGHT_MOTOR_BOOT_FULL_DUTY AND NOT WHISKEY_RIGHT_MOTOR_BOOT_PULSE)
        message(FATAL_ERROR "Full-duty boot test requires WHISKEY_RIGHT_MOTOR_BOOT_PULSE=ON")
    endif()
    if(NOT WHISKEY_RIGHT_MOTOR_BENCH_VERIFIED)
        message(FATAL_ERROR "Right-motor test requires WHISKEY_RIGHT_MOTOR_BENCH_VERIFIED=ON; see docs/verification.md")
    endif()
    add_executable(right_motor_test src/right_motor_test.cpp src/encoder_capture.cpp)
    target_include_directories(right_motor_test PRIVATE src)
    target_compile_options(right_motor_test PRIVATE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
    )
    target_compile_definitions(right_motor_test PRIVATE
        WHISKEY_ENCODER_CAPTURE_VERIFIED=1
        WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED=$<BOOL:${WHISKEY_RIGHT_MOTOR_STBY_DISCONNECTED}>
        WHISKEY_RIGHT_MOTOR_BOOT_PULSE=$<BOOL:${WHISKEY_RIGHT_MOTOR_BOOT_PULSE}>
        WHISKEY_RIGHT_MOTOR_BOOT_FULL_DUTY=$<BOOL:${WHISKEY_RIGHT_MOTOR_BOOT_FULL_DUTY}>
        PICO_STDIO_USB_STDOUT_TIMEOUT_US=1000
        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0
    )
    target_link_libraries(right_motor_test PRIVATE pico_stdlib hardware_pwm hardware_watchdog)
    add_dependencies(right_motor_test bench_logic_tests)
    pico_set_program_name(right_motor_test "WhiskeyWagon right motor test")
    pico_enable_stdio_uart(right_motor_test 0)
    pico_enable_stdio_usb(right_motor_test 1)
    pico_add_extra_outputs(right_motor_test)
endif()

option(WHISKEY_BUILD_SERVO_TEST "Build the standalone, manually armed USB servo bench test" OFF)
if(WHISKEY_BUILD_SERVO_TEST)
    add_library(servo_test_logic_tests OBJECT tests/servo_test_logic_tests.cpp)
    target_include_directories(servo_test_logic_tests PRIVATE src)
    target_compile_features(servo_test_logic_tests PRIVATE cxx_std_17)
    set_target_properties(servo_test_logic_tests PROPERTIES CXX_EXTENSIONS OFF)

    add_executable(servo_test src/servo_test.cpp)
    target_include_directories(servo_test PRIVATE src)
    set_target_properties(servo_test PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_compile_options(servo_test PRIVATE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
    )
    target_compile_definitions(servo_test PRIVATE
        PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=0
        PICO_STDIO_USB_STDOUT_TIMEOUT_US=1000
        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0
        PICO_ENABLE_USB_RESET_VIA_BAUD_RATE=0
        PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=0
    )
    target_link_libraries(servo_test PRIVATE pico_stdlib hardware_pwm hardware_watchdog)
    add_dependencies(servo_test servo_test_logic_tests)
    pico_set_program_name(servo_test "WhiskeyWagon servo bench test")
    pico_enable_stdio_uart(servo_test 0)
    pico_enable_stdio_usb(servo_test 1)
    pico_add_extra_outputs(servo_test)
endif()
