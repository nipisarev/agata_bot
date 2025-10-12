#include "sd_images.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_heap_caps.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "SD_IMAGES";

// SD card configuration for ESP32-C6-LCD-1.47
#define PIN_NUM_MISO  5
#define PIN_NUM_MOSI  4  
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    8

static bool sd_card_mounted = false;
static sdmmc_card_t *card = NULL;

// Natural sorting comparison function
static int natural_compare(const void *a, const void *b) {
    const char *str_a = (const char *)a;
    const char *str_b = (const char *)b;
    
    while (*str_a && *str_b) {
        if (isdigit(*str_a) && isdigit(*str_b)) {
            // Extract numbers for proper comparison
            int num_a = 0, num_b = 0;
            while (isdigit(*str_a)) {
                num_a = num_a * 10 + (*str_a - '0');
                str_a++;
            }
            while (isdigit(*str_b)) {
                num_b = num_b * 10 + (*str_b - '0');
                str_b++;
            }
            if (num_a != num_b) {
                return num_a - num_b;
            }
        } else {
            if (*str_a != *str_b) {
                return *str_a - *str_b;
            }
            str_a++;
            str_b++;
        }
    }
    return *str_a - *str_b;
}

esp_err_t sd_images_init(void) {
    if (sd_card_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card for image loading");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 16,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting SD card filesystem");
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    
    // Print card info
    sdmmc_card_print_info(stdout, card);
    
    sd_card_mounted = true;
    return ESP_OK;
}

void sd_images_deinit(void) {
    if (sd_card_mounted) {
        esp_vfs_fat_sdcard_unmount("/sdcard", card);
        spi_bus_free(SPI2_HOST);
        sd_card_mounted = false;
        ESP_LOGI(TAG, "SD card unmounted");
    }
}

static esp_err_t load_raw_image_from_file(const char* filepath, sd_image_t* image) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return ESP_FAIL;
    }

    // Read image header (width, height)
    uint16_t width, height;
    if (fread(&width, sizeof(uint16_t), 1, file) != 1 ||
        fread(&height, sizeof(uint16_t), 1, file) != 1) {
        ESP_LOGE(TAG, "Failed to read image header from %s", filepath);
        fclose(file);
        return ESP_FAIL;
    }

    size_t pixel_count = width * height;
    size_t data_size = pixel_count * sizeof(uint16_t);

    // Allocate memory for image data in PSRAM if available
    uint16_t* data = (uint16_t*)heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!data) {
        // Fallback to internal RAM
        data = (uint16_t*)malloc(data_size);
        if (!data) {
            ESP_LOGE(TAG, "Failed to allocate memory for image %s (%zu bytes)", filepath, data_size);
            fclose(file);
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGW(TAG, "Using internal RAM for image %s", filepath);
    }

    // Read pixel data
    size_t bytes_read = fread(data, 1, data_size, file);
    fclose(file);

    if (bytes_read != data_size) {
        ESP_LOGE(TAG, "Failed to read complete image data from %s", filepath);
        free(data);
        return ESP_FAIL;
    }

    // Extract filename without path
    const char* filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    strncpy(image->filename, filename, MAX_FILENAME_LENGTH - 1);
    image->filename[MAX_FILENAME_LENGTH - 1] = '\0';

    image->data = data;
    image->width = width;
    image->height = height;
    image->size_bytes = data_size;

    ESP_LOGI(TAG, "Loaded image %s: %dx%d (%zu bytes)", image->filename, width, height, data_size);
    return ESP_OK;
}

esp_err_t load_animation_from_sd(const char* directory, sd_animation_t* animation) {
    ESP_LOGI(TAG, "Loading animation from %s", directory);

    DIR* dir = opendir(directory);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory %s", directory);
        return ESP_FAIL;
    }

    // First pass: count .raw files
    int file_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".raw")) {
            file_count++;
        }
    }
    rewinddir(dir);

    if (file_count == 0) {
        ESP_LOGW(TAG, "No .raw files found in %s", directory);
        closedir(dir);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Found %d image files", file_count);

    // Collect filenames for sorting
    char filenames[file_count][MAX_FILENAME_LENGTH];
    int idx = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".raw")) {
            strncpy(filenames[idx], entry->d_name, MAX_FILENAME_LENGTH - 1);
            filenames[idx][MAX_FILENAME_LENGTH - 1] = '\0';
            idx++;
        }
    }
    closedir(dir);

    // Sort filenames naturally
    qsort(filenames, file_count, MAX_FILENAME_LENGTH, natural_compare);

    // Allocate memory for images
    sd_image_t* images = (sd_image_t*)calloc(file_count, sizeof(sd_image_t));
    if (!images) {
        ESP_LOGE(TAG, "Failed to allocate memory for animation images");
        return ESP_ERR_NO_MEM;
    }

    // Load images
    int loaded_count = 0;
    for (int i = 0; i < file_count; i++) {
        char filepath[MAX_PATH_LENGTH];
        snprintf(filepath, sizeof(filepath), "%s/%s", directory, filenames[i]);

        if (load_raw_image_from_file(filepath, &images[loaded_count]) == ESP_OK) {
            loaded_count++;
        } else {
            ESP_LOGW(TAG, "Failed to load image %s", filepath);
        }
    }

    if (loaded_count == 0) {
        ESP_LOGE(TAG, "No images loaded from %s", directory);
        free(images);
        return ESP_FAIL;
    }

    animation->images = images;
    animation->count = loaded_count;
    animation->current_index = 0;

    ESP_LOGI(TAG, "Successfully loaded %d images for animation", loaded_count);
    return ESP_OK;
}

void free_animation(sd_animation_t* animation) {
    if (animation && animation->images) {
        for (int i = 0; i < animation->count; i++) {
            if (animation->images[i].data) {
                free(animation->images[i].data);
                animation->images[i].data = NULL;
            }
        }
        free(animation->images);
        animation->images = NULL;
        animation->count = 0;
        animation->current_index = 0;
    }
}

esp_err_t get_next_animation_frame(sd_animation_t* animation, sd_image_t** image) {
    if (!animation || !animation->images || animation->count == 0) {
        return ESP_FAIL;
    }

    *image = &animation->images[animation->current_index];
    animation->current_index = (animation->current_index + 1) % animation->count;
    
    return ESP_OK;
}

void reset_animation(sd_animation_t* animation) {
    if (animation) {
        animation->current_index = 0;
    }
}

esp_err_t display_sd_image(sd_image_t* image) {
    if (!image || !image->data) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Displaying SD image %s (%dx%d)", image->filename, image->width, image->height);
    
    // This will be called by the main display function
    // The actual display logic will use the existing ST7789 functions
    return ESP_OK;
}

esp_err_t display_raw_image_file(const char* filepath) {
    sd_image_t temp_image = {0};
    esp_err_t ret = load_raw_image_from_file(filepath, &temp_image);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_sd_image(&temp_image);
    
    // Clean up temporary image
    if (temp_image.data) {
        free(temp_image.data);
    }

    return ret;
}
