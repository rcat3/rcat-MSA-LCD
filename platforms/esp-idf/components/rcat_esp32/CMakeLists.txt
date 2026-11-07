# The parts of the HAL that are the same on every ESP32 board (see
# include/rcat_esp32.h). Each board's component requires this one. It only
# needs core's hal.h, so it doesn't require rcat_core, which keeps it after the
# board and the core when they're linked.
idf_component_register(SRCS hal_common.c hal_imu.c hal_settings.c hal_network.c hal_ble.c lvgl_mem.c
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS ../../../../core/include
                       REQUIRES lvgl esp_driver_i2c esp_driver_sdmmc sdmmc fatfs nvs_flash
                                esp_wifi esp_netif esp_event bt)

# LVGL's memory functions are in lvgl_mem.c. Only LVGL calls them, and LVGL is
# linked after this component, so ask for them up front or the linker passes
# them by.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u lv_mem_init")
