"""Makes config/mod/sprites/blood_chnk_yellow.spr, a yellow (alien blood) version of Opposing Force's sprites/blood_chnk.spr, for the gib splash (src/game/client/client_gibs.cpp).

Usage: python scripts/make_yellow_blood_chunk.py <path to Half-Life>/gearbox/sprites/blood_chnk.spr
"""

import struct
import sys
from pathlib import Path

YELLOW = (160, 135, 40)

PALETTE_COUNT_OFFSET = 40


def luminance(color):
    r, g, b = color
    return 0.299 * r + 0.587 * g + 0.114 * b


def main():
    source = Path(sys.argv[1])
    data = bytearray(source.read_bytes())

    count = struct.unpack_from("<h", data, PALETTE_COUNT_OFFSET)[0]
    start = PALETTE_COUNT_OFFSET + 2
    palette = [tuple(data[start + i * 3:start + i * 3 + 3]) for i in range(count)]

    brightest = max(luminance(color) for color in palette[:-1])

    for i, color in enumerate(palette[:-1]):
        scale = luminance(color) / brightest
        data[start + i * 3:start + i * 3 + 3] = bytes(min(255, round(channel * scale)) for channel in YELLOW)

    target = Path(__file__).resolve().parent.parent / "config" / "mod" / "sprites" / "blood_chnk_yellow.spr"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    print(f"wrote {target}")


if __name__ == "__main__":
    main()
