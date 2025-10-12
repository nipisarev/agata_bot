#!/usr/bin/env python3
"""
ESP32-C6 SD Card Image Converter
Converts PNG images to raw RGB565 format for SD card storage
"""

import os
import re
import struct
from PIL import Image
import argparse

def natural_sort_key(filename):
    """Natural sorting key for filenames with numbers"""
    return [int(text) if text.isdigit() else text.lower() for text in re.split('([0-9]+)', filename)]

def convert_png_to_raw(png_path, output_path, target_width=170, target_height=320):
    """Convert PNG image to raw RGB565 format with header"""
    try:
        # Open and process image
        with Image.open(png_path) as img:
            # Convert to RGB if necessary
            if img.mode != 'RGB':
                img = img.convert('RGB')
            
            # Resize if necessary
            if img.size != (target_width, target_height):
                img = img.resize((target_width, target_height), Image.Resampling.LANCZOS)
                print(f"Resized {png_path} from {img.size} to {target_width}x{target_height}")
            
            width, height = img.size
            
            # Convert to RGB565 format
            rgb565_data = []
            for y in range(height):
                for x in range(width):
                    r, g, b = img.getpixel((x, y))
                    
                    # Convert to RGB565
                    r = (r >> 3) & 0x1F  # 5 bits
                    g = (g >> 2) & 0x3F  # 6 bits  
                    b = (b >> 3) & 0x1F  # 5 bits
                    
                    rgb565 = (r << 11) | (g << 5) | b
                    rgb565_data.append(rgb565)
            
            # Write to raw file with header
            with open(output_path, 'wb') as f:
                # Write header: width (2 bytes) + height (2 bytes)
                f.write(struct.pack('<HH', width, height))
                
                # Write pixel data
                for pixel in rgb565_data:
                    f.write(struct.pack('<H', pixel))
            
            file_size = os.path.getsize(output_path)
            print(f"Converted {png_path} -> {output_path} ({width}x{height}, {file_size} bytes)")
            return True
            
    except Exception as e:
        print(f"Error converting {png_path}: {e}")
        return False

def convert_animation_folder(input_dir, output_dir, animation_name):
    """Convert all PNG files in a folder to raw format"""
    if not os.path.exists(input_dir):
        print(f"Input directory {input_dir} does not exist")
        return False
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Find all PNG files
    png_files = [f for f in os.listdir(input_dir) if f.lower().endswith('.png')]
    if not png_files:
        print(f"No PNG files found in {input_dir}")
        return False
    
    # Sort naturally
    png_files.sort(key=natural_sort_key)
    
    print(f"Converting {animation_name} animation:")
    print(f"Found {len(png_files)} PNG files: {png_files}")
    
    success_count = 0
    for png_file in png_files:
        input_path = os.path.join(input_dir, png_file)
        
        # Generate output filename (.png -> .raw)
        raw_file = png_file.replace('.png', '.raw').replace('.PNG', '.raw')
        output_path = os.path.join(output_dir, raw_file)
        
        if convert_png_to_raw(input_path, output_path):
            success_count += 1
    
    print(f"Successfully converted {success_count}/{len(png_files)} {animation_name} images")
    return success_count > 0

def create_sd_card_structure():
    """Create the recommended SD card folder structure"""
    print("\\nRecommended SD Card Structure:")
    print("/")
    print("├── init/")
    print("│   ├── init_1.raw")  
    print("│   ├── init_2.raw")
    print("│   └── ... (up to init_18.raw)")
    print("├── hi/")
    print("│   ├── hi_1.raw")
    print("│   ├── hi_2.raw") 
    print("│   └── ... (up to hi_11.raw)")
    print("")
    print("Copy the generated .raw files to your SD card following this structure.")

def main():
    parser = argparse.ArgumentParser(description='Convert PNG animations to ESP32-C6 SD card format')
    parser.add_argument('--input-base', default='assets', help='Base input directory containing init/ and hi/ folders')
    parser.add_argument('--output-base', default='sd_images', help='Base output directory for raw files')
    parser.add_argument('--width', type=int, default=170, help='Target image width')
    parser.add_argument('--height', type=int, default=320, help='Target image height')
    
    args = parser.parse_args()
    
    print("ESP32-C6 SD Card Image Converter")
    print("=" * 40)
    
    success = True
    
    # Convert init animation
    init_input = os.path.join(args.input_base, 'init')
    init_output = os.path.join(args.output_base, 'init')
    if os.path.exists(init_input):
        success &= convert_animation_folder(init_input, init_output, 'init')
    else:
        print(f"Warning: Init animation folder {init_input} not found")
    
    # Convert hi animation  
    hi_input = os.path.join(args.input_base, 'hi')
    hi_output = os.path.join(args.output_base, 'hi')
    if os.path.exists(hi_input):
        success &= convert_animation_folder(hi_input, hi_output, 'hi')
    else:
        print(f"Warning: Hi animation folder {hi_input} not found")
    
    if success:
        print("\\n✅ Conversion completed successfully!")
        create_sd_card_structure()
    else:
        print("\\n❌ Some conversions failed")
        return 1
    
    return 0

if __name__ == '__main__':
    exit(main())
