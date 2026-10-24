"""Makes config/mod/sprites/xspark4_round.spr, Half-Life's sprites/xspark4.spr with its haze faded out outside a
circle, for the controller's head ball (src/game/server/entities/NPCs/aliens/controller.cpp). The sprite is additive,
so black is transparent; at the head ball's scale of 2 the haze, which fills the 32 px frames to their edges, showed
as a faint square.

Every pixel's color is scaled by a soft circular falloff (full inside FULL_RADIUS, nothing past EDGE_RADIUS, from the
frame's center) and mapped to the nearest color of the sprite's own palette. The palette has no black: its darkest
colors, which the frames' background is (7,0,0), are lifted a lot by the engine's texture gamma, so the whole square
showed faintly. Its last color (4,0,0) becomes black, and every pixel darker than BLACK_BELOW goes to it.

Usage: python scripts/make_round_xspark4.py <path to Half-Life>/valve/sprites/xspark4.spr [preview.png]
"""

import struct
import sys
from pathlib import Path

FULL_RADIUS = 9.0
EDGE_RADIUS = 15.0
BLACK_BELOW = 12  # Sum of red, green and blue.
BLACK = 255  # The palette entry made black.


def falloff(distance):
    if distance <= FULL_RADIUS:
        return 1.0
    if distance >= EDGE_RADIUS:
        return 0.0
    t = (distance - FULL_RADIUS) / (EDGE_RADIUS - FULL_RADIUS)
    return (1 - t) * (1 - t) * (1 + 2 * t)  # Smooth.


def main():
    data = bytearray(Path(sys.argv[1]).read_bytes())

    frame_count = struct.unpack_from("<i", data, 28)[0]
    palette_count = struct.unpack_from("<h", data, 40)[0]
    palette = [tuple(data[42 + i * 3:45 + i * 3]) for i in range(palette_count)]
    palette[BLACK] = (0, 0, 0)
    data[42 + BLACK * 3:45 + BLACK * 3] = bytes(3)

    def nearest(color):
        if sum(color) < BLACK_BELOW:
            return BLACK
        return min(range(palette_count), key=lambda i: sum((a - b) ** 2 for a, b in zip(palette[i], color)))

    cache = {}
    frames = []
    offset = 42 + palette_count * 3

    for _ in range(frame_count):
        assert struct.unpack_from("<i", data, offset)[0] == 0, "single frames only"
        offset += 4
        _, _, width, height = struct.unpack_from("<iiii", data, offset)
        offset += 16

        for y in range(height):
            for x in range(width):
                distance = ((x + 0.5 - width / 2) ** 2 + (y + 0.5 - height / 2) ** 2) ** 0.5
                scale = falloff(distance)
                pixel = offset + y * width + x
                key = (data[pixel], round(scale * 32))

                if key not in cache:
                    color = tuple(round(c * key[1] / 32) for c in palette[data[pixel]])
                    cache[key] = nearest(color)

                data[pixel] = cache[key]

        frames.append((width, height, bytes(data[offset:offset + width * height])))
        offset += width * height

    target = Path(__file__).resolve().parent.parent / "config" / "mod" / "sprites" / "xspark4_round.spr"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    print(f"wrote {target}")

    if len(sys.argv) > 2:
        from PIL import Image

        tile = 96
        sheet = Image.new("RGB", (tile * len(frames), tile))

        for i, (width, height, pixels) in enumerate(frames):
            image = Image.new("RGB", (width, height))
            image.putdata([palette[p] for p in pixels])
            sheet.paste(image.resize((tile, tile), Image.NEAREST), (i * tile, 0))

        sheet.save(sys.argv[2])


if __name__ == "__main__":
    main()
