#ifndef ST7789_DISPLAY_H
#define ST7789_DISPLAY_H

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lvgl.h"

#define LCD_HOST    SPI2_HOST
#define DMA_CHAN    SPI_DMA_CH_AUTO

// ESP32-C6-LCD-1.47 Pin definitions
#define PIN_NUM_MISO 5
#define PIN_NUM_MOSI 6
#define PIN_NUM_CLK  7
#define PIN_NUM_CS   14
#define PIN_NUM_DC   15
#define PIN_NUM_RST  21
#define PIN_NUM_BCKL 22

#define LCD_H_RES 172
#define LCD_V_RES 320

typedef struct {
    spi_device_handle_t spi;
    int dc_io;
    int reset_io;
    int backlight_io;
} st7789_handle_t;

void st7789_init(void);
void st7789_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
void display_set_brightness(uint8_t brightness);
esp_err_t st7789_test_display(void);

// Function aliases for compatibility with LVGL_Driver
#define LCD_Init() st7789_init()
#define LCD_addWindow(x1, y1, x2, y2, color) st7789_flush_cb(NULL, &(lv_area_t){x1, y1, x2, y2}, (lv_color_t*)color)
#define Backlight_Init() display_set_brightness(50)
#define Set_Backlight(level) display_set_brightness(level)

#endif