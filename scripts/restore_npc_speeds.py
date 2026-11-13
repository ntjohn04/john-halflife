"""Gives John Half-Life's models (the Unified SDK's) their vanilla movement speeds back.

Usage:
  python restore_npc_speeds.py <mod folder>
  python restore_npc_speeds.py --make-table <Half-Life folder> <Unified SDK release zip>
      (writes npc_speeds.txt, comparing the release's models with the player's vanilla ones)
"""

import os
import struct
import sys
import zipfile

SEQUENCE_SIZE = 176
FPS = 32
NUMFRAMES = 56
LINEAR = 76
TABLE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "npc_speeds.txt")
SAME_ANIMATIONS = {"models/rosenberg.mdl": "models/scientist.mdl", "models/harold.mdl": "models/scientist.mdl"}


def sequences(data):
    numseq, seqindex = struct.unpack_from("<2i", data, 164)
    for i in range(numseq):
        o = seqindex + i * SEQUENCE_SIZE
        label = bytes(data[o:o + 32]).split(b"\0", 1)[0].decode("latin-1").lower()
        fps = struct.unpack_from("<f", data, o + FPS)[0]
        frames = struct.unpack_from("<i", data, o + NUMFRAMES)[0]
        yield label, o, fps, frames, struct.unpack_from("<3f", data, o + LINEAR)


def length(v):
    return sum(x * x for x in v) ** 0.5


def has_shortfall(linear, frames, vanilla):
    expected = length(vanilla) * (frames - 1) / frames
    return expected >= 1 and abs(length(linear) - expected) <= 0.01 * expected


def make_table(half_life, release_zip):
    """Each of the release's models against the vanilla one of its name: the sequences short by a frame."""
    rows = []
    with zipfile.ZipFile(release_zip) as z:
        for info in sorted(z.infolist(), key=lambda i: i.filename):
            name = info.filename.replace("\\", "/")
            name = name.split("/", 1)[1] if name.lower().startswith("hlu/") else name
            if not name.lower().startswith("models/") or not name.lower().endswith(".mdl"):
                continue
            parts = SAME_ANIMATIONS.get(name, name).split("/")
            if len(parts) == 3 and parts[1] in ("op4", "bshift"):
                games = ("gearbox",) if parts[1] == "op4" else ("bshift",)
            elif len(parts) == 2:
                games = ("valve", "gearbox", "bshift")
            else:
                continue
            vanilla_path = next((p for p in (os.path.join(half_life, g, "models", parts[-1]) for g in games) if os.path.exists(p)), None)
            if not vanilla_path:
                continue
            data = z.read(info)
            vanilla = open(vanilla_path, "rb").read()
            if data[:4] != b"IDST" or vanilla[:4] != b"IDST":
                continue
            wanted = {label: (fps, frames, linear) for label, _, fps, frames, linear in sequences(vanilla)}
            for label, _, fps, frames, linear in sequences(data):
                v = wanted.get(label)
                if v and v[0] == fps and v[1] == frames and frames >= 2 and v[2] != linear and has_shortfall(linear, frames, v[2]):
                    rows.append((name, label, v[2]))
    with open(TABLE, "w", encoding="utf-8", newline="\n") as out:
        out.write("// The vanilla linear movement of each sequence of the Unified SDK's models that's a frame short of it\n"
                  "// (restore_npc_speeds.py). Each line: the model, the sequence, x y z.\n")
        for name, label, v in rows:
            out.write(f"{name}\t{label}\t" + " ".join(f"{x:.9g}" for x in v) + "\n")
    print(f"restore_npc_speeds: {len(rows)} sequences in {TABLE}")


def restore(mod):
    table = {}
    for line in open(TABLE, encoding="utf-8"):
        if line.startswith("//") or not line.strip():
            continue
        name, label, values = line.rstrip("\n").split("\t")
        table.setdefault(name, {})[label] = tuple(struct.unpack("<3f", struct.pack("<3f", *map(float, values.split()))))
    for name, source in SAME_ANIMATIONS.items():
        if name not in table and source in table:
            table[name] = table[source]

    fixed = 0
    for name, wanted in table.items():
        path = os.path.join(mod, *name.split("/"))
        if not os.path.exists(path):
            continue
        data = bytearray(open(path, "rb").read())
        if data[:4] != b"IDST":
            continue
        changed = 0
        for label, o, fps, frames, linear in sequences(data):
            v = wanted.get(label)
            if v and frames >= 2 and linear != v and has_shortfall(linear, frames, v):
                struct.pack_into("<3f", data, o + LINEAR, *v)
                changed += 1
        if changed:
            open(path, "wb").write(data)
            fixed += changed
    print(f"restore_npc_speeds: {fixed} sequences given their vanilla movement")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--make-table":
        make_table(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 2:
        restore(sys.argv[1])
    else:
        print(__doc__)
        sys.exit(2)
