#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "images/images.h"

static const char *TAG = "AGATA_SLIDESHOW";

// Pin definitions from working example
static int PIN_MOSI = 6;   
static int PIN_SCLK = 7;   
static int PIN_CS = 14;    
static int PIN_DC = 15;    
static int PIN_RST = 21;   
static int PIN_BCKL = 22;  

// Display parameters
#define LCD_WIDTH  172
#define LCD_HEIGHT 320
#define OFFSET_X 34
#define OFFSET_Y 0

// ST7789 Commands
#define ST7789_SWRST   0x01
#define ST7789_SLPOUT  0x11
#define ST7789_COLMOD  0x3A
#define ST7789_MADCTL  0x36
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_INVON   0x21
#define ST7789_NORON   0x13
#define ST7789_DISPON  0x29

// Animation timing constants
#define INIT_SLIDE_DELAY_MS 300
#define ANIMATION_SLIDE_DELAY_MS 400  
#define ANIMATION_LOOP_DELAY_MS 1000

static spi_device_handle_t spi;
#define OFFSET_Y    0

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
    t.user = (void*)0; // DC low for command
    spi_device_polling_transmit(spi, &t);
}

void st7789_write_data(const uint8_t *data, int len)
{
    if (len == 0) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void*)1; // DC high for data
    spi_device_polling_transmit(spi, &t);
}

void st7789_write_data_byte(uint8_t data)
{
    st7789_write_data(&data, 1);
}

void init_spi_and_display()
{
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST) | (1ULL << PIN_BCKL),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    
    // Initialize SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 64 * 2, // Increased buffer for stable transfers
        .flags = SPICOMMON_BUSFLAG_MASTER
    };
    
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8*1000*1000, // Reduced to 8MHz for maximum stability
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 1,
        .pre_cb = spi_pre_transfer_callback,
        .flags = SPI_DEVICE_HALFDUPLEX
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));
    
    // Setup PWM backlight control to reduce brightness and prevent overheating
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 64, // 25% brightness (0-255 range) to reduce heating
        .gpio_num = PIN_BCKL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    ESP_LOGI(TAG, "Backlight set to 25%% brightness to prevent overheating");
    
    // More robust display reset sequence
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(200));  // Longer reset pulse
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Longer recovery time
    
    // Complete display initialization from working example
    st7789_write_cmd(ST7789_SWRST);
    vTaskDelay(pdMS_TO_TICKS(200));  // Longer delay after software reset
    
    st7789_write_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(150));  // Longer sleep out delay
    
    // Color mode: 16-bit/pixel
    st7789_write_cmd(0x3A);
    st7789_write_data_byte(0x05);
    
    // Memory access control - Fix orientation and color order
    st7789_write_cmd(ST7789_MADCTL);
    st7789_write_data_byte(0x00);  // Normal orientation, RGB order
    
    // Porch control
    st7789_write_cmd(0xB2);
    st7789_write_data_byte(0x0C);
    st7789_write_data_byte(0x0C);
    st7789_write_data_byte(0x00);
    st7789_write_data_byte(0x33);
    st7789_write_data_byte(0x33);
    
    // Gate control
    st7789_write_cmd(0xB7);
    st7789_write_data_byte(0x35);
    
    // VCOM setting
    st7789_write_cmd(0xBB);
    st7789_write_data_byte(0x19);
    
    // LCM control
    st7789_write_cmd(0xC0);
    st7789_write_data_byte(0x2C);
    
    // VDV and VRH command enable
    st7789_write_cmd(0xC2);
    st7789_write_data_byte(0x01);
    
    // VRH set
    st7789_write_cmd(0xC3);
    st7789_write_data_byte(0x12);
    
    // VDV set
    st7789_write_cmd(0xC4);
    st7789_write_data_byte(0x20);
    
    // Frame rate control
    st7789_write_cmd(0xC6);
    st7789_write_data_byte(0x0F);
    
    // Power control 1
    st7789_write_cmd(0xD0);
    st7789_write_data_byte(0xA4);
    st7789_write_data_byte(0xA1);
    
    // Positive voltage gamma control
    st7789_write_cmd(0xE0);
    st7789_write_data_byte(0xD0);
    st7789_write_data_byte(0x04);
    st7789_write_data_byte(0x0D);
    st7789_write_data_byte(0x11);
    st7789_write_data_byte(0x13);
    st7789_write_data_byte(0x2B);
    st7789_write_data_byte(0x3F);
    st7789_write_data_byte(0x54);
    st7789_write_data_byte(0x4C);
    st7789_write_data_byte(0x18);
    st7789_write_data_byte(0x0D);
    st7789_write_data_byte(0x0B);
    st7789_write_data_byte(0x1F);
    st7789_write_data_byte(0x23);
    
    // Negative voltage gamma control
    st7789_write_cmd(0xE1);
    st7789_write_data_byte(0xD0);
    st7789_write_data_byte(0x04);
    st7789_write_data_byte(0x0C);
    st7789_write_data_byte(0x11);
    st7789_write_data_byte(0x13);
    st7789_write_data_byte(0x2C);
    st7789_write_data_byte(0x3F);
    st7789_write_data_byte(0x44);
    st7789_write_data_byte(0x51);
    st7789_write_data_byte(0x2F);
    st7789_write_data_byte(0x1F);
    st7789_write_data_byte(0x1F);
    st7789_write_data_byte(0x20);
    st7789_write_data_byte(0x23);
    
    // Inversion on
    st7789_write_cmd(0x21);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Normal display mode on
    st7789_write_cmd(0x13);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Display on
    st7789_write_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(120));
}

void clear_display()
{
    ESP_LOGI(TAG, "Clearing display with black background");
    
    // Set full screen window
    st7789_write_cmd(ST7789_CASET);
    st7789_write_data_byte((OFFSET_X) >> 8);
    st7789_write_data_byte((OFFSET_X) & 0xFF);
    st7789_write_data_byte((OFFSET_X + LCD_WIDTH - 1) >> 8);
    st7789_write_data_byte((OFFSET_X + LCD_WIDTH - 1) & 0xFF);
    
    st7789_write_cmd(ST7789_RASET);
    st7789_write_data_byte((OFFSET_Y) >> 8);
    st7789_write_data_byte((OFFSET_Y) & 0xFF);
    st7789_write_data_byte((OFFSET_Y + LCD_HEIGHT - 1) >> 8);
    st7789_write_data_byte((OFFSET_Y + LCD_HEIGHT - 1) & 0xFF);
    
    // Start memory write
    st7789_write_cmd(ST7789_RAMWR);
    
    // Send black pixels (RGB565 black = 0x0000)
    uint8_t black_line[344]; // 172 pixels * 2 bytes = 344 bytes per line
    memset(black_line, 0x00, sizeof(black_line)); // All zeros = black
    
    // Fill screen line by line
    for (int y = 0; y < LCD_HEIGHT; y++) {
        st7789_write_data(black_line, sizeof(black_line));
        if ((y % 50) == 0) {
            esp_rom_delay_us(100); // Small delay every 50 lines
        }
    }
    
    ESP_LOGI(TAG, "Display cleared to black");
}

void display_test_pattern()
{
    ESP_LOGI(TAG, "Displaying test pattern with buffering");
    
    // Set full screen area
    st7789_write_cmd(ST7789_CASET);
    st7789_write_data_byte((OFFSET_X) >> 8);
    st7789_write_data_byte((OFFSET_X) & 0xFF);
    st7789_write_data_byte((OFFSET_X + LCD_WIDTH - 1) >> 8);
    st7789_write_data_byte((OFFSET_X + LCD_WIDTH - 1) & 0xFF);
    
    st7789_write_cmd(ST7789_RASET);
    st7789_write_data_byte((OFFSET_Y) >> 8);
    st7789_write_data_byte((OFFSET_Y) & 0xFF);
    st7789_write_data_byte((OFFSET_Y + LCD_HEIGHT - 1) >> 8);
    st7789_write_data_byte((OFFSET_Y + LCD_HEIGHT - 1) & 0xFF);
    
    // Memory write
    st7789_write_cmd(ST7789_RAMWR);
    
    // Use buffered approach for test pattern too
    int total_pixels = LCD_WIDTH * LCD_HEIGHT;
    uint8_t* display_buffer = malloc(total_pixels * 2);
    if (display_buffer) {
        ESP_LOGI(TAG, "Using buffered test pattern (allocated %d bytes)", total_pixels * 2);
        
        // Create colored stripes in buffer: red, green, blue
        for (int y = 0; y < LCD_HEIGHT; y++) {
            for (int x = 0; x < LCD_WIDTH; x++) {
                uint16_t color;
                if (y < 107) {
                    color = 0xF800; // Red
                } else if (y < 214) {
                    color = 0x07E0; // Green  
                } else {
                    color = 0x001F; // Blue
                }
                
                int pixel_index = (y * LCD_WIDTH + x);
                display_buffer[pixel_index * 2] = color & 0xFF;           // Low byte first
                display_buffer[pixel_index * 2 + 1] = (color >> 8) & 0xFF; // High byte second
            }
        }
        
        // Send buffer in chunks to avoid SPI transfer size limits
        const int chunk_size = 4096; // Safe chunk size for ESP32
        int bytes_to_send = total_pixels * 2;
        int bytes_sent = 0;
        
        while (bytes_sent < bytes_to_send) {
            int current_chunk = (bytes_to_send - bytes_sent) > chunk_size ? chunk_size : (bytes_to_send - bytes_sent);
            st7789_write_data(display_buffer + bytes_sent, current_chunk);
            bytes_sent += current_chunk;
        }
        free(display_buffer);
        
        ESP_LOGI(TAG, "Buffered test pattern completed");
    } else {
        ESP_LOGW(TAG, "Buffer allocation failed for test pattern, using pixel-by-pixel");
        
        // Fallback to pixel-by-pixel
        for (int y = 0; y < LCD_HEIGHT; y++) {
            for (int x = 0; x < LCD_WIDTH; x++) {
                uint16_t color;
                if (y < 107) {
                    color = 0xF800; // Red
                } else if (y < 214) {
                    color = 0x07E0; // Green  
                } else {
                    color = 0x001F; // Blue
                }
                uint8_t pixel_bytes[2] = {color & 0xFF, (color >> 8) & 0xFF};
                st7789_write_data(pixel_bytes, 2);
            }
        }
        ESP_LOGI(TAG, "Pixel-by-pixel test pattern completed");
    }
}

void display_slide_image(int slide_index)
{
    if (slide_index < 0 || slide_index >= NUM_HI_SLIDES) {
        ESP_LOGE(TAG, "Invalid slide index: %d", slide_index);
        return;
    }
    
    const image_info_t *img_info = &hi_images[slide_index];
    int total_pixels = img_info->width * img_info->height;
    
    ESP_LOGI(TAG, "Displaying hi slide %d: %dx%d (%d pixels)", 
             slide_index, img_info->width, img_info->height, total_pixels);
    
    // Center the 170px image on 172px screen: (172-170)/2 = 1 pixel on each side
    int center_x = OFFSET_X + 1;  // OFFSET_X (34) + centering (1) = 35
    int center_y = OFFSET_Y;      // No vertical centering needed (already matches)
    
    ESP_LOGI(TAG, "Centering at (%d,%d) for %dx%d image", center_x, center_y, img_info->width, img_info->height);
    
    // Set column address (X coordinates) - CENTER THE IMAGE
    st7789_write_cmd(ST7789_CASET);
    st7789_write_data_byte((center_x) >> 8);
    st7789_write_data_byte((center_x) & 0xFF);
    st7789_write_data_byte((center_x + img_info->width - 1) >> 8);
    st7789_write_data_byte((center_x + img_info->width - 1) & 0xFF);
    
    // Set page address (Y coordinates)
    st7789_write_cmd(ST7789_RASET);
    st7789_write_data_byte((center_y) >> 8);
    st7789_write_data_byte((center_y) & 0xFF);
    st7789_write_data_byte((center_y + img_info->height - 1) >> 8);
    st7789_write_data_byte((center_y + img_info->height - 1) & 0xFF);
    
    // Start memory write
    st7789_write_cmd(ST7789_RAMWR);
    
    // Use line-by-line method to prevent diagonal artifacts
    const uint16_t* pixel_data = img_info->data;
    const int line_pixels = img_info->width;
    const int bytes_per_line = line_pixels * 2;
    
    // Allocate 4-byte aligned buffer for one line to prevent alignment issues
    uint8_t* line_buffer = heap_caps_aligned_alloc(4, bytes_per_line, MALLOC_CAP_DMA);
    if (line_buffer) {
        ESP_LOGI(TAG, "Using line-by-line method with DMA-aligned buffer");
        
        for (int y = 0; y < img_info->height; y++) {
            // Prepare one line with proper byte order and alignment
            for (int x = 0; x < line_pixels; x++) {
                uint16_t pixel = pixel_data[y * line_pixels + x];
                // ST7789 RGB565 format: send high byte first
                line_buffer[x * 2] = (pixel >> 8) & 0xFF;     // High byte (RRRRRGGG)
                line_buffer[x * 2 + 1] = pixel & 0xFF;        // Low byte (GGGBBBBB)
            }
            
            // Send the line with a small delay to prevent timing issues
            st7789_write_data(line_buffer, bytes_per_line);
            
            // Micro-delay every few lines to prevent SPI overrun
            if ((y % 10) == 0) {
                esp_rom_delay_us(100); // 100 microsecond delay every 10 lines
            }
        }
        
        heap_caps_free(line_buffer);
        ESP_LOGI(TAG, "Line-by-line centered display completed");
    } else {
        ESP_LOGW(TAG, "DMA buffer allocation failed, using pixel-by-pixel method");
        
        // Fallback: pixel-by-pixel with correct byte order
        for (int i = 0; i < total_pixels; i++) {
            uint16_t pixel = pixel_data[i];
            uint8_t pixel_bytes[2] = {(pixel >> 8) & 0xFF, pixel & 0xFF};
            st7789_write_data(pixel_bytes, 2);
            
            // Add micro-delay every 100 pixels to prevent timing issues
            if ((i % 100) == 0) {
                esp_rom_delay_us(50);
            }
        }
        
        ESP_LOGI(TAG, "Pixel-by-pixel centered display completed");
    }
}

void display_init_image(int slide_index)
{
    if (slide_index < 0 || slide_index >= NUM_INIT_SLIDES) {
        ESP_LOGE(TAG, "Invalid init slide index: %d", slide_index);
        return;
    }
    
    const image_info_t *img_info = &init_images[slide_index];
    int total_pixels = img_info->width * img_info->height;
    
    ESP_LOGI(TAG, "Displaying init slide %d: %dx%d (%d pixels)", 
             slide_index, img_info->width, img_info->height, total_pixels);
    
    // Center the image on screen
    int center_x = OFFSET_X + ((172 - img_info->width) / 2);  // Dynamic centering
    int center_y = OFFSET_Y;
    
    ESP_LOGI(TAG, "Centering init at (%d,%d) for %dx%d image", center_x, center_y, img_info->width, img_info->height);
    
    // Set display window - CENTERED
    st7789_write_cmd(ST7789_CASET);
    st7789_write_data_byte((center_x) >> 8);
    st7789_write_data_byte((center_x) & 0xFF);
    st7789_write_data_byte((center_x + img_info->width - 1) >> 8);
    st7789_write_data_byte((center_x + img_info->width - 1) & 0xFF);
    
    st7789_write_cmd(ST7789_RASET);
    st7789_write_data_byte((center_y) >> 8);
    st7789_write_data_byte((center_y) & 0xFF);
    st7789_write_data_byte((center_y + img_info->height - 1) >> 8);
    st7789_write_data_byte((center_y + img_info->height - 1) & 0xFF);
    
    // Start memory write
    st7789_write_cmd(ST7789_RAMWR);
    
    // Use line-by-line method for better stability
    const uint16_t* pixel_data = img_info->data;
    const int line_pixels = img_info->width;
    const int bytes_per_line = line_pixels * 2;
    
    // Use DMA-aligned buffer for better performance
    uint8_t* line_buffer = heap_caps_aligned_alloc(4, bytes_per_line, MALLOC_CAP_DMA);
    if (line_buffer) {
        ESP_LOGI(TAG, "Using line-by-line init display with DMA buffer");
        
        for (int y = 0; y < img_info->height; y++) {
            // Prepare line with correct byte order
            for (int x = 0; x < line_pixels; x++) {
                uint16_t pixel = pixel_data[y * line_pixels + x];
                line_buffer[x * 2] = (pixel >> 8) & 0xFF;     // High byte first
                line_buffer[x * 2 + 1] = pixel & 0xFF;        // Low byte second
            }
            
            // Send line
            st7789_write_data(line_buffer, bytes_per_line);
            
            // Micro-delay every few lines
            if ((y % 10) == 0) {
                esp_rom_delay_us(100);
            }
        }
        
        heap_caps_free(line_buffer);
        ESP_LOGI(TAG, "Line-by-line init display completed");
    } else {
        ESP_LOGW(TAG, "DMA buffer allocation failed for init, using pixel-by-pixel");
        
        // Fallback method
        const uint16_t* pixel_data = img_info->data;
        for (int i = 0; i < total_pixels; i++) {
            uint16_t pixel = pixel_data[i];
            uint8_t pixel_bytes[2] = {(pixel >> 8) & 0xFF, pixel & 0xFF};
            st7789_write_data(pixel_bytes, 2);
            
            // Add micro-delay every 100 pixels
            if ((i % 100) == 0) {
                esp_rom_delay_us(50);
            }
        }
        
        ESP_LOGI(TAG, "Pixel-by-pixel init display completed");
    }
}

static void animation_task(void *pvParameter)
{
    bool init_completed = false;
    int current_slide = 0;
    
    ESP_LOGI(TAG, "Starting dual animation system - init (%d frames) then hi loop (%d frames)", 
             NUM_INIT_SLIDES, NUM_HI_SLIDES);
    
    // Clear display to black background first
    clear_display();
    vTaskDelay(pdMS_TO_TICKS(300)); // Brief pause after clearing
    
    while (1) {
        if (!init_completed) {
            // Play init animation once
            ESP_LOGI(TAG, "Playing init animation - frame %d/%d", current_slide + 1, NUM_INIT_SLIDES);
            
            display_init_image(current_slide);
            
            current_slide++;
            if (current_slide >= NUM_INIT_SLIDES) {
                ESP_LOGI(TAG, "Init animation completed, switching to hi loop");
                init_completed = true;
                current_slide = 0;
                vTaskDelay(pdMS_TO_TICKS(ANIMATION_LOOP_DELAY_MS)); // Pause before starting hi loop
            } else {
                vTaskDelay(pdMS_TO_TICKS(INIT_SLIDE_DELAY_MS));
            }
        } else {
            // Play hi animation in continuous loop
            ESP_LOGI(TAG, "Playing hi loop - frame %d/%d", current_slide + 1, NUM_HI_SLIDES);
            
            display_slide_image(current_slide);
            
            current_slide++;
            if (current_slide >= NUM_HI_SLIDES) {
                current_slide = 0;
                ESP_LOGI(TAG, "Hi animation loop completed, restarting...");
                vTaskDelay(pdMS_TO_TICKS(ANIMATION_LOOP_DELAY_MS));
            } else {
                vTaskDelay(pdMS_TO_TICKS(ANIMATION_SLIDE_DELAY_MS));
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 AGATA Robot Slideshow Starting");
    
    // Initialize SPI and display with working configuration
    init_spi_and_display();
    
    ESP_LOGI(TAG, "Display initialized, preparing for slideshow");
    
    // Clear any garbage/noise from display first
    clear_display();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Display cleared");
    
    ESP_LOGI(TAG, "starting dual animation system: init (%d frames) + hi loop (%d frames)", 
             NUM_INIT_SLIDES, NUM_HI_SLIDES);
    
    // Start animation task
    xTaskCreate(
        animation_task,           // Task function
        "dual_animation_task",    // Task name
        8192,                     // Increased stack size
        NULL,                     // Parameters
        5,                        // Priority
        NULL                      // Task handle
    );
    
    ESP_LOGI(TAG, "Dual animation system started - init: %d frames, hi loop: %d frames", 
             NUM_INIT_SLIDES, NUM_HI_SLIDES);
    
    // Main loop - just keep alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // Check every 10 seconds
    }
}