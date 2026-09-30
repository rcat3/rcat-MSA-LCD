# Portable application code shared by every platform.
#
# Nothing in here may include vendor SDK headers (pico-sdk, ESP-IDF, ...).
# Hardware access goes through include/hal.h, which each board implements.
#
# The including project must define an `lvgl` library target first, and set
# RCAT_IMAGE_SIZE to the board's screen size (images are converted for it).
# RCAT_PYTHON can be set to pick the Python used for the image converter.

file(GLOB_RECURSE RCAT_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.c)

if(NOT RCAT_IMAGE_SIZE)
    message(FATAL_ERROR "RCAT_IMAGE_SIZE isn't set. The board's .cmake file should set it to the screen size.")
endif()
if(NOT RCAT_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(RCAT_PYTHON ${Python3_EXECUTABLE})
endif()
include(${CMAKE_CURRENT_LIST_DIR}/media/images.cmake)
rcat_generate_images(RCAT_IMAGE_SOURCES ${RCAT_IMAGE_SIZE} ${RCAT_PYTHON})

add_library(rcat_core STATIC ${RCAT_CORE_SOURCES} ${RCAT_IMAGE_SOURCES})
target_include_directories(rcat_core
    PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(rcat_core PUBLIC lvgl)
