"""Extracts one submodel of a compiled GoldSrc model as an SMD reference mesh on another model's skeleton (a reference
SMD's nodes and skeleton), each vertex kept where it is relative to its bone, matched by name. Also writes the
textures it uses as 8-bit BMPs.

  python mdl2smd.py <model.mdl> <submodel name> <skeleton.smd> <out.smd> <texture dir> [bone map: src=dst ...]
  python mdl2smd.py --list <model.mdl>
"""
import math, os, struct, sys


def read_model(path):
    d = open(path, 'rb').read()
    m = {'data': d}
    nb, bi = struct.unpack_from('<ii', d, 140)
    bones = []
    for i in range(nb):
        o = bi + i * 112
        name = d[o:o + 32].split(b'\0')[0].decode('latin-1')
        parent = struct.unpack_from('<i', d, o + 32)[0]
        value = struct.unpack_from('<6f', d, o + 64)
        bones.append({'name': name, 'parent': parent, 'value': value})
    m['bones'] = bones
    ntex, ti = struct.unpack_from('<ii', d, 180)
    texs = []
    for i in range(ntex):
        o = ti + i * 80
        name = d[o:o + 64].split(b'\0')[0].decode('latin-1')
        flags, w, h, idx = struct.unpack_from('<iiii', d, o + 64)
        texs.append({'name': name, 'flags': flags, 'w': w, 'h': h, 'idx': idx})
    m['textures'] = texs
    nref, nfam, si = struct.unpack_from('<iii', d, 192)
    m['skins'] = [struct.unpack_from('<h', d, si + 2 * k)[0] for k in range(nref)]
    nbp, bpi = struct.unpack_from('<ii', d, 204)
    subs = {}
    for i in range(nbp):
        o = bpi + i * 76
        nm, base, mi = struct.unpack_from('<iii', d, o + 64)
        for j in range(nm):
            mo = mi + j * 112
            name = d[mo:mo + 64].split(b'\0')[0].decode('latin-1')
            nmesh, meshi, nverts, vinfo, vidx, nnorms, ninfo, nidx = struct.unpack_from('<8i', d, mo + 72)
            subs[name] = (nmesh, meshi, nverts, vinfo, vidx, nnorms, ninfo, nidx)
    m['subs'] = subs
    return m


def angle_matrix(a):
    sy, cy = math.sin(a[2]), math.cos(a[2])
    sp, cp = math.sin(a[1]), math.cos(a[1])
    sr, cr = math.sin(a[0]), math.cos(a[0])
    return [[cp * cy, sr * sp * cy + cr * -sy, cr * sp * cy + -sr * -sy],
            [cp * sy, sr * sp * sy + cr * cy, cr * sp * sy + -sr * cy],
            [-sp, sr * cp, cr * cp]]


def concat(p, c):
    # 3x4 matrices: rotation rows plus translation column.
    r = [[sum(p[i][k] * c[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    t = [sum(p[i][k] * c[k][3] for k in range(3)) + p[i][3] for i in range(3)]
    return [r[i] + [t[i]] for i in range(3)]


def world_matrices(bones):
    out = []
    for b in bones:
        rot = angle_matrix(b['value'][3:])
        local = [rot[i] + [b['value'][i]] for i in range(3)]
        out.append(local if b['parent'] < 0 else concat(out[b['parent']], local))
    return out


def read_skeleton(smd):
    lines = open(smd, encoding='latin-1').read().splitlines()
    nodes, pose, mode = [], {}, None
    for line in lines:
        s = line.strip()
        if s in ('nodes', 'skeleton', 'triangles'):
            mode = s
            continue
        if s == 'end':
            if mode == 'skeleton':
                break
            mode = None
            continue
        if mode == 'nodes':
            i = int(s.split()[0])
            name = s[s.index('"') + 1:s.rindex('"')]
            parent = int(s.split()[-1])
            nodes.append((i, name, parent))
        elif mode == 'skeleton' and not s.startswith('time'):
            v = s.split()
            pose[int(v[0])] = tuple(float(x) for x in v[1:7])
    bones = [{'name': n, 'parent': p, 'value': pose[i]} for i, n, p in nodes]
    head = []
    for line in lines:
        head.append(line)
        if line.strip() == 'end' and any(l.strip() == 'skeleton' for l in head):
            break
    return bones, head


def write_bmp(path, w, h, pixels, palette):
    row = (w + 3) & ~3
    data = bytearray()
    for y in range(h - 1, -1, -1):
        data += pixels[y * w:(y + 1) * w] + bytes(row - w)
    pal = bytearray()
    for i in range(256):
        r, g, b = palette[i * 3:i * 3 + 3]
        pal += bytes((b, g, r, 0))
    header = struct.pack('<2sIHHI', b'BM', 14 + 40 + 1024 + len(data), 0, 0, 14 + 40 + 1024)
    info = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 8, 0, len(data), 2835, 2835, 256, 256)
    open(path, 'wb').write(header + info + pal + data)


def main():
    if sys.argv[1] == '--list':
        m = read_model(sys.argv[2])
        print('bones:', [b['name'] for b in m['bones']])
        print('submodels:', list(m['subs']))
        return
    src, subname, skel, out, texdir = sys.argv[1:6]
    args = sys.argv[6:]
    # --offset x y z: placed where it is on its own skeleton, moved by that, instead of kept relative to its bones (as
    # the Unified SDK moved Blue Shift's heads onto its low-definition body).
    offset = None
    if args and args[0] == '--offset':
        offset = [float(x) for x in args[1:4]]
        args = args[4:]
    bonemap = dict(a.split('=', 1) for a in args)
    m = read_model(src)
    d = m['data']
    sworld = world_matrices(m['bones'])
    dbones, header = read_skeleton(skel)
    dworld = world_matrices(dbones)
    dindex = {b['name'].lower(): i for i, b in enumerate(dbones)}

    def target(srcbone):
        name = bonemap.get(srcbone, srcbone)
        if name.lower() in dindex:
            return dindex[name.lower()]
        # Bip01 Head -> Bip02 Head: the same biped, numbered differently.
        parts = name.split(' ', 1)
        for k, i in dindex.items():
            if len(parts) == 2 and k.split(' ', 1)[-1] == parts[1].lower() and ' ' in k:
                return i
        raise SystemExit(f'no bone for {srcbone}')

    nmesh, meshi, nverts, vinfo, vidx, nnorms, ninfo, nidx = m['subs'][subname]
    vbone = d[vinfo:vinfo + nverts]
    verts = [struct.unpack_from('<3f', d, vidx + 12 * i) for i in range(nverts)]
    nbone = d[ninfo:ninfo + nnorms]
    norms = [struct.unpack_from('<3f', d, nidx + 12 * i) for i in range(nnorms)]
    mapped = {}
    tris = []
    used_tex = set()
    for k in range(nmesh):
        numtris, triindex, skinref, mnorms, mnormi = struct.unpack_from('<5i', d, meshi + 20 * k)
        tex = m['textures'][m['skins'][skinref]]
        used_tex.add(m['skins'][skinref])
        o = triindex
        while True:
            count = struct.unpack_from('<h', d, o)[0]
            o += 2
            if count == 0:
                break
            fan = count < 0
            count = abs(count)
            pts = [struct.unpack_from('<4h', d, o + 8 * j) for j in range(count)]
            o += 8 * count
            for j in range(count - 2):
                if fan:
                    tri = (pts[0], pts[j + 1], pts[j + 2])
                elif j % 2 == 0:
                    tri = (pts[j], pts[j + 1], pts[j + 2])
                else:
                    tri = (pts[j + 1], pts[j], pts[j + 2])
                tris.append((tex, tri))

    def place(vi, ni):
        sb = m['bones'][vbone[vi]]['name']
        b = mapped.setdefault(sb, target(sb))
        M = dworld[b]
        if offset is not None:
            S = sworld[vbone[vi]]
            v = verts[vi]
            p = [sum(S[r][c] * v[c] for c in range(3)) + S[r][3] + offset[r] for r in range(3)]
            n = norms[ni]
            nn = [sum(S[r][c] * n[c] for c in range(3)) for r in range(3)]
            return b, p, nn
        v = verts[vi]
        p = [sum(M[r][c] * v[c] for c in range(3)) + M[r][3] for r in range(3)]
        n = norms[ni]
        nn = [sum(M[r][c] * n[c] for c in range(3)) for r in range(3)]
        return b, p, nn

    lines = list(header) + ['triangles']
    for tex, tri in tris:
        lines.append(tex['name'])
        # Strips and fans as studiomdl writes them wind the other way from the SMD's.
        for vi, ni, s, t in reversed(tri):
            b, p, n = place(vi, ni)
            u = s / tex['w']
            v = 1 - t / tex['h']
            lines.append(f"{b} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {n[0]:.6f} {n[1]:.6f} {n[2]:.6f} {u:.6f} {v:.6f}")
    lines.append('end')
    open(out, 'w', encoding='latin-1', newline='\n').write('\n'.join(lines) + '\n')

    os.makedirs(texdir, exist_ok=True)
    for t in used_tex:
        tex = m['textures'][t]
        px = d[tex['idx']:tex['idx'] + tex['w'] * tex['h']]
        pal = d[tex['idx'] + tex['w'] * tex['h']:tex['idx'] + tex['w'] * tex['h'] + 768]
        write_bmp(os.path.join(texdir, tex['name']), tex['w'], tex['h'], px, pal)
    print(f'{len(tris)} triangles; bones {mapped}; textures {[m["textures"][t]["name"] for t in used_tex]}')


main()
