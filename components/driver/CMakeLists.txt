idf_component_register(SRCS "LVGL_Driver.cpp" "st7789_display.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl esp_driver_spi esp_driver_ledc esp_driver_gpio esp_timer freertos)
