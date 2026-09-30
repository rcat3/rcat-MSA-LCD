idf_component_register(SRCS hal_board.c
                       REQUIRES rcat_core lvgl esp_lcd esp_driver_spi esp_driver_i2c
                                esp_driver_gpio esp_driver_ledc esp_driver_i2s esp_driver_sdmmc
                                sdmmc fatfs esp_adc esp_timer nvs_flash)
