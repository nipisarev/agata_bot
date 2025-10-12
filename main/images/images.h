#pragma once
#include <stdint.h>

typedef struct {
    const uint16_t* data;
    uint16_t width;
    uint16_t height;
} image_info_t;

extern const uint16_t init_1_width;
extern const uint16_t init_1_height;
extern const uint16_t init_1_data[];
extern const uint16_t init_2_width;
extern const uint16_t init_2_height;
extern const uint16_t init_2_data[];
extern const uint16_t init_3_width;
extern const uint16_t init_3_height;
extern const uint16_t init_3_data[];
extern const uint16_t init_4_width;
extern const uint16_t init_4_height;
extern const uint16_t init_4_data[];
extern const uint16_t init_5_width;
extern const uint16_t init_5_height;
extern const uint16_t init_5_data[];
extern const uint16_t init_6_width;
extern const uint16_t init_6_height;
extern const uint16_t init_6_data[];
extern const uint16_t init_7_width;
extern const uint16_t init_7_height;
extern const uint16_t init_7_data[];
extern const uint16_t init_8_width;
extern const uint16_t init_8_height;
extern const uint16_t init_8_data[];
extern const uint16_t init_9_width;
extern const uint16_t init_9_height;
extern const uint16_t init_9_data[];
extern const uint16_t init_10_width;
extern const uint16_t init_10_height;
extern const uint16_t init_10_data[];
extern const uint16_t init_11_width;
extern const uint16_t init_11_height;
extern const uint16_t init_11_data[];
extern const uint16_t init_12_width;
extern const uint16_t init_12_height;
extern const uint16_t init_12_data[];
extern const uint16_t init_13_width;
extern const uint16_t init_13_height;
extern const uint16_t init_13_data[];
extern const uint16_t init_14_width;
extern const uint16_t init_14_height;
extern const uint16_t init_14_data[];
extern const uint16_t init_15_width;
extern const uint16_t init_15_height;
extern const uint16_t init_15_data[];
extern const uint16_t init_16_width;
extern const uint16_t init_16_height;
extern const uint16_t init_16_data[];
extern const uint16_t init_17_width;
extern const uint16_t init_17_height;
extern const uint16_t init_17_data[];
extern const uint16_t init_18_width;
extern const uint16_t init_18_height;
extern const uint16_t init_18_data[];

#define NUM_INIT_SLIDES 18
extern const image_info_t init_images[NUM_INIT_SLIDES];

extern const uint16_t hi_1_width;
extern const uint16_t hi_1_height;
extern const uint16_t hi_1_data[];
extern const uint16_t hi_2_width;
extern const uint16_t hi_2_height;
extern const uint16_t hi_2_data[];
extern const uint16_t hi_3_width;
extern const uint16_t hi_3_height;
extern const uint16_t hi_3_data[];
extern const uint16_t hi_4_width;
extern const uint16_t hi_4_height;
extern const uint16_t hi_4_data[];
extern const uint16_t hi_5_width;
extern const uint16_t hi_5_height;
extern const uint16_t hi_5_data[];
extern const uint16_t hi_6_width;
extern const uint16_t hi_6_height;
extern const uint16_t hi_6_data[];
extern const uint16_t hi_7_width;
extern const uint16_t hi_7_height;
extern const uint16_t hi_7_data[];
extern const uint16_t hi_8_width;
extern const uint16_t hi_8_height;
extern const uint16_t hi_8_data[];
extern const uint16_t hi_9_width;
extern const uint16_t hi_9_height;
extern const uint16_t hi_9_data[];
extern const uint16_t hi_10_width;
extern const uint16_t hi_10_height;
extern const uint16_t hi_10_data[];
extern const uint16_t hi_11_width;
extern const uint16_t hi_11_height;
extern const uint16_t hi_11_data[];

#define NUM_HI_SLIDES 11
extern const image_info_t hi_images[NUM_HI_SLIDES];

#define NUM_SLIDES 11
extern const image_info_t slide_images[NUM_SLIDES];
