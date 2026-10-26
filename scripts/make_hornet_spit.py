"""Makes config/mod/sprites/hornet_spit.spr: Half-Life's tinyspit.spr (the bullsquid's yellow-green spittle) in the
hornet gun's own brownish red, for the spit it sprays from its muzzle (johnhalflife, client ev_hldm.cpp).

tinyspit is an index-alpha sprite: its pixels are alpha and its colour is the palette's last entry, so the recolour is
that one entry.

Usage: python make_hornet_spit.py <path to valve/sprites/tinyspit.spr> <output .spr>
"""

import struct
import sys

COLOR = (110, 42, 30)


def main():
    source, output = sys.argv[1], sys.argv[2]
    data = bytearray(open(source, "rb").read())

    # The header is 40 bytes; the palette follows: a short count, then count RGB triples.
    count = struct.unpack_from("<h", data, 40)[0]
    if count != 256:
        raise SystemExit(f"expected a 256-colour palette, found {count}")

    last = 42 + 255 * 3
    data[last:last + 3] = bytes(COLOR)

    open(output, "wb").write(data)


if __name__ == "__main__":
    main()
