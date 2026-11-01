"""Gives an installed John Half-Life's models their vanilla movement speeds back.

A sequence's ground speed (how fast an NPC walks or runs with it) is its linear movement times its frame rate over its
frame count less one (server animation.cpp GetSequenceInfo). The Unified SDK's models were recompiled from decompiled
sources, which works the linear movement out over one frame fewer: it comes out (frames - 1) / frames of vanilla's, so
every NPC moved 2 to 6% slower than in vanilla (a running grunt 287 units/s in place of 304).

For each model in the mod folder, each sequence that a vanilla model of the same name also has, with the same frame
count and frame rate and with that shortfall, gets the vanilla linear movement. Sequences made or changed by the
Unified SDK keep theirs. Vanilla models are read from the player's own games; nothing of Valve's is committed to the
repository. Running it again changes nothing.

Usage: python restore_npc_speeds.py <mod folder>
"""

import os
import struct
import sys

SEQUENCE_SIZE = 176
FPS = 32
NUMFRAMES = 56
LINEAR = 76


def sequences(data):
    numseq, seqindex = struct.unpack_from("<2i", data, 164)
    for i in range(numseq):
        o = seqindex + i * SEQUENCE_SIZE
        label = bytes(data[o:o + 32]).split(b"\0", 1)[0].lower()
        fps = struct.unpack_from("<f", data, o + FPS)[0]
        frames = struct.unpack_from("<i", data, o + NUMFRAMES)[0]
        yield label, o, fps, frames, struct.unpack_from("<3f", data, o + LINEAR)


def length(v):
    return sum(x * x for x in v) ** 0.5


def restore(path, vanilla_path):
    data = bytearray(open(path, "rb").read())
    vanilla = open(vanilla_path, "rb").read()
    if data[:4] != b"IDST" or vanilla[:4] != b"IDST":
        return 0
    wanted = {label: (fps, frames, linear) for label, _, fps, frames, linear in sequences(vanilla)}
    fixed = 0
    for label, o, fps, frames, linear in sequences(data):
        v = wanted.get(label)
        if not v or v[0] != fps or v[1] != frames or frames < 2 or v[2] == linear:
            continue
        expected = length(v[2]) * (frames - 1) / frames
        if expected < 1 or abs(length(linear) - expected) > 0.01 * expected:
            continue
        struct.pack_into("<3f", data, o + LINEAR, *v[2])
        fixed += 1
    if fixed:
        open(path, "wb").write(data)
    return fixed


def main():
    mod = sys.argv[1]
    root = os.path.dirname(os.path.abspath(mod))
    total = 0
    # A model in models/op4 or models/bshift is that game's; one in models/ is Half-Life's, or the expansion's own
    # if Half-Life has none of that name.
    for sub, games in (("", ("valve", "gearbox", "bshift")), ("op4", ("gearbox",)), ("bshift", ("bshift",))):
        folder = os.path.join(mod, "models", sub)
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".mdl"):
                continue
            vanilla = next((p for p in (os.path.join(root, g, "models", name) for g in games) if os.path.exists(p)), None)
            if vanilla:
                total += restore(os.path.join(folder, name), vanilla)
    print(f"restore_npc_speeds: {total} sequences given their vanilla movement")


main()
