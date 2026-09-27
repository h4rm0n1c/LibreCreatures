#!/usr/bin/env python3
"""Draws the LibreCreatures kits' own icons.

The kits carry no Creature Labs art (see ../README.md): these icons are
drawn here, from shapes, and written as .ico files (16, 32 and 48 pixels,
32-bit) into each kit's resources.  Run it from anywhere:

    python3 src/kits/tools/make_kit_icons.py

Needs Pillow.
"""

import os
import struct

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
KITS = os.path.dirname(HERE)
SIZES = (16, 32, 48)
SCALE = 8  # drawn large, then reduced, for smooth edges

WHITE = (255, 255, 255, 255)
INK = (28, 32, 40, 255)


def badge(draw, size, colour):
    inset = size * 0.04
    draw.rounded_rectangle([inset, inset, size - inset, size - inset], radius=size * 0.22,
                           fill=colour)


def eye(draw, s):
    draw.ellipse([s * .16, s * .30, s * .84, s * .70], fill=WHITE)
    draw.ellipse([s * .38, s * .36, s * .62, s * .64], fill=INK)
    draw.ellipse([s * .45, s * .41, s * .52, s * .48], fill=WHITE)


def star(draw, s):
    import math
    points = []
    for i in range(10):
        r = s * (.36 if i % 2 == 0 else .15)
        a = math.pi / 2 + i * math.pi / 5
        points.append((s / 2 + r * math.cos(a), s * .53 - r * math.sin(a)))
    draw.polygon(points, fill=WHITE)


def camera(draw, s):
    draw.rounded_rectangle([s * .16, s * .32, s * .84, s * .74], radius=s * .06, fill=WHITE)
    draw.rectangle([s * .36, s * .24, s * .60, s * .34], fill=WHITE)
    draw.ellipse([s * .36, s * .38, s * .64, s * .66], fill=INK)
    draw.ellipse([s * .42, s * .44, s * .58, s * .60], fill=WHITE)


def headstone(draw, s):
    draw.rounded_rectangle([s * .28, s * .20, s * .72, s * .78], radius=s * .20, fill=WHITE)
    draw.rectangle([s * .28, s * .45, s * .72, s * .78], fill=WHITE)
    draw.rectangle([s * .47, s * .30, s * .53, s * .56], fill=INK)
    draw.rectangle([s * .39, s * .37, s * .61, s * .42], fill=INK)
    draw.rectangle([s * .18, s * .76, s * .82, s * .82], fill=WHITE)


def flask(draw, s):
    draw.rectangle([s * .42, s * .18, s * .58, s * .40], fill=WHITE)
    draw.polygon([(s * .42, s * .38), (s * .58, s * .38), (s * .80, s * .78), (s * .20, s * .78)],
                 fill=WHITE)
    draw.polygon([(s * .30, s * .62), (s * .70, s * .62), (s * .77, s * .74), (s * .23, s * .74)],
                 fill=(90, 170, 240, 255))


def cross(draw, s):
    draw.rectangle([s * .40, s * .18, s * .60, s * .82], fill=WHITE)
    draw.rectangle([s * .18, s * .40, s * .82, s * .60], fill=WHITE)


def heart(draw, s):
    draw.ellipse([s * .18, s * .24, s * .52, s * .56], fill=WHITE)
    draw.ellipse([s * .48, s * .24, s * .82, s * .56], fill=WHITE)
    draw.polygon([(s * .20, s * .46), (s * .80, s * .46), (s * .50, s * .82)], fill=WHITE)


def egg(draw, s):
    draw.ellipse([s * .26, s * .16, s * .74, s * .84], fill=(250, 240, 220, 255))
    draw.ellipse([s * .36, s * .28, s * .46, s * .40], fill=WHITE)


def syringe(draw, s):
    body = [(s * .30, s * .62), (s * .62, s * .30), (s * .72, s * .40), (s * .40, s * .72)]
    draw.polygon(body, fill=WHITE)
    draw.line([(s * .35, s * .67), (s * .16, s * .86)], fill=WHITE, width=int(s * .05))
    draw.line([(s * .67, s * .35), (s * .80, s * .22)], fill=WHITE, width=int(s * .06))
    draw.line([(s * .72, s * .16), (s * .86, s * .30)], fill=WHITE, width=int(s * .06))


def graph(draw, s):
    draw.line([(s * .18, s * .80), (s * .82, s * .80)], fill=WHITE, width=int(s * .05))
    draw.line([(s * .18, s * .80), (s * .18, s * .20)], fill=WHITE, width=int(s * .05))
    draw.line([(s * .22, s * .66), (s * .38, s * .44), (s * .52, s * .58), (s * .66, s * .30),
               (s * .80, s * .40)], fill=WHITE, width=int(s * .07), joint="curve")


# kit directory: (badge colour, glyph)
KIT_ICONS = {
    "observation": ((40, 140, 150, 255), eye),
    "score": ((210, 160, 30, 255), star),
    "owner": ((130, 80, 170, 255), camera),
    "funeral": ((90, 100, 115, 255), headstone),
    "science": ((40, 100, 190, 255), flask),
    "health": ((200, 50, 50, 255), cross),
    "breeder": ((215, 80, 130, 255), heart),
    "hatchery": ((150, 100, 60, 255), egg),
    "injector": ((50, 150, 90, 255), syringe),
    "biochem": ((80, 70, 170, 255), graph),
}


def render(colour, glyph, size):
    big = size * SCALE
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    badge(draw, big, colour)
    glyph(draw, big)
    return image.resize((size, size), Image.LANCZOS)


def dib_entry(image):
    """An .ico image entry as a 32-bit DIB (with its AND mask), which every
    Windows resource compiler and Windows version accept."""
    width, height = image.size
    header = struct.pack("<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    pixels = bytearray()
    for y in range(height - 1, -1, -1):
        for x in range(width):
            r, g, b, a = image.getpixel((x, y))
            pixels += bytes((b, g, r, a))
    row = ((width + 31) // 32) * 4
    mask = bytearray()
    for y in range(height - 1, -1, -1):
        bits = bytearray(row)
        for x in range(width):
            if image.getpixel((x, y))[3] < 128:
                bits[x // 8] |= 0x80 >> (x % 8)
        mask += bits
    return header + bytes(pixels) + bytes(mask)


def write_ico(path, images):
    entries = [dib_entry(image) for image in images]
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for image, data in zip(images, entries):
        w, h = image.size
        out += struct.pack("<BBBBHHII", w % 256, h % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    for data in entries:
        out += data
    with open(path, "wb") as f:
        f.write(out)


def main():
    for kit, (colour, glyph) in KIT_ICONS.items():
        folder = os.path.join(KITS, kit, "resources", "images", "icon")
        os.makedirs(folder, exist_ok=True)
        write_ico(os.path.join(folder, "128.ico"), [render(colour, glyph, s) for s in SIZES])
        print("wrote", os.path.relpath(os.path.join(folder, "128.ico"), KITS))


if __name__ == "__main__":
    main()
