#!/usr/bin/env python3
"""Convert a C1 .spr gallery to .s32 (and optionally @2x.s32) for testing.

Developer tool for the neorender branch.  Each 8-bit frame is expanded
through the game palette exactly as the renderer does it -- PALETTE.DTA
entries 10..245 scaled from 6 to 8 bits (<< 2), the Windows static colours at
0..9 and 246..255 -- with index 0 transparent, and written as one PNG per
frame in an S32 container.  --scale 2 makes NAME@2x.s32 by nearest-neighbour
doubling, so the result is pixel-for-pixel what the .spr draws, just bigger:
useful to prove a tier is found and drawn, not as real HD art.  --mark
outlines every frame, magenta in a .s32 and cyan in an @2x.s32, so a
screenshot shows at a glance which tier each frame came from.

  spr_to_s32.py Images/back.spr Palettes/palette.dta out/ [--scale 1|2] [--mark]
"""

import argparse
import io
import os
import struct
import sys

from PIL import Image

# Windows' twenty static palette entries, which the game keeps at 0..9 and
# 246..255 of its logical palette.
STATIC_LOW = [(0, 0, 0), (128, 0, 0), (0, 128, 0), (128, 128, 0),
              (0, 0, 128), (128, 0, 128), (0, 128, 128), (192, 192, 192),
              (192, 220, 192), (166, 202, 240)]
STATIC_HIGH = [(255, 251, 240), (160, 160, 164), (128, 128, 128),
               (255, 0, 0), (0, 255, 0), (255, 255, 0), (0, 0, 255),
               (255, 0, 255), (0, 255, 255), (255, 255, 255)]


def load_palette(path):
    data = open(path, 'rb').read()
    if len(data) < 768:
        sys.exit(f'{path}: expected 768 bytes')
    palette = []
    for index in range(256):
        if index < 10:
            palette.append(STATIC_LOW[index])
        elif index >= 246:
            palette.append(STATIC_HIGH[index - 246])
        else:
            r, g, b = data[index * 3:index * 3 + 3]
            palette.append((r << 2 & 0xff, g << 2 & 0xff, b << 2 & 0xff))
    return palette


def read_spr(path):
    data = open(path, 'rb').read()
    (count,) = struct.unpack_from('<H', data, 0)
    frames = []
    for index in range(count):
        offset, width, height = struct.unpack_from('<IHH', data, 2 + 8 * index)
        pixels = data[offset:offset + width * height]
        if len(pixels) != width * height:
            sys.exit(f'{path}: frame {index} runs past the end of the file')
        frames.append((width, height, pixels))
    return frames


def frame_png(width, height, pixels, palette, scale, mark):
    rgba = bytearray()
    for value in pixels:
        r, g, b = palette[value]
        rgba += bytes((r, g, b, 0 if value == 0 else 255))
    image = Image.frombytes('RGBA', (width, height), bytes(rgba))
    if scale != 1:
        image = image.resize((width * scale, height * scale), Image.NEAREST)
    if mark:
        colour = (0, 255, 255, 255) if scale == 2 else (255, 0, 255, 255)
        w, h = image.size
        for x in range(w):
            image.putpixel((x, 0), colour)
            image.putpixel((x, h - 1), colour)
        for y in range(h):
            image.putpixel((0, y), colour)
            image.putpixel((w - 1, y), colour)
    out = io.BytesIO()
    image.save(out, 'PNG', optimize=True)
    return out.getvalue()


def write_s32(path, pngs):
    header = struct.pack('<IH', 0x4, len(pngs))
    offset = len(header) + 8 * len(pngs)
    table = b''
    for png in pngs:
        table += struct.pack('<IHH', offset, 0, 0)  # width/height deprecated
        offset += len(png)
    with open(path, 'wb') as out:
        out.write(header + table + b''.join(pngs))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('spr')
    parser.add_argument('palette')
    parser.add_argument('output_directory')
    parser.add_argument('--scale', type=int, choices=(1, 2), default=1)
    parser.add_argument('--mark', action='store_true',
                        help='outline every frame (magenta 1x, cyan 2x)')
    args = parser.parse_args()

    palette = load_palette(args.palette)
    frames = read_spr(args.spr)
    pngs = [frame_png(w, h, p, palette, args.scale, args.mark) for w, h, p in frames]
    stem = os.path.splitext(os.path.basename(args.spr))[0]
    name = stem + ('@2x.s32' if args.scale == 2 else '.s32')
    os.makedirs(args.output_directory, exist_ok=True)
    path = os.path.join(args.output_directory, name)
    write_s32(path, pngs)
    print(f'{path}: {len(frames)} frames')


if __name__ == '__main__':
    main()
