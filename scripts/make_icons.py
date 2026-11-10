"""Writes each game folder's icon from assets/icons/<folder>.png (square, any size, 256x256 or more is best): its
game.ico (16 to 256 pixels: the game's window and taskbar), game.tga (16x16) and game_big.tga (32x32, the one Steam's
library shows for a mod, from liblist.gam's icon "game"), into the folder's config: config/mod for johnhalflife,
config/expansions/<op4|bs|uplink> for the others.

A hand-made small version can sit beside a picture, <folder>_16.png or <folder>_32.png, used as it is for that size:
a picture reduced to 16 or 32 pixels loses its fine lines.

Steam needs the TGAs uncompressed (it left out a mod whose icon was RLE-compressed) and the folders' pictures
different from each other (it left out one whose icon was the same as another's).

Usage:
  python make_icons.py
"""

import os

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICONS = os.path.join(REPO, "assets", "icons")
FOLDERS = {
    "johnhalflife": os.path.join(REPO, "config", "mod"),
    "johnhalflife_op4": os.path.join(REPO, "config", "expansions", "op4"),
    "johnhalflife_bs": os.path.join(REPO, "config", "expansions", "bs"),
    "johnhalflife_uplink": os.path.join(REPO, "config", "expansions", "uplink"),
}


def sized(folder, picture, size):
    own = os.path.join(ICONS, f"{folder}_{size}.png")
    if os.path.exists(own):
        return Image.open(own).convert("RGBA").resize((size, size), Image.LANCZOS)
    return picture.resize((size, size), Image.LANCZOS)


def main():
    seen = {}
    for folder, config in FOLDERS.items():
        picture = Image.open(os.path.join(ICONS, f"{folder}.png")).convert("RGBA")
        if picture.width != picture.height:
            raise SystemExit(f"make_icons: {folder}.png isn't square")
        big = sized(folder, picture, 32)
        if big.tobytes() in seen:
            raise SystemExit(f"make_icons: {folder}'s icon is the same as {seen[big.tobytes()]}'s (Steam shows only one)")
        seen[big.tobytes()] = folder
        # Uncompressed, 32-bit with alpha (PIL's default for RGBA without rle=True).
        big.save(os.path.join(config, "game_big.tga"))
        sized(folder, picture, 16).save(os.path.join(config, "game.tga"))
        picture.save(os.path.join(config, "game.ico"), sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)
                                                             if s <= picture.width])
    print("make_icons: the four folders' icons written")


if __name__ == "__main__":
    main()
