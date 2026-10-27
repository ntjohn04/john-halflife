"""Adds the hornet gun's dart splats to an installed John Half-Life's decals.wad (johnhalflife, server hornet.cpp):
Half-Life's bullsquid spit splats ({spit1, {spit2) in each hornet trail's colour, muted and darker (organic, not
paint), and smaller (the shape at 75% in the middle of its 32x32: hornets are small), as {hspit_red1, {hspit_red2, and
so on for pur(ple-red), org (orange), yel(low) and brn (light brown); and wet, a dull neutral grey, the barnacle
grapple's mark where its tongue latches onto a surface (server CGrappleTip.cpp).

A decal's pixels are alpha (0 clear) and its colour is the palette's last entry, so each recolour is a copy with a new
name, that one entry changed and its pixels shrunk (its smaller mip levels made again from them).

The engine reads one decals.wad, so the new decals go into the installed one (the Unified SDK's, made from the
player's own games when installing its assets). Its original is kept once as decals_unified.wad and every run starts
from that, so running it again changes nothing; nothing of Valve's is committed to the repository.

Usage: python make_hornet_decals.py <mod folder>
"""

import os
import shutil
import struct
import sys

SHAPES = ["{spit1", "{spit2"]

COLORS = {
    "red": (90, 50, 42),
    "pur": (80, 48, 55),
    "org": (105, 80, 50),
    "yel": (110, 98, 62),
    "brn": (100, 85, 65),
    "wet": (65, 63, 58),
}

# The shape's size within its texture.
SCALE = 0.75


def read_wad(path):
    data = open(path, "rb").read()
    magic, count, directory = struct.unpack_from("<4sii", data, 0)
    if magic != b"WAD3":
        raise SystemExit(f"{path} isn't a WAD3 file")

    lumps = []
    for i in range(count):
        entry = directory + i * 32
        offset, disk_size, size, kind, compression, _pad = struct.unpack_from("<iiibbh", data, entry)
        name = data[entry + 16: entry + 32].split(b"\0")[0].decode("latin-1")
        lumps.append((name, kind, compression, data[offset: offset + disk_size], size))

    return lumps


def write_wad(path, lumps):
    body = bytearray()
    directory = bytearray()

    for name, kind, compression, lump, size in lumps:
        offset = 12 + len(body)
        body += lump
        directory += struct.pack("<iiibbh", offset, len(lump), size, kind, compression, 0)
        directory += name.encode("latin-1")[:15].ljust(16, b"\0")

    header = struct.pack("<4sii", b"WAD3", len(lumps), 12 + len(body))
    open(path, "wb").write(header + body + directory)


def shrink(pixels, width, height, scale):
    """The alpha image at scale in the middle of the same size, clear around it (bilinear)."""
    out = bytearray(width * height)
    inner_w, inner_h = width * scale, height * scale
    left, top = (width - inner_w) / 2, (height - inner_h) / 2

    def at(x, y):
        x = min(max(x, 0), width - 1)
        y = min(max(y, 0), height - 1)
        return pixels[y * width + x]

    for y in range(height):
        for x in range(width):
            # Where this pixel's centre falls in the original.
            u = (x + 0.5 - left) / scale - 0.5
            v = (y + 0.5 - top) / scale - 0.5

            if u < -0.5 or v < -0.5 or u > width - 0.5 or v > height - 0.5:
                continue

            x0, y0 = int(u // 1), int(v // 1)
            fx, fy = u - x0, v - y0
            value = (at(x0, y0) * (1 - fx) * (1 - fy) + at(x0 + 1, y0) * fx * (1 - fy) +
                     at(x0, y0 + 1) * (1 - fx) * fy + at(x0 + 1, y0 + 1) * fx * fy)
            out[y * width + x] = min(255, int(round(value)))

    return out


def downsample(pixels, width, height):
    """Half the size, each pixel the average of the four it covers."""
    half_w, half_h = width // 2, height // 2
    out = bytearray(half_w * half_h)
    for y in range(half_h):
        for x in range(half_w):
            total = (pixels[(2 * y) * width + 2 * x] + pixels[(2 * y) * width + 2 * x + 1] +
                     pixels[(2 * y + 1) * width + 2 * x] + pixels[(2 * y + 1) * width + 2 * x + 1])
            out[y * half_w + x] = total // 4
    return out


def recolour(lump, name, color):
    """A decal miptex lump renamed, in another colour (its palette's last entry) and shrunk."""
    data = bytearray(lump)
    data[0:16] = name.encode("latin-1")[:15].ljust(16, b"\0")

    width, height = struct.unpack_from("<II", data, 16)
    offsets = struct.unpack_from("<4I", data, 24)

    level = shrink(data[offsets[0]: offsets[0] + width * height], width, height, SCALE)
    w, h = width, height
    for mip in range(4):
        data[offsets[mip]: offsets[mip] + w * h] = level
        if mip < 3:
            level = downsample(level, w, h)
            w, h = w // 2, h // 2

    # After the four mip levels: a short colour count, then the palette.
    palette = offsets[3] + (width // 8) * (height // 8) + 2
    last = palette + 255 * 3
    data[last:last + 3] = bytes(color)
    return bytes(data)


def main():
    mod = sys.argv[1]
    wad = os.path.join(mod, "decals.wad")
    original = os.path.join(mod, "decals_unified.wad")

    if not os.path.exists(original):
        if not os.path.exists(wad):
            print(f"make_hornet_decals: no decals.wad in {mod}; install the Unified SDK's assets first")
            return
        shutil.copyfile(wad, original)

    lumps = read_wad(original)
    by_name = {lump[0].lower(): lump for lump in lumps}

    added = []
    for colour, rgb in COLORS.items():
        for index, shape in enumerate(SHAPES, start=1):
            source = by_name.get(shape)
            if source is None:
                print(f"make_hornet_decals: {shape} isn't in {original}")
                continue

            name = f"{{hspit_{colour}{index}"
            _, kind, compression, lump, size = source
            added.append((name, kind, compression, recolour(lump, name, rgb), size))

    write_wad(wad, lumps + added)
    print(f"make_hornet_decals: {len(added)} hornet splats added to {wad}")


if __name__ == "__main__":
    main()
