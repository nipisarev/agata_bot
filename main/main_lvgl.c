#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "images/images.h"
#include "st7789_display.h"

static const char *TAG = "AGATA_SLIDESHOW";

// Animation timing constants
#define INIT_SLIDE_DELAY_MS 300
#define ANIMATION_SLIDE_DELAY_MS 400  
#define ANIMATION_LOOP_DELAY_MS 1000

// Display initialization function
void init_spi_and_display()
{
    ESP_LOGI(TAG, "Initializing ST7789 display driver");
    
    // Initialize ST7789 display
    st7789_init();
    
    // Set backlight
    display_set_brightness(80); // 80% brightness
    
    ESP_LOGI(TAG, "Display initialization complete");
}

// Display slide image using ST7789 flush callback
void display_slide_image(int slide_index)
{
    if (slide_index < 0 || slide_index >= NUM_HI_SLIDES) {
        ESP_LOGE(TAG, "Invalid slide index: %d", slide_index);
        return;
    }
    
    const image_info_t *img = &hi_images[slide_index];
    ESP_LOGI(TAG, "Displaying hi slide %d (%dx%d)", slide_index, img->width, img->height);
    
    // Create area for the image (centered on 172px screen)
    lv_area_t area;
    area.x1 = (172 - img->width) / 2;  // Center 170px image on 172px screen
    area.y1 = 0;
    area.x2 = area.x1 + img->width - 1;
    area.y2 = img->height - 1;
    
    // Use ST7789 flush callback to render the image
    st7789_flush_cb(NULL, &area, (lv_color16_t*)img->data);
}

// Display init image using ST7789 flush callback
void display_init_image(int slide_index)
{
    if (slide_index < 0 || slide_index >= NUM_INIT_SLIDES) {
        ESP_LOGE(TAG, "Invalid init slide index: %d", slide_index);
        return;
    }
    
    const image_info_t *img = &init_images[slide_index];
    ESP_LOGI(TAG, "Displaying init slide %d (%dx%d)", slide_index, img->width, img->height);
    
    // Create area for the image (centered on 172px screen)
    lv_area_t area;
    area.x1 = (172 - img->width) / 2;  // Center 170px image on 172px screen  
    area.y1 = 0;
    area.x2 = area.x1 + img->width - 1;
    area.y2 = img->height - 1;
    
    // Use ST7789 flush callback to render the image
    st7789_flush_cb(NULL, &area, (lv_color16_t*)img->data);
}

// Clear display to black
void clear_display()
{
    ESP_LOGI(TAG, "Clearing display to black");
    
    // Create full screen area
    lv_area_t area;
    area.x1 = 0;
    area.y1 = 0; 
    area.x2 = 171; // 172 - 1
    area.y2 = 319; // 320 - 1
    
    // Create black buffer (smaller for memory efficiency)
    static lv_color16_t black_buffer[172 * 32]; // 32 lines at a time
    memset(black_buffer, 0, sizeof(black_buffer));
    
    // Clear screen in chunks to save memory
    for (int y = 0; y < 320; y += 32) {
        lv_area_t chunk_area;
        chunk_area.x1 = 0;
        chunk_area.y1 = y;
        chunk_area.x2 = 171;
        chunk_area.y2 = (y + 31 < 319) ? y + 31 : 319;
        
        st7789_flush_cb(NULL, &chunk_area, black_buffer);
    }
    
    ESP_LOGI(TAG, "Display cleared");
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
    ESP_LOGI(TAG, "ESP32-C6 Agata Robot Animation Starting");
    
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialize display
    init_spi_and_display();
    
    ESP_LOGI(TAG, "Creating animation task");
    
    // Create animation task
    xTaskCreate(animation_task, "animation_task", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Setup complete. Animation task started.");
}
