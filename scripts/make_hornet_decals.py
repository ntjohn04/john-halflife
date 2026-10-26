"""Adds the hornet gun's dart splats to an installed John Half-Life's decals.wad (johnhalflife, server hornet.cpp):
Half-Life's bullsquid spit splats ({spit1, {spit2) in each hornet trail's colour, as {hspit_red1, {hspit_red2, and so
on for pur(ple-red), org (orange) and yel(low).

A decal's pixels are alpha and its colour is the palette's last entry, so each recolour is a copy with a new name and
that one entry changed.

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
    "red": (170, 40, 20),
    "pur": (150, 30, 95),
    "org": (215, 120, 25),
    "yel": (215, 185, 45),
}


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


def recolour(lump, name, color):
    """A decal miptex lump renamed and in another colour (its palette's last entry)."""
    data = bytearray(lump)
    data[0:16] = name.encode("latin-1")[:15].ljust(16, b"\0")

    width, height = struct.unpack_from("<II", data, 16)
    offsets = struct.unpack_from("<4I", data, 24)

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
    by_name = {name.lower(): lump for name, lump in ((l[0], l) for l in lumps)}

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
