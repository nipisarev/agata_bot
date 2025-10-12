idf_component_register(SRCS "LVGL_Driver.cpp" "st7789_display.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl driver esp_driver_spi esp_driver_ledc)
