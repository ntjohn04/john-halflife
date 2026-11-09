"""Gives John Half-Life's Blue Shift viewmodels (the Unified SDK's models/bshift/v_*.mdl) Barney's arms at low
definition, as the rest of them: Gearbox gave every one of its low-definition viewmodels high-definition arm textures
(barney_hand 260x247, sleeve 256x254), and its high-definition ones low-definition arms (68x62, 64x64). The same art,
laid out alike; the small pair is taken from the Unified SDK's own high-definition Blue Shift handgun (its release's
hlu_hd, which John Half-Life doesn't ship otherwise).

A model's texture data is the last thing in it, so the two textures are put in place there; the texture coordinates of
the meshes using them, kept in texture pixels, are scaled to the new size. A model whose arms are already small is
left alone, so running it again changes nothing.

Run by the release packager on the Unified SDK's files.

Usage:
  python lowdef_arms.py <mod folder> <Unified SDK release zip>
"""

import os
import struct
import sys
import zipfile

SOURCE = "hlu_hd/models/bshift/v_9mmhandgun.mdl"
# Each big arm texture, by name, and its small counterpart's name in the source.
ARMS = {"barney_hand.bmp": "barney_hand_hd.bmp", "sleeve.bmp": "sleeve_hd.bmp"}
TEXTURE_SIZE = 80
MODEL_SIZE = 112
MESH_SIZE = 20


def textures(data):
    count, index = struct.unpack_from("<2i", data, 180)
    out = []
    for i in range(count):
        o = index + i * TEXTURE_SIZE
        name = bytes(data[o:o + 64]).split(b"\0", 1)[0].decode("latin-1")
        flags, width, height, offset = struct.unpack_from("<4i", data, o + 64)
        out.append({"header": o, "name": name, "width": width, "height": height,
                    "pixels": bytes(data[offset:offset + width * height + 768])})
    return out


def small_arms(release_zip):
    with zipfile.ZipFile(release_zip) as z:
        source = z.read(SOURCE)
    found = {t["name"].lower(): t for t in textures(source)}
    return {big: found[small] for big, small in ARMS.items()}


def patch(path, small):
    data = bytearray(open(path, "rb").read())
    if data[:4] != b"IDST":
        return False
    texs = textures(data)
    new = {}
    for i, t in enumerate(texs):
        s = small.get(t["name"].lower())
        if s and (t["width"], t["height"]) != (s["width"], s["height"]):
            new[i] = s
    if not new:
        return False

    # The texture coordinates of the meshes drawn with them, scaled.
    skinref_count, _, skin_index = struct.unpack_from("<3i", data, 192)
    skins = [struct.unpack_from("<h", data, skin_index + 2 * k)[0] for k in range(skinref_count)]
    bodypart_count, bodypart_index = struct.unpack_from("<2i", data, 204)
    for b in range(bodypart_count):
        model_count, _, model_index = struct.unpack_from("<3i", data, bodypart_index + b * 76 + 64)
        for m in range(model_count):
            mesh_count, mesh_index = struct.unpack_from("<2i", data, model_index + m * MODEL_SIZE + 72)
            for k in range(mesh_count):
                _, tri_index, skinref, _, _ = struct.unpack_from("<5i", data, mesh_index + k * MESH_SIZE)
                ti = skins[skinref]
                if ti not in new:
                    continue
                sx = new[ti]["width"] / texs[ti]["width"]
                sy = new[ti]["height"] / texs[ti]["height"]
                o = tri_index
                while True:
                    n = struct.unpack_from("<h", data, o)[0]
                    o += 2
                    if n == 0:
                        break
                    for _ in range(abs(n)):
                        s, t = struct.unpack_from("<2h", data, o + 4)
                        struct.pack_into("<2h", data, o + 4, round(s * sx), round(t * sy))
                        o += 8

    # The texture data, last in the file, written again with the two in place.
    data_index = struct.unpack_from("<i", data, 188)[0]
    block = bytearray()
    for i, t in enumerate(texs):
        tex = new.get(i, t)
        struct.pack_into("<3i", data, t["header"] + 68, tex["width"], tex["height"], data_index + len(block))
        block += tex["pixels"]
    del data[data_index:]
    data += block
    struct.pack_into("<i", data, 72, len(data))
    open(path, "wb").write(data)
    return True


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    mod, release_zip = sys.argv[1:]
    small = small_arms(release_zip)
    folder = os.path.join(mod, "models", "bshift")
    patched = [f for f in sorted(os.listdir(folder)) if f.lower().startswith("v_") and f.lower().endswith(".mdl")
               and patch(os.path.join(folder, f), small)]
    print(f"lowdef_arms: {len(patched)} Blue Shift viewmodels given low-definition arms")


if __name__ == "__main__":
    main()
