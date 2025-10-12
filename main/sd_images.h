#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PATH_LENGTH 256
#define MAX_FILENAME_LENGTH 64
#define INIT_IMAGES_DIR "/sdcard/init"
#define HI_IMAGES_DIR "/sdcard/hi"

typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    uint16_t* data;
    uint16_t width;
    uint16_t height;
    size_t size_bytes;
} sd_image_t;

typedef struct {
    sd_image_t* images;
    int count;
    int current_index;
} sd_animation_t;

// SD card initialization
esp_err_t sd_images_init(void);
void sd_images_deinit(void);

// Animation loading
esp_err_t load_animation_from_sd(const char* directory, sd_animation_t* animation);
void free_animation(sd_animation_t* animation);

// Image display functions
esp_err_t display_sd_image(sd_image_t* image);
esp_err_t display_raw_image_file(const char* filepath);

// Animation management
esp_err_t get_next_animation_frame(sd_animation_t* animation, sd_image_t** image);
void reset_animation(sd_animation_t* animation);

#ifdef __cplusplus
}
#endif
