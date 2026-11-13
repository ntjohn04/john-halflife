"""Writes config/mod/jhl_setup_files.txt, the list of files John Half-Life's first start copies from the player's own games (client jhl_setup.cpp).

Usage: python make_setup_files.py <Half-Life folder> <Unified SDK release zip> <output>
"""

import collections
import hashlib
import os
import re
import sys
import zipfile

H, ZIP, OUT = sys.argv[1:4]
MOD = os.path.join(H, "johnhalflife")
OWN = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config", "mod")
EXPANSIONS = ("gearbox", "bshift")

PER_GAME = ["sound/barney/icanhear.wav", "sound/common/null.wav", "sound/misc/talk.wav",
            "sound/scientist/needsleep.wav", "sound/scientist/survival.wav", "sprites/640_logo.spr",
            "sprites/320_logo.spr", "sprites/laserdot.spr"]
EXTRA = [("johnhalflife", "media/valve.webm", ["valve/media/valve.webm"]),
         ("johnhalflife_uplink", "media/valve.webm", ["valve/media/valve.webm"]),
         ("johnhalflife", "models/shell.mdl", ["valve/models/shell.mdl"]),
         ("johnhalflife", "models/shell_op4.mdl", ["gearbox/models/shell.mdl"])]
for name in ("320_pain", "320_train", "320hud2", "320hud3", "320hudof01"):
    EXTRA.append(("johnhalflife", f"sprites/op4/{name}.spr", [f"gearbox/sprites/{name}.spr"]))
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


def sources(data):
    """Every file in the games with these contents."""
    h = hashlib.md5(data).hexdigest()
    found = []
    for game, r, p in by_size.get(len(data), []):
        if p not in hashes:
            hashes[p] = md5(p)
        if hashes[p] == h:
            found.append(f"{game}/{r}")
    return found


sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import package_release  # noqa: E402  (its list of the release's files never shipped)

entries = []
per_game = {(d, f.lower()) for d, f, _ in EXTRA}
release = package_release.release_files(ZIP)

with zipfile.ZipFile(ZIP) as z:
    for r, info in sorted(release.items()):
        if any(re.search(p, r, re.I) for p in package_release.DROP) or ("johnhalflife", r.lower()) in per_game:
            continue
        if os.path.isfile(os.path.join(OWN, *r.split("/"))):
            continue
        data = z.read(info)
        same = os.path.join(H, "valve", *r.split("/"))
        if os.path.isfile(same) and os.path.getsize(same) == len(data) and open(same, "rb").read() == data:
            continue
        found = sources(data)
        if found:
            found.sort(key=lambda s: (s.split("/", 1)[1].lower() != r.lower(), s.startswith("valve/"), s))
            entries.append(("johnhalflife", r, found))

for f in sorted(os.listdir(os.path.join(MOD, "media"))):
    data = open(os.path.join(MOD, "media", f), "rb").read()
    found = sources(data)
    if found and not any(s.startswith("valve/") for s in found):
        found.sort(key=lambda s: (s.split("/", 1)[1].lower() != f"media/{f}".lower(), s))
        entries.append(("johnhalflife", f"media/{f}", found))
entries += EXTRA

with open(OUT, "w", encoding="utf-8", newline="\n") as o:
    o.write("// Files John Half-Life's first start copies from the player's own games (client jhl_setup.cpp), made by\n"
            "// scripts/make_setup_files.py. Each line: the game folder, the file, then where to copy it from, in order\n"
            "// of preference; a file none of whose sources is installed is left out.\n")
    for folder, r, found in sorted(entries, key=lambda e: (e[0], e[1].lower())):
        o.write("\t".join([folder, r] + found) + "\n")
print(f"{len(entries)} files")
