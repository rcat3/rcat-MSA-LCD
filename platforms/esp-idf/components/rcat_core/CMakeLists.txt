# The portable application code in core/, built as an ESP-IDF component.
set(CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../core)

# CONFIGURE_DEPENDS so a new source file is picked up without a reconfigure.
# ESP-IDF first reads this file as a script, where it isn't allowed.
if(CMAKE_BUILD_EARLY_EXPANSION)
    file(GLOB_RECURSE CORE_SOURCES ${CORE_DIR}/*.c)
else()
    file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS ${CORE_DIR}/*.c)
endif()

# Optional features, chosen in platforms/esp-idf/features.cmake
include(${CMAKE_CURRENT_LIST_DIR}/../../features.cmake)
include(${CORE_DIR}/features.cmake)
rcat_feature_filter_sources(CORE_SOURCES)

# Images from imgs/, converted for the board's screen size. This can't run
# while ESP-IDF is only scanning components for their requirements.
set(IMAGE_SOURCES "")
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    idf_build_get_property(image_size RCAT_IMAGE_SIZE)
    # ESP-IDF's own Python by default; pass -DRCAT_PYTHON=... to use another one.
    if(RCAT_PYTHON)
        set(python ${RCAT_PYTHON})
    else()
        idf_build_get_property(python PYTHON)
    endif()
    include(${CORE_DIR}/media/images.cmake)
    rcat_generate_images(IMAGE_SOURCES ${image_size} ${python})
endif()

# core calls the HAL functions implemented by the board component
# (boards/<BOARD>), and the board includes core's hal.h, so each requires
# the other.
idf_build_get_property(board_component RCAT_BOARD_COMPONENT)

idf_component_register(SRCS ${CORE_SOURCES} ${IMAGE_SOURCES}
                       INCLUDE_DIRS ${CORE_DIR}/include
                       PRIV_INCLUDE_DIRS ${CORE_DIR} ${CORE_DIR}/../third_party/dr_libs
                       REQUIRES lvgl
                       PRIV_REQUIRES ${board_component} lwip)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    rcat_feature_definitions(feature_definitions)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ${feature_definitions})

    # The bigger animated GIFs, on boards with room for them (see board.cmake)
    idf_build_get_property(large_images RCAT_LARGE_IMAGES)
    if(large_images)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE RCAT_LARGE_IMAGES=1)
    endif()
endif()

# Images and sounds can be loaded from the SD card
target_compile_definitions(${COMPONENT_LIB} PRIVATE RCAT_HAVE_FILESYSTEM=1)
# and the voice lab streams over WiFi with BSD sockets
target_compile_definitions(${COMPONENT_LIB} PRIVATE RCAT_HAVE_NETWORK=1)
