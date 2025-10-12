#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t* data;
    uint16_t width;
    uint16_t height;
} image_info_t;

// Initialize SD card
bool sd_card_init(void);

// Load image from SD card
bool load_image_from_sd(const char* filename, image_info_t* image_info);

// Free loaded image
void free_image(image_info_t* image_info);

// Image counts
#define NUM_SLIDES 11
#define NUM_HI_SLIDES 11