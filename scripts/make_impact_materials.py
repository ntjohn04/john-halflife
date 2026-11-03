"""Makes config/mod/sound/jhl_impact_materials.json: how bullet impacts, melee and explosion debris treat the textures
Valve's materials table doesn't list (footsteps and ricochet sounds keep Valve's). Each texture the three games' maps
use gets Half-Life: Source's surface type where it has one (Valve's own later judgement, read from its hl1_pak VPK),
else, for Opposing Force's and Blue Shift's own textures, a guess from the name; textures left concrete aren't listed.

Needs Half-Life (with Opposing Force and Blue Shift, and johnhalflife set up: its sound/materials.json is Valve's table
as the game has it) and Half-Life: Source installed.

Usage: make_impact_materials.py <Half-Life folder> <Half-Life 2 folder, with Half-Life: Source> <output json>
"""
import collections
import glob
import json
import os
import re
import struct
import sys


def strip(name):
    """A texture name without its animation, tiling or transparency prefix, as the materials table keys it."""
    name = name.upper()
    if name[:1] in "+-" and len(name) > 2:
        name = name[2:]
    if name[:1] in "{!~":
        name = name[1:]
    return name


def vpk_files(dir_path, extension):
    """Each file with this extension in a VPK (version 1 or 2): (path without extension, contents)."""
    d = open(dir_path, "rb").read()
    signature, version, tree = struct.unpack_from("<III", d, 0)
    if signature != 0x55AA1234:
        sys.exit(f"{dir_path}: not a VPK")
    start = 12 if version == 1 else 28
    end = start + tree
    base = dir_path[:-len("_dir.vpk")]

    def cstr(p):
        e = d.index(b"\0", p)
        return d[p:e].decode("latin1"), e + 1

    p = start
    while p < end:
        ext, p = cstr(p)
        if not ext:
            break
        while True:
            path, p = cstr(p)
            if not path:
                break
            while True:
                name, p = cstr(p)
                if not name:
                    break
                _crc, preload, archive, offset, length, _term = struct.unpack_from("<IHHIIH", d, p)
                p += 18
                body = d[p:p + preload]
                p += preload
                if ext != extension:
                    continue
                if archive == 0x7FFF:
                    body += d[end + offset:end + offset + length]
                else:
                    with open(f"{base}_{archive:03d}.vpk", "rb") as f:
                        f.seek(offset)
                        body += f.read(length)
                yield f"{path}/{name}".lower(), body.decode("latin1", "replace")


def hls_surfaceprops(hl2):
    """Half-Life: Source's materials: texture name -> $surfaceprop (through one include), "" if none."""
    files = dict(vpk_files(f"{hl2}/hl1/hl1_pak_dir.vpk", "vmt"))
    prop = re.compile(r'"?\$surfaceprop"?\s+"?([^"\s]+)"?', re.I)
    include = re.compile(r'"?include"?\s+"?([^"\s]+)"?', re.I)
    result = {}
    for path, text in files.items():
        if not path.startswith("materials/"):
            continue
        m = prop.search(text)
        if not m and (inc := include.search(text)):
            m = prop.search(files.get(inc.group(1).lower().replace("\\", "/").removesuffix(".vmt"), ""))
        result.setdefault(strip(path.rsplit("/", 1)[-1]), m.group(1).lower() if m else "")
    return result


def used_textures(hl):
    """Every texture the three games' maps use."""
    used = collections.Counter()
    for game in ("valve", "gearbox", "bshift"):
        for path in glob.glob(f"{hl}/{game}/maps/*.bsp"):
            d = open(path, "rb").read()
            offset, _ = struct.unpack_from("<ii", d, 4 + 2 * 8)
            for i in range(struct.unpack_from("<i", d, offset)[0]):
                o = struct.unpack_from("<i", d, offset + 4 + 4 * i)[0]
                if o >= 0:
                    used[strip(d[offset + o:offset + o + 16].split(b"\0")[0].decode("latin1"))] += 1
    return used


# Half-Life: Source's surface types as the materials table's letters; the rest (concrete, brick, rock, paper, tile,
# default) look like concrete on impact, so aren't listed.
FROM_HLS = {
    "metal": "M", "metalpanel": "M", "metal_barrel_hl": "M", "metal_barrellight_hl": "M", "floating_metal_barrel": "M",
    "metalgrate": "G", "metalgrating": "G", "metalvent": "V", "computer": "P",
    "glass": "Y", "wood": "W", "dirt": "D",
}
SKIP = re.compile(r"^(SKY|AAATRIGGER|NULL|CLIP|ORIGIN|BEVEL|HINT|SKIP|TRANSLUCENT|BLACK_HIDDEN|SCROLL|WATER|SLIME|LAVA|TOXIC|FLUID)")
METAL = re.compile(r"METAL|MET_|STEEL|PIPE|IRON|TANK|DUCT|VENT|GRATE|GRT|RUST|ALUM|PLATE|GIRD|TRUSS|ELEV|LADDER|CRANE|CHROME|BOLT|BARREL|LOCKER|MACHIN|TURBIN|VALVE|HATCH|RIVET|CORRUG|DIAMOND|TRACK|TRAIN|LIFT|SHAFT|SHUTTER|CAGE|MESH|FENCE|RAILING")
NOT_METAL = re.compile(r"CONC|CRETE|BRICK|WOOD|DIRT|ROCK|STONE|CARPET|TILE|GLASS|PAPER|POSTER|SIGN|LIGHT|LITE|FABRIC|CEIL|PLASTER|SAND|GRASS|MUD|XEN|OUT_|FLESH|SILO")

HEADER = """// johnhalflife: how bullet impacts, melee and explosion debris treat textures Valve's materials table
// (materials.json) doesn't list; footsteps and ricochet sounds still use Valve's. "Impact" is a material
// letter as in materials.json. Half-Life: Source's surface type for each texture where it has one (Valve's own
// later judgement), else, for Opposing Force's and Blue Shift's own textures, a guess from the name.
// Made by scripts/make_impact_materials.py.
"""


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    hl, hl2, out = sys.argv[1:]
    known = {k.upper() for k in json.loads(re.sub(r"//.*", "", open(f"{hl}/johnhalflife/sound/materials.json").read()))}
    hls = hls_surfaceprops(hl2)

    table = {}
    sources = collections.Counter()
    for name in sorted(used_textures(hl)):
        if name in known or SKIP.match(name) or not re.fullmatch(r"\w+", name):
            continue
        if name in hls:
            if letter := FROM_HLS.get(hls[name]):
                table[name] = {"Impact": letter}
                sources["Half-Life: Source " + letter] += 1
        elif METAL.search(name) and not NOT_METAL.search(name):
            table[name] = {"Impact": "M"}
            sources["name guess M"] += 1

    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER)
        json.dump(table, f, indent="\t", sort_keys=True)
        f.write("\n")
    print(f"{out}: {len(table)} textures", dict(sources))


if __name__ == "__main__":
    main()
