# The portable application code in core/, built as an ESP-IDF component.
set(CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../core)

file(GLOB_RECURSE CORE_SOURCES ${CORE_DIR}/*.c)

# core calls the HAL functions implemented by the board component
# (boards/<BOARD>), and the board includes core's hal.h, so each requires
# the other.
idf_build_get_property(board_component RCAT_BOARD_COMPONENT)

idf_component_register(SRCS ${CORE_SOURCES}
                       INCLUDE_DIRS ${CORE_DIR}/include
                       PRIV_INCLUDE_DIRS ${CORE_DIR}
                       REQUIRES lvgl
                       PRIV_REQUIRES ${board_component})
