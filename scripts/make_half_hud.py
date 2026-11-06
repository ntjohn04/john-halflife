"""John Half-Life's half-size HUD (jhl_hud_scale 0.5, and below 640 wide as vanilla): the Unified SDK's HUD layouts
(sprites/*.json, sprites/op4/*.json) only have the full-size ("640") set; vanilla had a half-size ("320") one too.

  layouts <Half-Life folder>: writes config/halfhud/sprites[/op4]/<name>_320.json (installed in the game folder only;
      the expansions find them there) from the installed Unified SDK layouts.
      Each icon gets, in order:
      1. Valve's half-size one, where the Unified SDK's icon is pixel for pixel one of the games' full-size icons of the
         same name (the Unified SDK moved many between sheets);
      2. else the game's own half-size one of that name (Opposing Force's folder: Opposing Force's, else Half-Life's
         layout drawn from Opposing Force's sprites, as vanilla Opposing Force did; Half-Life's folder: Half-Life's or
         Blue Shift's), never another game's style;
      3. else its sprite halved (sprites/half/<name>.spr, made by `sprites` from the Unified SDK's own sprite), if that
         sprite is the Unified SDK's own (shipped in the zip);
      4. else as it is, full size.
      Crosshairs keep their own size (crosshair_scale). Valve's sprites in valve/ are found by the engine; Opposing
      Force's that differ are copied at first start to sprites/op4/ (make_setup_files.py's list).
  sprites <mod folder>: makes the sprites/half/*.spr the layouts name, from the folder's sprites, each frame halved (a
      2x2 block a pixel: transparent if two or more are, else their average in the sprite's own palette).
  list: prints the Opposing Force sprites the layouts need copied (for make_setup_files.py).

Usage: python make_half_hud.py layouts <Half-Life folder> | sprites <mod folder> | list
"""

import glob
import hashlib
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG = os.path.join(HERE, "..", "config", "halfhud", "sprites")
UNIFIED_FILES = os.path.join(HERE, "release_unified_files.txt")
TRANSPARENT = 255  # Index 255 is transparent in HUD sprites (alpha tested).


def load_json(path):
    text = open(path, encoding="utf-8").read()
    text = re.sub(r"(?m)^\s*//.*$", "", text)
    text = re.sub(r",(\s*[}\]])", r"\1", text)
    return json.loads(text)


def md5(path):
    try:
        return hashlib.md5(open(path, "rb").read()).hexdigest()
    except OSError:
        return None


def txt_rows(path):
    rows = {}
    if os.path.exists(path):
        for line in open(path, encoding="latin-1"):
            f = line.split()
            if len(f) >= 7 and f[1] in ("320", "640"):
                rows[(f[0], f[1])] = (f[2], tuple(int(v) for v in f[3:7]))
    return rows


_sprites = {}


def read_sprite(path):
    """(header, palette, [(origin x, origin y, width, height, pixels)]) or None."""
    if path not in _sprites:
        try:
            data = open(path, "rb").read()
        except OSError:
            _sprites[path] = None
            return None
        count = struct.unpack_from("<h", data, 40)[0]
        palette = data[42:42 + count * 3]
        offset = 42 + count * 3
        frames = []
        for _ in range(struct.unpack_from("<i", data, 28)[0]):
            assert struct.unpack_from("<i", data, offset)[0] == 0, "single frames only"
            ox, oy, w, h = struct.unpack_from("<iiii", data, offset + 4)
            offset += 20
            frames.append((ox, oy, w, h, data[offset:offset + w * h]))
            offset += w * h
        _sprites[path] = (data[:40], palette, frames)
    return _sprites[path]


def crop(path, rect):
    sprite = read_sprite(path)
    if not sprite:
        return None
    _, palette, frames = sprite
    _, _, w, h, pixels = frames[0]
    x0, y0, cw, ch = rect
    if x0 + cw > w or y0 + ch > h:
        return None
    return bytes(b for y in range(y0, y0 + ch) for i in pixels[y * w + x0:y * w + x0 + cw] for b in palette[i * 3:i * 3 + 3])


def layouts(half_life):
    shipped = {line.strip().lower() for line in open(UNIFIED_FILES) if line.strip()}
    mod = os.path.join(half_life, "johnhalflife")

    def sprite_name_for(game, name):
        """A vanilla half-size sprite of game's, as a name the game folder finds."""
        if game == "valve" or md5(f"{half_life}/{game}/sprites/{name}.spr") == md5(f"{half_life}/valve/sprites/{name}.spr"):
            return name
        if game == "gearbox":
            return f"op4/{name}"
        return name  # Blue Shift's own (bs_hud) is in the mod folder already.

    report = {"pixels": 0, "name": 0, "halved": 0, "full": 0, "crosshair": 0}
    copies = set()

    for folder, match_games, own_games in (("", ("valve", "bshift", "gearbox"), (("valve", "valve"), ("bshift", "bshift"))),
                                           ("op4", ("gearbox", "valve"), (("gearbox", "gearbox"), ("valve", "gearbox")))):
        source_dir = os.path.join(mod, "sprites", folder)
        target_dir = os.path.join(CONFIG, folder)
        os.makedirs(target_dir, exist_ok=True)

        for path in sorted(glob.glob(os.path.join(source_dir, "*.json"))):
            base = os.path.basename(path)[:-5]
            if base.endswith("_320"):
                continue
            half = {}

            for entry, e in load_json(path).items():
                sprite, rect = e["SpriteName"], (e["Left"], e["Top"], e["Width"], e["Height"])
                result = None

                if sprite.startswith(("crosshair", "ofch")):
                    result = dict(e)
                    report["crosshair"] += 1
                elif sprite in ("hud_pain", "op4/hud_pain", "hud_train", "op4/hud_train"):
                    # Whole sprites (pain arrows, train controls): each game's own half-size one.
                    game = "gearbox" if sprite.startswith("op4/") else "valve"
                    name = "320_pain" if "pain" in sprite else "320_train"
                    vanilla = read_sprite(f"{half_life}/{game}/sprites/{name}.spr")
                    scale_w, scale_h = vanilla[2][0][2] / read_sprite(f"{mod}/sprites/{sprite}.spr")[2][0][2], \
                        vanilla[2][0][3] / read_sprite(f"{mod}/sprites/{sprite}.spr")[2][0][3]
                    result = {"SpriteName": sprite_name_for(game, name), "Left": round(rect[0] * scale_w),
                              "Top": round(rect[1] * scale_h), "Width": round(rect[2] * scale_w),
                              "Height": round(rect[3] * scale_h)}
                    report["name"] += 1
                else:
                    mine = crop(f"{mod}/sprites/{sprite}.spr", rect)

                    for game in match_games:
                        rows = txt_rows(f"{half_life}/{game}/sprites/{base}.txt")
                        if (entry, "640") in rows and (entry, "320") in rows:
                            vs, vr = rows[(entry, "640")]
                            if mine and crop(f"{half_life}/{game}/sprites/{vs}.spr", vr) == mine:
                                name, r = rows[(entry, "320")]
                                result = {"SpriteName": sprite_name_for(game, name), "Left": r[0], "Top": r[1],
                                          "Width": r[2], "Height": r[3]}
                                report["pixels"] += 1
                                break

                    if not result:
                        for layout_game, sprite_game in own_games:
                            rows = txt_rows(f"{half_life}/{layout_game}/sprites/{base}.txt")
                            if (entry, "320") in rows:
                                name, r = rows[(entry, "320")]
                                result = {"SpriteName": sprite_name_for(sprite_game, name), "Left": r[0], "Top": r[1],
                                          "Width": r[2], "Height": r[3]}
                                report["name"] += 1
                                break

                    if not result and f"sprites/{sprite}.spr".lower() in shipped:
                        result = {"SpriteName": f"half/{sprite}", "Left": rect[0] // 2, "Top": rect[1] // 2,
                                  "Width": max(1, rect[2] // 2), "Height": max(1, rect[3] // 2)}
                        report["halved"] += 1

                    if not result:
                        result = dict(e)
                        report["full"] += 1

                if result["SpriteName"].startswith("op4/320") or result["SpriteName"] in ("op4/320_pain", "op4/320_train", "op4/320_logo"):
                    copies.add(result["SpriteName"])
                half[entry] = result

            with open(os.path.join(target_dir, f"{base}_320.json"), "w", encoding="utf-8", newline="\n") as f:
                json.dump(half, f, indent="\t")
                f.write("\n")

    print("make_half_hud: icons:", report)
    print("make_half_hud: Opposing Force sprites to copy:", sorted(copies))


def halve(path, target):
    header, palette, frames = read_sprite(path)
    colors = [tuple(palette[i * 3:i * 3 + 3]) for i in range(len(palette) // 3)]
    nearest_cache = {}

    def nearest(color):
        if color not in nearest_cache:
            nearest_cache[color] = min((i for i in range(len(colors)) if i != TRANSPARENT),
                                       key=lambda i: sum((a - b) ** 2 for a, b in zip(colors[i], color)))
        return nearest_cache[color]

    out = bytearray(header)
    width, height = struct.unpack_from("<ii", header, 20)
    struct.pack_into("<ii", out, 20, max(1, width // 2), max(1, height // 2))
    out += struct.pack("<h", len(colors)) + palette

    for ox, oy, w, h, pixels in frames:
        hw, hh = max(1, w // 2), max(1, h // 2)
        out += struct.pack("<iiiii", 0, ox // 2, oy // 2, hw, hh)
        for y in range(hh):
            for x in range(hw):
                block = [pixels[(2 * y + dy) * w + 2 * x + dx] for dy in (0, 1) for dx in (0, 1)
                         if 2 * y + dy < h and 2 * x + dx < w]
                solid = [i for i in block if i != TRANSPARENT]
                if len(solid) * 2 <= len(block):
                    out.append(TRANSPARENT)
                else:
                    out.append(nearest(tuple(round(sum(colors[i][c] for i in solid) / len(solid)) for c in range(3))))

    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(target, "wb") as f:
        f.write(out)


def sprites(mod):
    wanted = set()
    for path in glob.glob(os.path.join(CONFIG, "**", "*_320.json"), recursive=True):
        for e in load_json(path).values():
            if e["SpriteName"].startswith("half/"):
                wanted.add(e["SpriteName"][len("half/"):])
    for name in sorted(wanted):
        source = os.path.join(mod, "sprites", *name.split("/")) + ".spr"
        if not os.path.exists(source):
            sys.exit(f"make_half_hud: {source} not found")
        halve(source, os.path.join(mod, "sprites", "half", *name.split("/")) + ".spr")
    print(f"make_half_hud: {len(wanted)} sprites halved into {os.path.join(mod, 'sprites', 'half')}")


def list_copies():
    names = set()
    for path in glob.glob(os.path.join(CONFIG, "op4", "*_320.json")) + glob.glob(os.path.join(CONFIG, "*_320.json")):
        for e in load_json(path).values():
            if e["SpriteName"].startswith("op4/320"):
                names.add(e["SpriteName"])
    for name in sorted(names):
        print(name)


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "layouts":
        layouts(sys.argv[2])
    elif len(sys.argv) >= 3 and sys.argv[1] == "sprites":
        sprites(sys.argv[2])
    elif len(sys.argv) >= 2 and sys.argv[1] == "list":
        list_copies()
    else:
        sys.exit(__doc__)
