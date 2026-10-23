"""Makes config/mod/sprites/blood_chnk_yellow.spr, a yellow (alien blood) version of Opposing Force's
sprites/blood_chnk.spr, for the gib splash (src/game/client/client_gibs.cpp).

Only the sprite's palette changes: each color keeps its brightness relative to the brightest one and takes the mustard
hue of the game's yellow blood. The frames, and the last palette entry (transparency), are untouched.

Usage: python scripts/make_yellow_blood_chunk.py <path to Half-Life>/gearbox/sprites/blood_chnk.spr
"""

import struct
import sys
from pathlib import Path

# The brightest yellow: between the yellow hit puffs (palette 198: 155,131,7) and the yellow blood decal tint
# (204,184,97).
YELLOW = (160, 135, 40)

PALETTE_COUNT_OFFSET = 40  # After the 40-byte sprite header.


def luminance(color):
    r, g, b = color
    return 0.299 * r + 0.587 * g + 0.114 * b


def main():
    source = Path(sys.argv[1])
    data = bytearray(source.read_bytes())

    count = struct.unpack_from("<h", data, PALETTE_COUNT_OFFSET)[0]
    start = PALETTE_COUNT_OFFSET + 2
    palette = [tuple(data[start + i * 3:start + i * 3 + 3]) for i in range(count)]

    # The last entry is the transparent color in an alphatest sprite.
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
