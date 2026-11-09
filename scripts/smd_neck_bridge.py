"""Lengthens a head SMD's neck down to where another head's ended: a strip of triangles from the head's open bottom
edge to the reference head's, all the way round, added to the head. For a head made for a body whose neck hole was
changed to meet it, put on a body that another head (the reference) is known to sit right on: the reference's neck
reaches into the body's collar and covers its openings (the body's hole isn't a simple ring, and isn't welded shut by
a head either; it's covered).

Each strip vertex on the head's edge is that head vertex as it is (bone, texture coordinates); each on the reference's
edge takes the reference vertex's place and bone, the nearest head edge vertex's texture coordinates (the neck's last
row of skin, stretched down), and faces out from the neck.

  python smd_neck_bridge.py <head.smd> <reference head.smd> <out.smd>
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


def bottom_loop(tris):
    """A head's open bottom edge (its neck: a head's only opening, besides any open bits higher up), in order. Points
    closer than 0.02 are one (a mesh splits its vertices along texture seams); each is given as one of its own."""
    reps = []
    def weld(p):
        for r in reps:
            if math.dist(r, p) < 0.02:
                return r
        reps.append(p)
        return p
    count = Counter()
    for _, v in tris:
        for a, b in ((0, 1), (1, 2), (2, 0)):
            ka, kb = weld(key(v[a][1])), weld(key(v[b][1]))
            if ka != kb:
                count[frozenset((ka, kb))] += 1
    edges = [tuple(e) for e, n in count.items() if n == 1]
    low = min(p[2] for e in edges for p in e)
    adj = defaultdict(set)
    for a, b in edges:
        if a[2] < low + 4 and b[2] < low + 4:
            adj[a].add(b)
            adj[b].add(a)
    # Only the ring with the lowest point (a mouth's opening can come within that height).
    start = min(adj, key=lambda p: p[2])
    comp, stack = set(), [start]
    while stack:
        p = stack.pop()
        if p not in comp:
            comp.add(p)
            stack.extend(adj[p])
    if any(len(adj[p]) != 2 for p in comp):
        raise SystemExit('the neck edge is not a simple ring')
    order, prev = [start], None
    while True:
        nxt = [n for n in adj[order[-1]] if n != prev]
        if nxt[0] == start:
            return order
        prev = order[-1]
        order.append(nxt[0])


def main():
    head_path, ref_path, out = sys.argv[1:4]
    header, hnodes, htris = parse(head_path)
    _, rnodes, rtris = parse(ref_path)
    hindex = {n.lower(): i for i, n in hnodes.items()}

    hvert, htex = {}, {}
    for tex, v in htris:
        for x in v:
            hvert.setdefault(key(x[1]), x)
            htex.setdefault(key(x[1]), tex)
    rbone = {}
    for _, v in rtris:
        for x in v:
            rbone.setdefault(key(x[1]), hindex[rnodes[x[0]].lower()])

    hloop = bottom_loop(htris)
    rloop = bottom_loop(rtris)
    cx = sum(p[0] for p in rloop) / len(rloop)
    cy = sum(p[1] for p in rloop) / len(rloop)

    def angle(p):
        return math.atan2(p[1] - cy, p[0] - cx) % (2 * math.pi)

    # Both rings in their own order, the same way round, from their point at the smallest angle; each must go
    # round the neck steadily for the zip to follow it.
    def arrange(ring):
        a = [angle(p) for p in ring]
        turn = sum(((a[(i + 1) % len(a)] - a[i] + math.pi) % (2 * math.pi)) - math.pi for i in range(len(a)))
        if turn < 0:
            ring = ring[::-1]
        k = min(range(len(ring)), key=lambda i: angle(ring[i]))
        ring = ring[k:] + ring[:k]
        steps = [(angle(ring[(i + 1) % len(ring)]) - angle(ring[i])) % (2 * math.pi) for i in range(len(ring))]
        if max(steps) > math.pi:
            raise SystemExit('a neck edge doubles back round the neck')
        return ring

    H, R = arrange(hloop), arrange(rloop)

    def unwrapped(ring):
        out, base, last = [], 0.0, None
        for p in ring:
            a = angle(p) + base
            if last is not None and a < last:
                base += 2 * math.pi
                a += 2 * math.pi
            out.append(a)
            last = a
        return out

    ah, ar = unwrapped(H), unwrapped(R)
    nh, nr = len(H), len(R)
    tris = []
    i = j = 0
    while i < nh or j < nr:
        nexth = ah[i + 1] if i + 1 < nh else ah[0] + 2 * math.pi
        nextr = ar[j + 1] if j + 1 < nr else ar[0] + 2 * math.pi
        if j >= nr or (i < nh and nexth <= nextr):
            tris.append((H[i % nh], H[(i + 1) % nh], R[j % nr]))
            i += 1
        else:
            tris.append((H[i % nh], R[(j + 1) % nr], R[j % nr]))
            j += 1

    def outward(p):
        d = (p[0] - cx, p[1] - cy)
        n = math.hypot(*d) or 1
        return (d[0] / n, d[1] / n, 0.0)

    def facing(a, b, c):
        u = [b[k] - a[k] for k in range(3)]
        w = [c[k] - a[k] for k in range(3)]
        n = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
        o = outward([(a[k] + b[k] + c[k]) / 3 for k in range(3)])
        return n[0] * o[0] + n[1] * o[1]

    # The winding the head's own neck triangles have, facing out.
    ring = set(H)
    sign = 1 if sum(facing(*(x[1] for x in v)) for _, v in htris if sum(key(x[1]) in ring for x in v) >= 2) > 0 else -1

    def line(bone, p, n, uv):
        return f"{bone} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {n[0]:.6f} {n[1]:.6f} {n[2]:.6f} {uv[0]:.6f} {uv[1]:.6f}"

    lines = list(header) + ['triangles']
    for tex, v in htris:
        lines.append(tex)
        lines += [line(*x) for x in v]
    for pts in tris:
        heads = [p for p in pts if p in hvert]
        tex = htex[heads[0]]
        ref_uv = hvert[heads[0]][3]
        verts = []
        for p in pts:
            if p in hvert:
                bone, pos, nrm, uv = hvert[p]
                if htex[p] != tex:
                    uv = ref_uv
            else:
                near = min(heads, key=lambda q: math.dist(q, p))
                bone, pos, nrm, uv = rbone[p], p, outward(p), hvert[near][3] if htex[near] == tex else ref_uv
            verts.append((bone, pos, nrm, uv))
        if facing(*(x[1] for x in verts)) * sign < 0:
            verts = [verts[0], verts[2], verts[1]]
        lines.append(tex)
        lines += [line(*x) for x in verts]
    lines.append('end')
    open(out, 'w', encoding='latin-1', newline='\n').write('\n'.join(lines) + '\n')
    print(f'smd_neck_bridge: head edge {nh} vertices, reference edge {nr}, {len(tris)} triangles added')


main()
