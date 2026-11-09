"""Joins a head SMD's neck to a body SMD's neck hole: a strip of triangles from the head's open bottom edge to the
body's open edge around the neck, all the way round, added to the head. For a head made for a body whose neck hole
was changed to meet it, put on a body whose hole is wider.

Each strip vertex on the head's edge is that head vertex as it is (bone, texture coordinates); each on the body's edge
takes the body vertex's place and bone, the nearest head edge vertex's texture coordinates (the neck's last row of
skin, stretched down), and faces out from the neck.

  python smd_neck_bridge.py <head.smd> <body.smd> <out.smd> [neck axis x y, default: the body hole's centre]
"""
import math, sys
from collections import Counter, defaultdict


def parse(path):
    lines = open(path, encoding='latin-1').read().splitlines()
    nodes, i = {}, lines.index('nodes') + 1
    while lines[i].strip() != 'end':
        s = lines[i].strip()
        nodes[int(s.split()[0])] = s[s.index('"') + 1:s.rindex('"')]
        i += 1
    t = lines.index('triangles')
    tris, j = [], t + 1
    while lines[j].strip() != 'end':
        tex = lines[j].strip()
        verts = []
        for k in (1, 2, 3):
            v = lines[j + k].split()
            verts.append((int(v[0]), tuple(map(float, v[1:4])), tuple(map(float, v[4:7])), tuple(map(float, v[7:9]))))
        tris.append((tex, verts))
        j += 4
    return lines[:t], nodes, tris


def key(p):
    return tuple(round(x, 3) for x in p)


def open_edges(tris):
    count = Counter()
    for _, v in tris:
        for a, b in ((0, 1), (1, 2), (2, 0)):
            ka, kb = key(v[a][1]), key(v[b][1])
            if ka != kb:
                count[frozenset((ka, kb))] += 1
    return [tuple(e) for e, n in count.items() if n == 1]


def loop(edges, keep):
    """The open edges' vertices that pass keep, in order round the loop they make."""
    adj = defaultdict(set)
    for a, b in edges:
        if keep(a) and keep(b):
            adj[a].add(b)
            adj[b].add(a)
    if not adj:
        raise SystemExit('no open edge found')
    start = min(adj)
    order, prev, cur = [start], None, start
    while True:
        nxt = [n for n in adj[cur] if n != prev]
        if not nxt or nxt[0] == start:
            break
        prev, cur = cur, nxt[0]
        order.append(cur)
    return order


def main():
    head_path, body_path, out = sys.argv[1:4]
    header, hnodes, htris = parse(head_path)
    _, bnodes, btris = parse(body_path)
    hindex = {n.lower(): i for i, n in hnodes.items()}

    # Vertex attributes by position.
    hvert, htex = {}, {}
    for tex, v in htris:
        for x in v:
            hvert.setdefault(key(x[1]), x)
            htex.setdefault(key(x[1]), tex)
    bbone = {}
    for _, v in btris:
        for x in v:
            bbone.setdefault(key(x[1]), hindex[bnodes[x[0]].lower()])

    hedges = open_edges(htris)
    bedges = open_edges(btris)
    hz = [p[2] for e in hedges for p in e]
    # The head's bottom edge: its open edges' lower part (a head's only opening is its neck).
    hloop = loop(hedges, lambda p: p[2] < min(hz) + 4)
    hc = [sum(p[k] for p in hloop) / len(hloop) for k in range(2)]
    # The body's hole: the open edge nearest the head's around the same axis, at about its height.
    bloop = loop(bedges, lambda p: abs(p[2] - min(hz)) < 4 and math.hypot(p[0] - hc[0], p[1] - hc[1]) < 6)
    cx, cy = (float(sys.argv[4]), float(sys.argv[5])) if len(sys.argv) > 5 else \
        (sum(p[0] for p in bloop) / len(bloop), sum(p[1] for p in bloop) / len(bloop))

    def angle(p):
        return math.atan2(p[1] - cy, p[0] - cx) % (2 * math.pi)

    def by_angle(ring):
        s = sorted(ring, key=angle)
        return s

    H, B = by_angle(hloop), by_angle(bloop)
    # Zip the two rings: always advance the one whose next vertex comes first round the neck.
    tris = []
    i = j = 0
    nh, nb = len(H), len(B)
    while i < nh or j < nb:
        h0, h1 = H[i % nh], H[(i + 1) % nh]
        b0, b1 = B[j % nb], B[(j + 1) % nb]
        ah = angle(h1) + (2 * math.pi if i + 1 >= nh else 0)
        ab = angle(b1) + (2 * math.pi if j + 1 >= nb else 0)
        if j >= nb or (i < nh and ah <= ab):
            tris.append(('h', h0, h1, b0))
            i += 1
        else:
            tris.append(('b', h0, b1, b0))
            j += 1

    def outward(p):
        d = (p[0] - cx, p[1] - cy, 0.0)
        n = math.hypot(d[0], d[1]) or 1
        return (d[0] / n, d[1] / n, 0.0)

    # The winding the head's own triangles have, facing out: (b - a) x (c - a) against the outward direction.
    def facing(a, b, c):
        u = [b[k] - a[k] for k in range(3)]
        w = [c[k] - a[k] for k in range(3)]
        n = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
        m = [(a[k] + b[k] + c[k]) / 3 for k in range(3)]
        o = outward(m)
        return n[0] * o[0] + n[1] * o[1]
    ring = set(H)
    sample = [v for tex, v in htris if sum(key(x[1]) in ring for x in v) >= 2]
    sign = 1 if sum(facing(*(x[1] for x in v)) for v in sample) > 0 else -1

    lines = list(header) + ['triangles']
    for tex, v in htris:
        lines.append(tex)
        for b, p, n, uv in v:
            lines.append(f"{b} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {n[0]:.6f} {n[1]:.6f} {n[2]:.6f} {uv[0]:.6f} {uv[1]:.6f}")
    added = 0
    for kind, a, b, c in tris:
        pts = [a, b, c]
        heads = [p for p in pts if p in hvert]
        tex = htex[heads[0]]
        ref = hvert[heads[0]]
        verts = []
        for p in pts:
            if p in hvert and htex[p] == tex:
                bone, pos, nrm, uv = hvert[p]
            elif p in hvert:
                bone, pos, nrm, uv = hvert[p][0], hvert[p][1], hvert[p][2], ref[3]
            else:
                near = min(heads, key=lambda q: math.dist(q, p))
                bone, pos, nrm, uv = bbone[p], p, outward(p), hvert[near][3] if htex[near] == tex else ref[3]
            verts.append((bone, pos, nrm, uv))
        if facing(*(x[1] for x in verts)) * sign < 0:
            verts = [verts[0], verts[2], verts[1]]
        lines.append(tex)
        for bone, p, n, uv in verts:
            lines.append(f"{bone} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {n[0]:.6f} {n[1]:.6f} {n[2]:.6f} {uv[0]:.6f} {uv[1]:.6f}")
        added += 1
    lines.append('end')
    open(out, 'w', encoding='latin-1', newline='\n').write('\n'.join(lines) + '\n')
    print(f'smd_neck_bridge: head edge {nh} vertices, body hole {nb}, {added} triangles added (axis {cx:.2f} {cy:.2f})')


main()
