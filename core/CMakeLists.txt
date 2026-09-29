# Portable application code shared by every platform.
#
# Nothing in here may include vendor SDK headers (pico-sdk, ESP-IDF, ...).
# Hardware access goes through include/hal.h, which each board implements.
# The including project must define an `lvgl` library target first.

file(GLOB_RECURSE RCAT_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.c)

add_library(rcat_core STATIC ${RCAT_CORE_SOURCES})
target_include_directories(rcat_core
    PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(rcat_core PUBLIC lvgl)
