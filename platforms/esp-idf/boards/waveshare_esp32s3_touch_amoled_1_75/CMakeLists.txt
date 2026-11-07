idf_component_register(SRCS hal_board.c
                       REQUIRES rcat_core rcat_esp32 lvgl esp_lcd esp_driver_spi esp_driver_i2c
                                esp_driver_gpio esp_driver_i2s esp_timer)
