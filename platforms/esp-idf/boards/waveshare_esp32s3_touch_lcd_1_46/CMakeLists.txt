idf_component_register(SRCS hal_board.c
                       REQUIRES rcat_core lvgl esp_lcd esp_driver_spi esp_driver_i2c
                                esp_driver_gpio esp_driver_ledc esp_driver_i2s esp_driver_sdmmc
                                sdmmc fatfs esp_adc esp_timer nvs_flash
                                esp_wifi esp_netif esp_event)

# Where the speaker's sound goes, from platforms/esp-idf/features.cmake
include(${CMAKE_CURRENT_LIST_DIR}/../../features.cmake)
if(RCAT_SPEAKER STREQUAL "EXTERNAL")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RCAT_SPEAKER_EXTERNAL=1)
elseif(NOT RCAT_SPEAKER STREQUAL "BUILT_IN")
    message(FATAL_ERROR "RCAT_SPEAKER is '${RCAT_SPEAKER}'. Use BUILT_IN or EXTERNAL.")
endif()
