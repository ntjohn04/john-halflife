"""Writes Opposing Force's and Blue Shift's widescreen main menu backgrounds.

Usage:
  python make_menu_backgrounds.py [version] [--art folder]
"""

import argparse
import os

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(os.path.dirname(os.path.dirname(REPO)), "art", "menu_bg", "7_final21")
WIDTH, HEIGHT, TILE = 3840, 1600, 256
COLUMNS, ROWS = WIDTH // TILE, -(-HEIGHT // TILE)


def write(game, picture):
    image = Image.open(picture).convert("RGB")
    if image.size != (WIDTH, HEIGHT):
        raise SystemExit(f"make_menu_backgrounds: {picture} is {image.size}, not {WIDTH}x{HEIGHT}")
    resource = os.path.join(REPO, "config", "expansions", game, "resource")
    os.makedirs(os.path.join(resource, "background"), exist_ok=True)
    layout = [f"resolution\t{WIDTH}\t{HEIGHT}", ""]
    for row in range(ROWS):
        for column in range(COLUMNS):
            name = f"resource/background/jhl_21_9_{row + 1}_{chr(ord('a') + column)}.tga"
            x, y = column * TILE, row * TILE
            image.crop((x, y, x + TILE, y + TILE)).save(os.path.join(REPO, "config", "expansions", game, name))
            layout.append(f"{name}\tfit\t{x}\t{y}")
    with open(os.path.join(resource, "HD_BackgroundLayout.txt"), "w", newline="\n") as f:
        f.write("\n".join(layout) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("version", nargs="?", default="clean", choices=["logos", "gamelogo-only", "clean"])
    parser.add_argument("--art", default=ART)
    args = parser.parse_args()
    for game in ("op4", "bs"):
        write(game, os.path.join(args.art, f"{game}_{args.version}_{WIDTH}x{HEIGHT}.png"))
    print(f"make_menu_backgrounds: Opposing Force's and Blue Shift's backgrounds written ({args.version})")


if __name__ == "__main__":
    main()
