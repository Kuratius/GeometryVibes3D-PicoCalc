#!/usr/bin/env python3
from __future__ import annotations

import argparse
import struct
from pathlib import Path
from PIL import Image


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    r5 = (r * 31 + 127) // 255
    g6 = (g * 63 + 127) // 255
    b5 = (b * 31 + 127) // 255
    return (r5 << 11) | (g6 << 5) | b5


def convert_image_to_rgb565(img: Image.Image) -> tuple[int, int, bytes]:
    img = img.convert("RGB")
    w, h = img.size

    src = img.tobytes()
    out = bytearray(w * h * 2)

    j = 0
    for i in range(0, len(src), 3):
        r = src[i]
        g = src[i + 1]
        b = src[i + 2]
        px = rgb888_to_rgb565(r, g, b)
        out[j] = px & 0xFF
        out[j + 1] = (px >> 8) & 0xFF
        j += 2

    return w, h, bytes(out)


def main() -> None:
    ap = argparse.ArgumentParser(description="Convert image to raw RGB565.")
    ap.add_argument("input", help="Input image (PNG/JPG/etc.)")
    ap.add_argument("output", nargs="?", help="Output .rgb565 file")
    args = ap.parse_args()

    in_path = Path(args.input)
    out_path = Path(args.output) if args.output else in_path.with_suffix(".rgb565")

    img = Image.open(in_path)
    w, h, data = convert_image_to_rgb565(img)

    with out_path.open("wb") as f:
        f.write(data)

    print(f"Input : {in_path}")
    print(f"Output: {out_path}")
    print(f"Size  : {w}x{h}")
    print(f"Bytes : {len(data)}")


if __name__ == "__main__":
    main()