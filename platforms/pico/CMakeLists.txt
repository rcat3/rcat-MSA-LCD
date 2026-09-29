# pico-sdk build for the RP2040/RP2350 based boards.
#
#   cmake -S platforms/pico -B build -DBOARD=waveshare_rp2350_touch_lcd_1_28
#
# See boards/*.cmake for the supported boards.

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(REPO_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

# Pick the board. This has to happen before the SDK is imported because it sets PICO_BOARD.
set(BOARD waveshare_rp2040_touch_lcd_1_28 CACHE STRING "Target board (see platforms/pico/boards/*.cmake)")
set(BOARD_FILE ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}.cmake)
if(NOT EXISTS ${BOARD_FILE})
    file(GLOB KNOWN_BOARDS RELATIVE ${CMAKE_CURRENT_LIST_DIR}/boards ${CMAKE_CURRENT_LIST_DIR}/boards/*.cmake)
    string(REPLACE ".cmake" "" KNOWN_BOARDS "${KNOWN_BOARDS}")
    message(FATAL_ERROR "Unknown BOARD '${BOARD}'. Choose one of: ${KNOWN_BOARDS}")
endif()
include(${BOARD_FILE})
message(STATUS "Building for board ${BOARD}")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(rcat-msa-lcd C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# LVGL (git submodule). lv_conf.h comes from config/.
file(GLOB_RECURSE LVGL_SOURCES ${REPO_ROOT}/third_party/lvgl/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC
    ${REPO_ROOT}/third_party/lvgl
    ${CMAKE_CURRENT_LIST_DIR}/config
)

# Portable application code
add_subdirectory(${REPO_ROOT}/core ${CMAKE_BINARY_DIR}/core)

# Board support: HAL implementation and the Waveshare drivers
set(BOARD_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD_DIR})
file(GLOB BOARD_SOURCES CONFIGURE_DEPENDS ${BOARD_SRC_DIR}/*.c)
add_library(board STATIC ${BOARD_SOURCES})
target_include_directories(board PUBLIC ${BOARD_SRC_DIR})
target_link_libraries(board PUBLIC
    rcat_core
    lvgl
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_pwm
    hardware_adc
    hardware_dma
    hardware_clocks
)

add_executable(rcat-msa-lcd main.c)

pico_set_program_name(rcat-msa-lcd "rcat-msa-lcd")
pico_set_program_version(rcat-msa-lcd "0.3")

pico_enable_stdio_uart(rcat-msa-lcd 0)
pico_enable_stdio_usb(rcat-msa-lcd 1)

target_link_libraries(rcat-msa-lcd rcat_core board)

pico_add_extra_outputs(rcat-msa-lcd)
