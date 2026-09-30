# ESP-IDF build for the ESP32 based boards.
#
#   idf.py -C platforms/esp-idf -B build-s3-146 -DBOARD=waveshare_esp32s3_touch_lcd_1_46 build
#
# See boards/ for the supported boards.

cmake_minimum_required(VERSION 3.16)

set(REPO_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

set(BOARD waveshare_esp32s3_touch_lcd_1_46 CACHE STRING "Target board (see platforms/esp-idf/boards/)")
set(BOARD_DIR ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD})
if(NOT EXISTS ${BOARD_DIR}/board.cmake)
    file(GLOB KNOWN_BOARDS RELATIVE ${CMAKE_CURRENT_LIST_DIR}/boards ${CMAKE_CURRENT_LIST_DIR}/boards/*)
    message(FATAL_ERROR "Unknown BOARD '${BOARD}'. Choose one of: ${KNOWN_BOARDS}")
endif()
include(${BOARD_DIR}/board.cmake)      # sets IDF_TARGET and RCAT_IMAGE_SIZE
message(STATUS "Building for board ${BOARD}")

# Keep the generated configuration in the build folder, so each board's build
# folder has its own and nothing generated ends up in the source tree.
set(SDKCONFIG ${CMAKE_BINARY_DIR}/sdkconfig)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults;${BOARD_DIR}/sdkconfig.defaults")

# LVGL comes from the git submodule, the board support from its board folder.
# components/ (the portable core) and main/ are picked up automatically.
set(EXTRA_COMPONENT_DIRS ${REPO_ROOT}/third_party/lvgl ${BOARD_DIR})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Each board has its own pinned driver versions.
idf_build_set_property(DEPENDENCIES_LOCK ${BOARD_DIR}/dependencies.lock)

# components/rcat_core requires the board component by this name so the HAL gets linked.
idf_build_set_property(RCAT_BOARD_COMPONENT ${BOARD})

# Images are converted for the board's screen size (see core/media/images.cmake).
idf_build_set_property(RCAT_IMAGE_SIZE ${RCAT_IMAGE_SIZE})

# lv_conf.h lives in config/ (LVGL finds it with LV_CONF_INCLUDE_SIMPLE).
idf_build_set_property(COMPILE_OPTIONS "-I${CMAKE_CURRENT_LIST_DIR}/config" APPEND)

project(rcat-msa-lcd)
