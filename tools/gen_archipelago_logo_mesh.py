#!/usr/bin/env python3
"""Builds native/petari/src/Game/Screen/NebulaArchipelagoLogoMesh.inc: the Archipelago logo (tools/archipelago_logo.png) as a
3D mesh for the file select, six rounded discs, each a circle with a bite out of it where its two neighbours come (the
white gaps of the logo). Positions and normals are in units where the logo's outer radius is 1; the logo faces +Z.
Run it again to change the shape (the constants below). `--preview out.png` draws a picture of the mesh."""
import math
import sys
from pathlib import Path

N = 120                     # samples around each petal's outline
BEVEL_STEPS = (0, 30, 60, 90)  # degrees of the rounded edge
R = 56.5                    # a disc's radius in the picture's pixels
GAP = 8.0                   # the white outline of the disc above, in pixels
HALF = 16.0                 # half the thickness
BEVEL = 9.0                 # the edge's rounding
# The logo is a pinwheel of six discs, each with a white outline that cuts the discs under it. Where the discs are (the
# picture's pixels, y up, the ring's centre at the origin), their colours (r, g, b) and the discs lying over them.
PETALS = [
    ((0.0, 87.5), (201, 118, 130), (5, 1)),     # 0 top: under the top left and the top right
    ((82.0, 41.0), (117, 194, 117), (2,)),      # 1 top right: under the bottom right
    ((82.0, -41.0), (202, 148, 194), (3,)),     # 2 bottom right: under the bottom
    ((0.0, -87.5), (217, 160, 125), ()),        # 3 bottom: over all
    ((-82.0, -41.0), (118, 126, 189), (3,)),    # 4 bottom left: under the bottom
    ((-82.0, 41.0), (238, 227, 145), (4,)),     # 5 top left: under the bottom left
]
OUTER = max(math.hypot(*c) for c, _, _ in PETALS) + R


def circle_hits(c0, r0, c1, r1):
    dx, dy = c1[0] - c0[0], c1[1] - c0[1]
    d = math.hypot(dx, dy)
    a = (r0 * r0 - r1 * r1 + d * d) / (2 * d)
    h = math.sqrt(max(0.0, r0 * r0 - a * a))
    mx, my = c0[0] + a * dx / d, c0[1] + a * dy / d
    return [(mx + h * dy / d, my - h * dx / d), (mx - h * dy / d, my + h * dx / d)]


def angle_of(c, p):
    return math.atan2(p[1] - c[1], p[0] - c[0])


def ccw_span(a, b):
    return (b - a) % (2 * math.pi)


def petal_outline(index):
    """The petal's boundary, counter-clockwise: the circle, and the arcs of the overlying discs' outlines that bite into it."""
    c, _, over = PETALS[index]
    bites = []
    for o in over:
        oc = PETALS[o][0]
        p, q = circle_hits(c, R, oc, R + GAP)
        ta, tb = angle_of(c, p), angle_of(c, q)
        # the arc of this circle inside the bite: its middle is within the bite
        mid = ta + ccw_span(ta, tb) / 2
        inside = math.hypot(c[0] + R * math.cos(mid) - oc[0], c[1] + R * math.sin(mid) - oc[1]) < R + GAP
        if not inside:
            ta, tb, p, q = tb, ta, q, p        # now the bite lies ccw from ta to tb
        bites.append((ta, tb, oc, p, q))
    bites.sort(key=lambda b: b[0])
    pieces = []                                  # (list of points) sampled later by length
    if not bites:
        pieces.append(("circle", c, R, 0.0, 2 * math.pi, 1))
    else:
        for j, (ta, tb, oc, p, q) in enumerate(bites):
            prev_exit = bites[j - 1][1]
            pieces.append(("circle", c, R, prev_exit, ta, 1))
            # the bite's arc from the entry p to the exit q, through the inside of the circle
            a0, a1 = angle_of(oc, p), angle_of(oc, q)
            span = ccw_span(a0, a1)
            mid = a0 + span / 2
            if math.hypot(oc[0] + (R + GAP) * math.cos(mid) - c[0], oc[1] + (R + GAP) * math.sin(mid) - c[1]) < R:
                pieces.append(("bite", oc, R + GAP, a0, a0 + span, 1))
            else:
                pieces.append(("bite", oc, R + GAP, a0, a0 - (2 * math.pi - span), -1))
    lengths = []
    for kind, cc, rr, a0, a1, d in pieces:
        span = ccw_span(a0, a1) if kind == "circle" else abs(a1 - a0)
        if kind == "circle" and not bites:
            span = 2 * math.pi
        lengths.append(span * rr)
    total = sum(lengths)
    counts = [max(2, round(N * l / total)) for l in lengths]
    while sum(counts) > N:
        counts[counts.index(max(counts))] -= 1
    while sum(counts) < N:
        counts[counts.index(max(counts))] += 1
    points = []
    for (kind, cc, rr, a0, a1, d), l, n in zip(pieces, lengths, counts):
        if kind == "circle":
            span = ccw_span(a0, a1) if bites else 2 * math.pi
            start, signed = a0, span
        else:
            start, signed = a0, a1 - a0
        for k in range(n):
            t = start + signed * k / n
            points.append((cc[0] + rr * math.cos(t), cc[1] + rr * math.sin(t)))
    return points


def triangulate(poly):
    """Ear clipping of a simple counter-clockwise polygon: a list of index triples."""
    idx = list(range(len(poly)))
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    def inside(p, a, b, c):
        return cross(a, b, p) >= 0 and cross(b, c, p) >= 0 and cross(c, a, p) >= 0
    out = []
    guard = 0
    while len(idx) > 3 and guard < 10000:
        guard += 1
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = poly[i0], poly[i1], poly[i2]
            if cross(a, b, c) <= 1e-9:
                continue
            if any(inside(poly[j], a, b, c) for j in idx if j not in (i0, i1, i2)):
                continue
            out.append((i0, i1, i2))
            idx.pop(k)
            break
        else:
            break
    out.append(tuple(idx))
    return out


def build_petal(index):
    outline = petal_outline(index)
    normals = []
    for k in range(N):
        px, py = outline[(k - 1) % N]
        qx, qy = outline[(k + 1) % N]
        tx, ty = qx - px, qy - py
        length = math.hypot(tx, ty) or 1.0
        normals.append((ty / length, -tx / length))
    rings = []
    for side in (1.0, -1.0):
        steps = BEVEL_STEPS if side > 0 else tuple(reversed(BEVEL_STEPS))
        for deg in steps:
            phi = math.radians(deg)
            inset = BEVEL * (1.0 - math.sin(phi))
            z = side * ((HALF - BEVEL) + BEVEL * math.cos(phi))
            ring = []
            for k in range(N):
                nx, ny = normals[k]
                ring.append(((outline[k][0] - nx * inset, outline[k][1] - ny * inset, z),
                             (nx * math.sin(phi), ny * math.sin(phi), side * math.cos(phi))))
            ring.append(ring[0])
            rings.append(ring)
    # The cap's triangles are those of the outline itself (the rounded rim's ring can pinch at the thin horns).
    cap = triangulate(outline)
    return rings, cap


def main():
    out = Path(__file__).resolve().parent.parent / "native/petari/src/Game/Screen/NebulaArchipelagoLogoMesh.inc"
    petals = [build_petal(i) for i in range(6)]
    if "--preview" in sys.argv:
        preview(petals, Path(sys.argv[sys.argv.index("--preview") + 1]))
        return
    ring_count = len(BEVEL_STEPS) * 2
    lines = [
        "// Generated by tools/gen_archipelago_logo_mesh.py: do not edit.",
        "// The Archipelago logo as six discs. Per disc: %d rings of %d vertices (the front's rounding, then the back's), each ring" % (ring_count, N + 1),
        "// closed on its first vertex: x y z nx ny nz, the logo's outer radius being 1 and the front facing +Z. The caps are",
        "// triangles over the first (front) and last (back) ring's vertices.",
        "constexpr int cLogoSamples = %d;" % N,
        "constexpr int cLogoRings = %d;" % ring_count,
        "constexpr int cLogoCapTriangles = %d;" % (N - 2),
        "constexpr unsigned char cLogoColors[6][3] = {" + ", ".join("{%d, %d, %d}" % c for _, c, _ in PETALS) + "};",
        "static const float cLogoMesh[6][cLogoRings * (cLogoSamples + 1)][6] = {",
    ]
    for rings, cap in petals:
        lines.append("    {")
        for ring in rings:
            for (x, y, z), (nx, ny, nz) in ring:
                lines.append("        {%.4ff, %.4ff, %.4ff, %.3ff, %.3ff, %.3ff}," % (x / OUTER, y / OUTER, z / OUTER, nx, ny, nz))
        lines.append("    },")
    lines.append("};")
    lines.append("static const unsigned short cLogoCaps[6][cLogoCapTriangles][3] = {")
    for rings, cap in petals:
        assert len(cap) == N - 2, len(cap)
        lines.append("    {" + ", ".join("{%d, %d, %d}" % t for t in cap) + "},")
    lines.append("};")
    out.write_text("\n".join(lines) + "\n")
    print("wrote", out, out.stat().st_size, "bytes")


def preview(petals, path):
    from PIL import Image, ImageDraw
    size = 700
    picture = Image.new("RGB", (size, size), (20, 60, 110))
    draw = ImageDraw.Draw(picture)
    light = (-0.4, 0.55, 0.75)
    ll = math.sqrt(sum(v * v for v in light))
    light = tuple(v / ll for v in light)
    angle = math.radians(float(sys.argv[sys.argv.index("--angle") + 1])) if "--angle" in sys.argv else 0.0
    ca, sa = math.cos(angle), math.sin(angle)
    tris = []
    for index, (rings, cap) in enumerate(petals):
        color = PETALS[index][1]
        def shade(n):
            nx, nz = n[0] * ca + n[2] * sa, -n[0] * sa + n[2] * ca
            ny = n[1]
            d = max(0.0, nx * light[0] + ny * light[1] + nz * light[2])
            h = (light[0], light[1], light[2] + 1.0)
            hl = math.sqrt(sum(v * v for v in h))
            spec = max(0.0, (nx * h[0] + ny * h[1] + nz * h[2]) / hl) ** 24 * 0.35
            k = 0.5 + 0.6 * d
            return tuple(min(255, int(c * k + 255 * spec)) for c in color)
        def project(v):
            x, y, z = v[0][0] / OUTER, v[0][1] / OUTER, v[0][2] / OUTER
            xr, zr = x * ca + z * sa, -x * sa + z * ca
            return (size / 2 + xr * size * 0.46, size / 2 - y * size * 0.46, zr)
        def add(a, b, c):
            pa, pb, pc = project(a), project(b), project(c)
            z = (pa[2] + pb[2] + pc[2]) / 3
            n = tuple((a[1][i] + b[1][i] + c[1][i]) / 3 for i in range(3))
            tris.append((z, [pa[:2], pb[:2], pc[:2]], shade(n)))
        for ia, ib, ic in cap:
            add(rings[0][ia], rings[0][ib], rings[0][ic])
            add(rings[-1][ia], rings[-1][ib], rings[-1][ic])
        for r in range(len(rings) - 1):
            for k in range(N):
                add(rings[r][k], rings[r + 1][k], rings[r][k + 1])
                add(rings[r][k + 1], rings[r + 1][k], rings[r + 1][k + 1])
    tris.sort(key=lambda t: t[0])
    for _, poly, colour in tris:
        draw.polygon(poly, fill=colour)
    picture.save(path)
    print("wrote", path)


main()
