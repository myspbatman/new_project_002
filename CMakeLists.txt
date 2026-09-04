cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(rp2040_eth_battery_monitor C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(rp2040_eth_battery_monitor
    src/main.cpp
    src/sensor/ina2xx.cpp
    src/battery/battery_monitor.cpp
    src/network/ch9120.cpp
    src/http_server.cpp
    src/api/rest_api.cpp
    src/led/status_led.cpp
    src/utils/watchdog.cpp
)

pico_generate_pio_header(rp2040_eth_battery_monitor
    ${CMAKE_CURRENT_LIST_DIR}/src/led/ws2812.pio
)

target_include_directories(rp2040_eth_battery_monitor PRIVATE src)
target_link_libraries(rp2040_eth_battery_monitor
    pico_stdlib
    hardware_i2c
    hardware_uart
    hardware_pio
    hardware_watchdog
    hardware_sync
)

pico_enable_stdio_usb(rp2040_eth_battery_monitor 1)
pico_enable_stdio_uart(rp2040_eth_battery_monitor 0)
pico_add_extra_outputs(rp2040_eth_battery_monitor)
