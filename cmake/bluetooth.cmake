include(FetchContent)

foreach(bluetooth_target pico_cyw43_arch_none pico_btstack_ble pico_btstack_classic
                         pico_btstack_cyw43)
    if(NOT TARGET ${bluetooth_target})
        message(FATAL_ERROR
            "Bluetooth requires a wireless Pico board and initialized SDK submodules: missing ${bluetooth_target}")
    endif()
endforeach()

FetchContent_Declare(bluepad32
    GIT_REPOSITORY https://github.com/ricardoquesada/bluepad32.git
    GIT_TAG 80d6314e46f871719fbb4b721a2cc02227c4f369 # 5.0.0-beta0
    GIT_SUBMODULES ""
    SOURCE_SUBDIR src/components/bluepad32
)

FetchContent_MakeAvailable(bluepad32)

target_include_directories(bluepad32 PUBLIC "${PROJECT_SOURCE_DIR}/src")
