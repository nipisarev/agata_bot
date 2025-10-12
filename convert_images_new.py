#!/usr/bin/env python3
"""
Convert PNG images to C arrays for LVGL
This script converts PNG images to C arrays suitable for LVGL
Handles both init and hi animations with proper sorting
"""

import os
import sys
import re
from PIL import Image
import struct

def convert_image_to_c_array(image_path, output_path, var_name):
    """Convert a PNG image to LVGL C array format"""
    
    # Open and convert image
    img = Image.open(image_path)
    
    # Convert to RGB if necessary
    if img.mode != 'RGB':
        img = img.convert('RGB')
    
    width, height = img.size
    
    # Convert to RGB565 format
    pixels = []
    for y in range(height):
        for x in range(width):
            r, g, b = img.getpixel((x, y))
            
            # Convert to RGB565 format
            r565 = (r >> 3) << 11
            g565 = (g >> 2) << 5
            b565 = b >> 3
            rgb565 = r565 | g565 | b565
            
            pixels.append(rgb565)
    
    # Write to C file
    with open(output_path, 'w') as f:
        f.write('#include <stdint.h>\n\n')
        f.write(f'const uint16_t {var_name}_width = {width};\n')
        f.write(f'const uint16_t {var_name}_height = {height};\n\n')
        f.write(f'const uint16_t {var_name}_data[] = {{\n')
        
        for i, pixel in enumerate(pixels):
            if i % 16 == 0:
                f.write('    ')
            f.write(f'0x{pixel:04x}')
            if i < len(pixels) - 1:
                f.write(', ')
            if i % 16 == 15:
                f.write('\n')
        
        if len(pixels) % 16 != 0:
            f.write('\n')
        
        f.write('};\n')
    
    print(f'Converted {image_path} -> {output_path} ({width}x{height})')

def natural_sort_key(text):
    """
    Natural sorting key function to properly sort filenames with numbers
    e.g., init_1.png, init_2.png, ..., init_10.png, init_11.png
    """
    def atoi(text):
        return int(text) if text.isdigit() else text

    return [atoi(c) for c in re.split(r'(\d+)', text)]

def process_animation_folder(folder_path, animation_name, output_dir):
    """Process all PNG files in an animation folder"""
    if not os.path.exists(folder_path):
        print(f"Warning: Folder {folder_path} does not exist")
        return []
    
    # Find all PNG files
    png_files = [f for f in os.listdir(folder_path) if f.lower().endswith('.png')]
    
    # Sort files naturally (1, 2, 3, ..., 10, 11 instead of 1, 10, 11, 2, 3...)
    png_files.sort(key=natural_sort_key)
    
    print(f"\nProcessing {animation_name} animation:")
    print(f"Found {len(png_files)} PNG files: {png_files}")
    
    converted_files = []
    
    for i, filename in enumerate(png_files):
        # Extract number from filename for consistent variable naming
        number_match = re.search(r'(\d+)', filename)
        if number_match:
            frame_num = int(number_match.group(1))
        else:
            frame_num = i + 1
            
        image_path = os.path.join(folder_path, filename)
        var_name = f'{animation_name}_{frame_num}'
        output_path = os.path.join(output_dir, f'{var_name}.c')
        
        print(f"Converting {filename} -> {var_name}")
        
        try:
            convert_image_to_c_array(image_path, output_path, var_name)
            converted_files.append((var_name, filename))
        except Exception as e:
            print(f"Error converting {filename}: {e}")
    
    return converted_files

def main():
    """Main conversion function"""
    # Setup paths
    project_root = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(project_root, 'assets')
    output_dir = os.path.join(project_root, 'main', 'images')
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    print("ESP32-C6 LVGL Image Converter")
    print("="*40)
    
    # Process both animations
    init_converted_files = process_animation_folder(
        os.path.join(assets_dir, 'init'), 
        'init', 
        output_dir
    )
    
    hi_converted_files = process_animation_folder(
        os.path.join(assets_dir, 'hi'), 
        'hi', 
        output_dir
    )
    
    if not init_converted_files and not hi_converted_files:
        print("No images found to convert!")
        return
    
    # Generate header file
    header_path = os.path.join(output_dir, 'images.h')
    with open(header_path, 'w') as f:
        f.write('#pragma once\n')
        f.write('#include <stdint.h>\n\n')
        
        # Image info structure
        f.write('typedef struct {\n')
        f.write('    const uint16_t* data;\n')
        f.write('    uint16_t width;\n')
        f.write('    uint16_t height;\n')
        f.write('} image_info_t;\n\n')
        
        # Init animation declarations
        if init_converted_files:
            for var_name, _ in init_converted_files:
                f.write(f'extern const uint16_t {var_name}_width;\n')
                f.write(f'extern const uint16_t {var_name}_height;\n')
                f.write(f'extern const uint16_t {var_name}_data[];\n')
            f.write(f'\n#define NUM_INIT_SLIDES {len(init_converted_files)}\n')
            f.write('extern const image_info_t init_images[NUM_INIT_SLIDES];\n\n')
        
        # Hi animation declarations  
        if hi_converted_files:
            for var_name, _ in hi_converted_files:
                f.write(f'extern const uint16_t {var_name}_width;\n')
                f.write(f'extern const uint16_t {var_name}_height;\n')
                f.write(f'extern const uint16_t {var_name}_data[];\n')
            f.write(f'\n#define NUM_HI_SLIDES {len(hi_converted_files)}\n')
            f.write('extern const image_info_t hi_images[NUM_HI_SLIDES];\n\n')
        
        # Backward compatibility
        if hi_converted_files:
            f.write(f'#define NUM_SLIDES {len(hi_converted_files)}\n')
            f.write('extern const image_info_t slide_images[NUM_SLIDES];\n')
    
    # Generate source file with image arrays
    images_c_path = os.path.join(output_dir, 'images.c')
    with open(images_c_path, 'w') as f:
        f.write('#include "images.h"\n\n')
        
        # Init animation array
        if init_converted_files:
            f.write('const image_info_t init_images[NUM_INIT_SLIDES] = {\n')
            for i, (var_name, _) in enumerate(init_converted_files):
                f.write(f'    {{{var_name}_data, 172, 320}}')
                if i < len(init_converted_files) - 1:
                    f.write(',')
                f.write('\n')
            f.write('};\n\n')
        
        # Hi animation array
        if hi_converted_files:
            f.write('const image_info_t hi_images[NUM_HI_SLIDES] = {\n')
            for i, (var_name, _) in enumerate(hi_converted_files):
                f.write(f'    {{{var_name}_data, 172, 320}}')
                if i < len(hi_converted_files) - 1:
                    f.write(',')
                f.write('\n')
            f.write('};\n\n')
            
            # Backward compatibility array (points to hi_images)
            f.write('const image_info_t slide_images[NUM_SLIDES] = {\n')
            for i, (var_name, _) in enumerate(hi_converted_files):
                f.write(f'    {{{var_name}_data, 172, 320}}')
                if i < len(hi_converted_files) - 1:
                    f.write(',')
                f.write('\n')
            f.write('};\n')
    
    print(f'\nConversion Summary:')
    print(f'Generated header: {header_path}')
    print(f'Generated source: {images_c_path}')
    if init_converted_files:
        print(f'Init animation: {len(init_converted_files)} frames')
    if hi_converted_files:
        print(f'Hi animation: {len(hi_converted_files)} frames')
    print('Conversion completed!')

if __name__ == '__main__':
    main()
