# ESP32-C6 LVGL Animation Project - AI Agent Instructions

## Project Overview
This is an ESP32-C6 project that displays PNG image animations on a 172x320 ST7789 LCD using LVGL graphics library. The project converts PNG slideshow images into C arrays and cycles through them for smooth animation playback.

## Architecture & Key Components

### Hardware Platform
- **Target**: ESP32-C6-LCD-1.47 (RISC-V architecture, not Xtensa)
- **Flash**: 4MB
- **Partition Scheme**: No OTA (2MB APP/2MB FATFS)
- **Display**: ST7789 172x320 LCD via SPI interface
- **Critical Offset**: X-offset of 34 pixels required for proper display alignment
- **GPIO Pins**: MOSI=6, SCLK=7, CS=14, DC=15, RST=21, BCKL=22

### Component Structure
```
components/
├── driver/           # Custom display drivers with C++/C linkage
│   ├── LVGL_Driver.cpp  # LVGL integration (requires extern "C" wrapper)
│   ├── st7789_display.c # Low-level ST7789 driver
│   └── Display_ST7789.h # Hardware definitions
├── lvgl/            # LVGL graphics library (v8.3.11)
main/
├── main.c           # Application entry point
├── images.h         # Generated image data arrays
└── sd_card.h        # SD card interface (if used)
```

### Build System Essentials

#### Component Dependencies
- ESP32-C6 uses different header paths than older ESP32 chips in ESP-IDF v5.5
- Use `driver/gpio.h` and `driver/ledc.h` (NOT `esp_driver_gpio.h`)
- Component requirements order matters: `esp_timer lvgl driver` in main/CMakeLists.txt

#### Memory Management
- Custom partition table at `partitions.csv` with 4MB app partition (0x3F0000)
- Image data stored as RGB565 format C arrays in PSRAM-compatible buffers
- LVGL buffer size: `LVGL_WIDTH * LVGL_HEIGHT / 20`

## Critical Development Workflows

### Image Conversion Pipeline
```bash
# Convert PNG images to C arrays
python3 convert_images.py
# This generates main/images.h with slide_images[] array
```

### Build & Flash Commands
```bash
# Must source ESP-IDF environment first
source ~/Projects/ESP32/ESP-IDF/v5.5/esp-idf/export.sh
idf.py build
idf.py flash
```

### ESP32-C6 Specific Issues
1. **Header Compatibility**: Use legacy driver headers, not esp_driver_* variants
2. **C++/C Linkage**: LVGL_Driver.cpp functions need `extern "C"` wrappers for main.c
3. **Partition Size**: Default partition too small; custom partitions.csv required
4. **RISC-V Architecture**: Different from Xtensa ESP32 variants

## Project-Specific Patterns

### Image Data Management
- Images stored as `const uint16_t slide_X_data[]` arrays in `main/images.h`
- Each image has corresponding `image_info_t` structure with dimensions
- Animation cycles through `slide_images[]` array with configurable timing

### Display Initialization Sequence
```c
// Critical sequence in main.c:
init_spi_and_display();  // Hardware setup
display_st7789_image();  // First image
// Animation handled in FreeRTOS task
```

### LVGL Integration Points
- `Lvgl_Init()` in driver component initializes LVGL with ST7789 backend
- `st7789_flush_cb()` bridges LVGL rendering to hardware SPI
- Timer-based LVGL tick handling at 5ms intervals

## Common Debugging Patterns

### Build Failures
- **Undefined reference to Lvgl_Init**: Missing extern "C" linkage in LVGL_Driver.h
- **Header not found**: Use driver/gpio.h instead of esp_driver_gpio.h for ESP32-C6
- **Partition overflow**: Binary too large for default partition; check partitions.csv

### Display Issues
- **Offset display**: Verify OFFSET_X=34 in ST7789 configuration
- **No display output**: Check SPI pin assignments and initialization sequence
- **Color issues**: Ensure RGB565 format in image conversion

## Integration Dependencies
- **ESP-IDF v5.5**: Latest framework with ESP32-C6 RISC-V support
- **LVGL v8.3.11**: Embedded in components/lvgl with custom lv_conf.h
- **Python PIL**: Required for convert_images.py PNG processing
- **FreeRTOS**: Task-based animation timing and LVGL handling

When modifying this project, always verify component linkage, test image conversion pipeline, and ensure ESP32-C6 specific header compatibility.
