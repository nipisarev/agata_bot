#include "sd_card.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SD_CARD";
static bool sd_initialized = false;

bool sd_card_init(void)
{
    if (sd_initialized) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing SD card");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";

    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 20000;  // 20MHz

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_NUM_11,     // MOSI
        .miso_io_num = GPIO_NUM_13,     // MISO  
        .sclk_io_num = GPIO_NUM_12,     // SCLK
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return false;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_10;    // CS
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    sdmmc_card_print_info(stdout, card);
    sd_initialized = true;
    return true;
}

bool load_image_from_sd(const char* filename, image_info_t* image_info)
{
    if (!sd_initialized) {
        ESP_LOGE(TAG, "SD card not initialized");
        return false;
    }

    char full_path[64];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", filename);

    FILE* file = fopen(full_path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        return false;
    }

    // Read header (width, height)
    if (fread(&image_info->width, 2, 1, file) != 1 || 
        fread(&image_info->height, 2, 1, file) != 1) {
        ESP_LOGE(TAG, "Failed to read image header: %s", filename);
        fclose(file);
        return false;
    }

    size_t pixel_count = image_info->width * image_info->height;
    size_t data_size = pixel_count * sizeof(uint16_t);

    ESP_LOGI(TAG, "Loading image %s: %dx%d (%zu bytes)", 
             filename, image_info->width, image_info->height, data_size);

    // Allocate memory for image data
    image_info->data = malloc(data_size);
    if (!image_info->data) {
        ESP_LOGE(TAG, "Failed to allocate memory for image data");
        fclose(file);
        return false;
    }

    // Read pixel data
    if (fread(image_info->data, sizeof(uint16_t), pixel_count, file) != pixel_count) {
        ESP_LOGE(TAG, "Failed to read image data: %s", filename);
        free(image_info->data);
        image_info->data = NULL;
        fclose(file);
        return false;
    }

    fclose(file);
    ESP_LOGI(TAG, "Successfully loaded image: %s", filename);
    return true;
}

void free_image(image_info_t* image_info)
{
    if (image_info && image_info->data) {
        free(image_info->data);
        image_info->data = NULL;
        image_info->width = 0;
        image_info->height = 0;
    }
}