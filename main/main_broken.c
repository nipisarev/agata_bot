#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "sd_images.h"

static const char *TAG = "AGATA_SD_SLIDESHOW";

// Pin definitions from working example
static int PIN_MOSI = 6;   
static int PIN_SCLK = 7;   
static int PIN_CS = 14;    
static int PIN_DC = 15;    
static int PIN_RST = 21;   
static int PIN_BCKL = 22;  

// Display dimensions and offsets from working example
#define LCD_WIDTH   172
#define LCD_HEIGHT  320
#define OFFSET_X    34    // Critical offset!
#define OFFSET_Y    0

// ST7789 Commands
#define ST7789_SWRESET    0x01
#define ST7789_SLPOUT     0x11
#define ST7789_DISPON     0x29
#define ST7789_CASET      0x2A
#define ST7789_RASET      0x2B
#define ST7789_RAMWR      0x2C
#define ST7789_MADCTL     0x36
#define ST7789_COLMOD     0x3A

static spi_device_handle_t spi;

void spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(PIN_DC, dc);
}

void st7789_write_cmd(uint8_t cmd)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void*)0;  // DC = 0 for command
    spi_device_polling_transmit(spi, &t);
}

void st7789_write_data_byte(uint8_t data)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &data;
    t.user = (void*)1;  // DC = 1 for data
    spi_device_polling_transmit(spi, &t);
}

void st7789_write_data_buf(uint8_t *buf, int len)
{
    if (len <= 0) return;
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = buf;
    t.user = (void*)1;  // DC = 1 for data
    spi_device_polling_transmit(spi, &t);
}

void st7789_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Apply offset
    x0 += OFFSET_X;
    x1 += OFFSET_X;
    y0 += OFFSET_Y;
    y1 += OFFSET_Y;
    
    // Column set
    st7789_write_cmd(ST7789_CASET);
    st7789_write_data_byte(x0 >> 8);
    st7789_write_data_byte(x0);
    st7789_write_data_byte(x1 >> 8);
    st7789_write_data_byte(x1);
    
    // Row set
    st7789_write_cmd(ST7789_RASET);
    st7789_write_data_byte(y0 >> 8);
    st7789_write_data_byte(y0);
    st7789_write_data_byte(y1 >> 8);
    st7789_write_data_byte(y1);
    
    // Write to RAM
    st7789_write_cmd(ST7789_RAMWR);
}

void backlight_init()
{
    // Configure backlight
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_config);
    
    ledc_channel_config_t channel_config = {
        .channel = LEDC_CHANNEL_0,
        .duty = 255,  // Full brightness
        .gpio_num = PIN_BCKL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_0
    };
    ledc_channel_config(&channel_config);
    
    ESP_LOGI(TAG, "Backlight initialized on pin %d", PIN_BCKL);
}

void st7789_init()
{
    ESP_LOGI(TAG, "Initializing ST7789 display");
    
    // Reset display
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Software reset
    st7789_write_cmd(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    // Sleep out
    st7789_write_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Memory Data Access Control
    st7789_write_cmd(ST7789_MADCTL);
    st7789_write_data_byte(0x00);  // Top to bottom, left to right
    
    // Interface Pixel Format
    st7789_write_cmd(ST7789_COLMOD);
    st7789_write_data_byte(0x55);  // 16-bit color
    
    // Display On
    st7789_write_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "ST7789 initialization complete");
}

void init_spi_and_display()
{
    ESP_LOGI(TAG, "Initializing SPI and display");
    
    // Configure GPIO pins
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // Initialize backlight
    backlight_init();
    
    // SPI configuration
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,  // Not used
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 8  // Full screen buffer
    };
    
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 80 * 1000 * 1000,  // 80MHz
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 7,
        .pre_cb = spi_pre_transfer_callback
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "SPI initialized successfully");
    
    // Initialize display
    st7789_init();
}

void clear_display()
{
    ESP_LOGI(TAG, "Clearing display");
    
    st7789_set_addr_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    
    // Send black pixels
    uint16_t black_pixel = 0x0000;
    uint8_t black_bytes[2] = {(black_pixel >> 8) & 0xFF, black_pixel & 0xFF};
    
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        st7789_write_data_buf(black_bytes, 2);
    }
}

void display_sd_image(const sd_image_t *img)
{
    if (!img || !img->data) {
        ESP_LOGE(TAG, "Invalid image data");
        return;
    }
    
    ESP_LOGI(TAG, "Displaying SD image: %dx%d", img->width, img->height);
    
    // Set address window for the image
    st7789_set_addr_window(0, 0, img->width - 1, img->height - 1);
    
    // Send pixel data in chunks to avoid large SPI transactions
    const size_t CHUNK_SIZE = 4096;  // 2KB chunks
    const uint8_t *pixel_data = (const uint8_t *)img->data;
    size_t total_bytes = img->width * img->height * 2;  // RGB565 = 2 bytes per pixel
    
    for (size_t offset = 0; offset < total_bytes; offset += CHUNK_SIZE) {
        size_t chunk_size = (offset + CHUNK_SIZE > total_bytes) ? 
                           (total_bytes - offset) : CHUNK_SIZE;
        
        st7789_write_data_buf((uint8_t *)(pixel_data + offset), chunk_size);
    }
}

// Animation configuration
#define INIT_SLIDE_DELAY_MS         200  // Delay between init animation frames  
#define ANIMATION_SLIDE_DELAY_MS    100  // Delay between hi animation frames
#define ANIMATION_LOOP_DELAY_MS     600  // 3 second pause between hi loops

static void animation_task(void *pvParameter)
{
    bool init_completed = false;
    int current_slide = 0;
    
    // Load animation data from SD card
    animation_set_t *anim_set = load_animations();
    if (!anim_set) {
        ESP_LOGE(TAG, "Failed to load animations from SD card");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Starting dual animation system - init (%d frames) then hi loop (%d frames)", 
             anim_set->init_count, anim_set->hi_count);
    
    while (1) {
        if (!init_completed) {
            // Play init animation once
            if (current_slide < anim_set->init_count) {
                ESP_LOGI(TAG, "Playing init animation - frame %d/%d", current_slide + 1, anim_set->init_count);
                
                display_sd_image(&anim_set->init_images[current_slide]);
                
                current_slide++;
                if (current_slide >= anim_set->init_count) {
                    ESP_LOGI(TAG, "Init animation completed, switching to hi loop");
                    init_completed = true;
                    current_slide = 0;
                    vTaskDelay(pdMS_TO_TICKS(ANIMATION_LOOP_DELAY_MS)); // Pause before starting hi loop
                } else {
                    vTaskDelay(pdMS_TO_TICKS(INIT_SLIDE_DELAY_MS));
                }
            } else {
                ESP_LOGW(TAG, "No init frames available, switching to hi loop");
                init_completed = true;
                current_slide = 0;
            }
        } else {
            // Play hi animation in continuous loop
            if (anim_set->hi_count > 0) {
                ESP_LOGI(TAG, "Playing hi loop - frame %d/%d", current_slide + 1, anim_set->hi_count);
                
                display_sd_image(&anim_set->hi_images[current_slide]);
                
                current_slide++;
                if (current_slide >= anim_set->hi_count) {
                    current_slide = 0;
                    ESP_LOGI(TAG, "Hi animation loop completed, restarting...");
                    vTaskDelay(pdMS_TO_TICKS(ANIMATION_LOOP_DELAY_MS));
                } else {
                    vTaskDelay(pdMS_TO_TICKS(ANIMATION_SLIDE_DELAY_MS));
                }
            } else {
                ESP_LOGW(TAG, "No hi frames available, stopping animation");
                break;
            }
        }
    }
    
    // Cleanup
    free_animations(anim_set);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 AGATA Robot SD Card Slideshow Starting");
    
    // Initialize SD card system first
    if (init_sd_card() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD card system");
        return;
    }
    ESP_LOGI(TAG, "SD card system initialized successfully");
    
    // Initialize SPI and display with working configuration
    init_spi_and_display();
    
    ESP_LOGI(TAG, "Display initialized, preparing for slideshow");
    
    // Clear any garbage/noise from display first
    clear_display();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Display cleared, starting SD card animation system");
    
    // Start animation task
    xTaskCreate(
        animation_task,           // Task function
        "sd_animation_task",      // Task name
        16384,                    // Increased stack size for SD operations
        NULL,                     // Parameters
        5,                        // Priority
        NULL                      // Task handle
    );
    
    ESP_LOGI(TAG, "SD card animation system started");
    
    // Main loop - just keep alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // Check every 10 seconds
    }
}
