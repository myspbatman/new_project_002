# Standard Raspberry Pi Pico SDK import helper. Set PICO_SDK_PATH or pass
# -DPICO_SDK_PATH=/absolute/path/to/pico-sdk when configuring.
if (DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()
if (NOT PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set")
endif()
get_filename_component(PICO_SDK_PATH "${PICO_SDK_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
