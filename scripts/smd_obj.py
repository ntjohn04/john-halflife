"""Round trip between SMD reference meshes and Wavefront OBJ, for editing a model's mesh in Blender (File > Import >
Wavefront (.obj), and File > Export > Wavefront (.obj), both with their default settings).

  python smd_obj.py export <out folder> <name> <edit.smd> [<reference.smd> ...] [--textures <dir> ...]
      Writes <name>.obj and <name>.mtl, and each texture the meshes use as a PNG in textures/. The first SMD is the
      object to edit (named "edit"); the others come along as objects named "reference_..." to see it against (the
      body under a head, say), left out on the way back. Texture search directories: --textures, in order.
  python smd_obj.py import <edited.obj> <original edit.smd> <out.smd> [--textures-out <dir>]
      The "edit" object back as an SMD on the original's skeleton: each vertex on the bone of the original's vertex
      nearest it (Blender keeps no bones in an OBJ). Faces with more than three corners are split into triangles.
      With --textures-out, each texture PNG next to the OBJ that differs from what was exported is written there as
      the 8-bit BMP studiomdl takes (its own palette when its colours fit, else a new one).

OBJ is Y-up and Blender converts on both import and export: GoldSrc (x, y, z) is written (x, z, -y).
"""
import hashlib, math, os, sys
from collections import defaultdict


def parse_smd(path):
    lines = open(path, encoding='latin-1').read().splitlines()
    t = lines.index('triangles')
    tris = []
    i = t + 1
    while lines[i].strip() != 'end':
        tex = lines[i].strip()
        verts = []
        for k in (1, 2, 3):
            v = lines[i + k].split()
            verts.append((int(v[0]), tuple(map(float, v[1:4])), tuple(map(float, v[4:7])), tuple(map(float, v[7:9]))))
        tris.append((tex, verts))
        i += 4
    return lines[:t], tris


def find_texture(name, dirs):
    for d in dirs:
        for f in os.listdir(d):
            if f.lower() == name.lower():
                return os.path.join(d, f)
    return None


def export(argv):
    out, name = argv[0], argv[1]
    rest = argv[2:]
    dirs = []
    if '--textures' in rest:
        k = rest.index('--textures')
        dirs = rest[k + 1:]
        rest = rest[:k]
    from PIL import Image
    os.makedirs(os.path.join(out, 'textures'), exist_ok=True)
    obj = [f'mtllib {name}.mtl']
    used = set()
    # Shared by place (and texture coordinates and normals by value), so Blender sees one connected mesh per object.
    vindex, tindex, nindex = {}, {}, {}
    count = {'v': 0, 'vt': 0, 'vn': 0}
    def ref(table, prefix, value):
        if value not in table:
            count[prefix] += 1
            table[value] = count[prefix]
            obj.append(prefix + ' ' + ' '.join(f'{x:.6f}' for x in value))
        return table[value]
    for n, path in enumerate(rest):
        _, tris = parse_smd(path)
        label = 'edit' if n == 0 else 'reference_' + os.path.splitext(os.path.basename(path))[0].replace(' ', '_')
        obj.append(f'o {label}')
        current = None
        faces = []
        for tex, verts in tris:
            idx = []
            for bone, p, nrm, uv in verts:  # both wind counter-clockwise seen from outside
                vi = ref(vindex, 'v', (round(p[0], 6), round(p[2], 6), round(-p[1], 6)))
                ti = ref(tindex, 'vt', (round(uv[0], 6), round(uv[1], 6)))
                ni = ref(nindex, 'vn', (round(nrm[0], 6), round(nrm[2], 6), round(-nrm[1], 6)))
                idx.append(f'{vi}/{ti}/{ni}')
            faces.append((tex, idx))
        for tex, idx in faces:
            if tex != current:
                obj.append(f'usemtl {tex}')
                current = tex
                used.add(tex)
            obj.append('f ' + ' '.join(idx))
        # The next object's vertices aren't shared with this one's (OBJ numbers them across the file).
        vindex.clear()
    open(os.path.join(out, f'{name}.obj'), 'w', newline='\n').write('\n'.join(obj) + '\n')
    mtl = []
    sums = {}
    for tex in sorted(used):
        src = find_texture(tex, dirs)
        png = os.path.splitext(tex)[0] + '.png'
        mtl += [f'newmtl {tex}', 'Kd 1 1 1', f'map_Kd textures/{png}', '']
        if src:
            Image.open(src).convert('RGB').save(os.path.join(out, 'textures', png))
            sums[png] = hashlib.md5(open(os.path.join(out, 'textures', png), 'rb').read()).hexdigest()
        else:
            print(f'smd_obj: texture {tex} not found')
    open(os.path.join(out, f'{name}.mtl'), 'w', newline='\n').write('\n'.join(mtl))
    # What was exported, so the way back knows which textures were edited.
    open(os.path.join(out, 'textures', 'exported.txt'), 'w', newline='\n').write(
        '\n'.join(f'{k}\t{v}' for k, v in sorted(sums.items())) + '\n')
    print(f'smd_obj: {os.path.join(out, name)}.obj, {len(used)} textures')


def import_(argv):
    obj_path, orig_path, out_path = argv[:3]
    tex_out = argv[argv.index('--textures-out') + 1] if '--textures-out' in argv else None
    header, otris = parse_smd(orig_path)
    obones = []
    for _, verts in otris:
        for bone, p, _, _ in verts:
            obones.append((p, bone))
    V, VT, VN = [], [], []
    faces = []
    obj_name, mat = None, None
    for line in open(obj_path, encoding='utf-8', errors='replace'):
        s = line.split()
        if not s:
            continue
        if s[0] == 'o' or s[0] == 'g':
            obj_name = ' '.join(s[1:])
        elif s[0] == 'v':
            x, y, z = map(float, s[1:4])
            V.append((x, -z, y))
        elif s[0] == 'vt':
            VT.append((float(s[1]), float(s[2])))
        elif s[0] == 'vn':
            x, y, z = map(float, s[1:4])
            VN.append((x, -z, y))
        elif s[0] == 'usemtl':
            mat = ' '.join(s[1:])
        elif s[0] == 'f':
            if obj_name is not None and not obj_name.startswith('edit'):
                continue
            corners = []
            for c in s[1:]:
                parts = (c.split('/') + ['', ''])[:3]
                vi = int(parts[0])
                ti = int(parts[1]) if parts[1] else None
                ni = int(parts[2]) if parts[2] else None
                corners.append((V[vi - 1 if vi > 0 else vi], VT[ti - 1 if ti > 0 else ti] if ti else (0, 0),
                                VN[ni - 1 if ni > 0 else ni] if ni else None))
            for k in range(1, len(corners) - 1):
                faces.append((mat, (corners[0], corners[k], corners[k + 1])))
    if not faces:
        raise SystemExit('smd_obj: no faces in an object named edit')
    # Blender names materials as they came, possibly with a .001 suffix; the texture is the name before that.
    def texname(m):
        base = m.split('.bmp')[0] + '.bmp' if '.bmp' in m.lower() else m
        return base

    lines = list(header) + ['triangles']
    for m, tri in faces:
        lines.append(texname(m))
        for p, uv, nrm in tri:
            bone = min(obones, key=lambda b: math.dist(b[0], p))[1]
            if nrm is None:
                nrm = (0.0, 0.0, 1.0)
            lines.append(f'{bone} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {nrm[0]:.6f} {nrm[1]:.6f} {nrm[2]:.6f} {uv[0]:.6f} {uv[1]:.6f}')
    lines.append('end')
    open(out_path, 'w', encoding='latin-1', newline='\n').write('\n'.join(lines) + '\n')
    print(f'smd_obj: {out_path}, {len(faces)} triangles')

    if tex_out:
        from PIL import Image
        tdir = os.path.join(os.path.dirname(obj_path), 'textures')
        sums = dict(l.rstrip('\n').split('\t') for l in open(os.path.join(tdir, 'exported.txt')) if '\t' in l)
        used = {texname(m) for m, _ in faces}
        os.makedirs(tex_out, exist_ok=True)
        for tex in sorted(used):
            png = os.path.splitext(tex)[0] + '.png'
            path = os.path.join(tdir, png)
            if not os.path.exists(path) or hashlib.md5(open(path, 'rb').read()).hexdigest() == sums.get(png):
                continue
            img = Image.open(path).convert('RGB')
            q = img.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
            q.save(os.path.join(tex_out, tex))
            print(f'smd_obj: edited texture {tex} written')


if __name__ == '__main__':
    {'export': export, 'import': import_}[sys.argv[1]](sys.argv[2:])
