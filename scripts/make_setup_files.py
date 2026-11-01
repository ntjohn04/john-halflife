"""Writes config/mod/jhl_setup_files.txt, the list of files John Half-Life's first start copies from the player's
own games (client jhl_setup.cpp): Opposing Force's and Blue Shift's files the Unified SDK's release puts in the mod
folder, their soundtracks, and each game folder's own versions of files the games share names for.

Made from a development install: each file in the mod folder that the Unified SDK's release zip has and that is a
byte-for-byte copy of one in gearbox or bshift (and not in valve, which the engine finds by itself). Only names are
listed; nothing of Valve's is in the list.

Usage: python make_setup_files.py <Half-Life folder> <Unified SDK release zip> <output>
"""

import collections
import hashlib
import os
import sys
import zipfile

H, ZIP, OUT = sys.argv[1:4]
MOD = os.path.join(H, "johnhalflife")
EXPANSIONS = ("gearbox", "bshift")

# Each game folder's own versions of files the three games share names for (cmake/JhlInstallGameFiles.cmake
# before), and the shells (weapons.md#spent-casings).
PER_GAME = ["sound/barney/icanhear.wav", "sound/common/null.wav", "sound/misc/talk.wav",
            "sound/scientist/needsleep.wav", "sound/scientist/survival.wav", "sprites/640_logo.spr",
            "sprites/laserdot.spr"]
EXTRA = [("johnhalflife", "models/shell.mdl", ["valve/models/shell.mdl"]),
         ("johnhalflife", "models/shell_op4.mdl", ["gearbox/models/shell.mdl"])]
for f in PER_GAME:
    EXTRA.append(("johnhalflife", f, [f"valve/{f}"]))
    EXTRA.append(("johnhalflife_op4", f, [f"gearbox/{f}"]))
    EXTRA.append(("johnhalflife_bs", f, [f"bshift/{f}"]))


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def rel(path, root):
    return os.path.relpath(path, root).replace("\\", "/")


in_zip = set()
for info in zipfile.ZipFile(ZIP).infolist():
    name = info.filename.replace("\\", "/")
    name = name.split("/", 1)[1] if name.lower().startswith("hlu/") else name
    in_zip.add(name.lower())

by_size = collections.defaultdict(list)
for game in ("valve",) + EXPANSIONS:
    root = os.path.join(H, game)
    for dp, _, files in os.walk(root):
        for f in files:
            p = os.path.join(dp, f)
            by_size[os.path.getsize(p)].append((game, rel(p, root), p))
hashes = {}


def sources(path):
    h = md5(path)
    found = []
    for game, r, p in by_size.get(os.path.getsize(path), []):
        if p not in hashes:
            hashes[p] = md5(p)
        if hashes[p] == h:
            found.append(f"{game}/{r}")
    return found


entries = []
per_game = {(d, f.lower()) for d, f, _ in EXTRA}
for dp, _, files in os.walk(MOD):
    for f in files:
        p = os.path.join(dp, f)
        r = rel(p, MOD)
        if r.lower() not in in_zip and not r.lower().startswith("media/"):
            continue
        if ("johnhalflife", r.lower()) in per_game:
            continue
        found = sources(p)
        if found and not any(s.startswith("valve/") for s in found):
            # The same path first, then any other copy (a soundtrack is renamed).
            found.sort(key=lambda s: (s.split("/", 1)[1].lower() != r.lower(), EXPANSIONS.index(s.split("/")[0])))
            entries.append(("johnhalflife", r, found))
entries += EXTRA

with open(OUT, "w", encoding="utf-8", newline="\n") as o:
    o.write("// Files John Half-Life's first start copies from the player's own games (client jhl_setup.cpp), made by\n"
            "// scripts/make_setup_files.py. Each line: the game folder, the file, then where to copy it from, in order\n"
            "// of preference; a file none of whose sources is installed is left out.\n")
    for folder, r, found in sorted(entries, key=lambda e: (e[0], e[1].lower())):
        o.write("\t".join([folder, r] + found) + "\n")
print(f"{len(entries)} files")
