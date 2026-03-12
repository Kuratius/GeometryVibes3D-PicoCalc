### RGB565 Image Converter (Python)

A simple Pillow-based converter for preparing bitmap assets for GV3D.

Features:
- Converts source images (such as PNG or JPG) to raw **RGB565**
- Outputs tightly packed **little-endian** pixel data
- Suitable for full-screen assets such as the title screen
- Keeps runtime image loading simple by avoiding PNG decoding on-device

Run:
```bash
python image_convert/convert_rgb565.py images/title.png