#!/usr/bin/env python3
"""Offline version of sat_water_prepare_mesh (include/saturn/water_prepare.h).

Classifies the faces of a static mesh against water regions, cuts RGB faces
that cross a surface, and tints submerged vertices by depth, with the same
fixed-point rules as the C implementation, so a converter can ship the result.
tests/tools/test_water_prepare_parity.py compares the two bit for bit.

Vertices are (x, y, z) 16.16 integers, indices are four per face, regions are
dicts with min_x, max_x, min_z, max_z, bottom and surface, and a material is
only described by whether it is a plain RGB one (cuttable).
"""
ONE = 1 << 16
NEUTRAL = 0x4210


def _trunc_div(a, b):
    """C integer division: truncates toward zero."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def _interpolate(low, high, numerator, denominator):
    delta = high - low
    offset = abs(delta) * numerator // denominator
    return low + (-offset if delta < 0 else offset)


def _intersection(a, b, y):
    low, high = (a, b) if a[1] < b[1] else (b, a)
    n, d = y - low[1], high[1] - low[1]
    return (_interpolate(low[0], high[0], n, d), y, _interpolate(low[2], high[2], n, d))


def _blend_color(low, high, numerator, denominator):
    result = low & 0x8000
    for shift in (0, 5, 10):
        a, b = (low >> shift) & 31, (high >> shift) & 31
        delta = b - a
        offset = abs(delta) * numerator // denominator
        result |= (a + (-offset if delta < 0 else offset)) << shift
    return result


def _cut(a, b, y):
    a_low = a[0][1] < b[0][1]
    low, high = (a, b) if a_low else (b, a)
    n, d = y - low[0][1], high[0][1] - low[0][1]
    return (_intersection(a[0], b[0], y), _blend_color(low[1], high[1], n, d))


def _emit(points, submerged, out):
    points = list(points)
    if len(points) > 1 and points[0][0] == points[-1][0]:
        points.pop()
    for i in range(1, len(points) - 1, 2):
        last = i + 2 if i + 2 < len(points) else i + 1
        out.append((submerged, [points[0], points[i], points[i + 1], points[last]]))


def split_quad_shaded(quad, colors, surface):
    """sat_water_split_quad_shaded: a list of (submerged, [(point, color) x 4])."""
    corners = [(quad[i], colors[i]) for i in range(4)]
    above = any(c[0][1] > surface for c in corners)
    below = any(c[0][1] < surface for c in corners)
    if not (above and below):
        return [(1 if below else 0, corners)]
    dry, wet = [], []

    def push(poly, corner):
        if not poly or poly[-1][0] != corner[0]:
            poly.append(corner)

    for i in range(4):
        a, b = corners[(i + 3) % 4], corners[i]
        if (a[0][1] < surface < b[0][1]) or (a[0][1] > surface > b[0][1]):
            cut = _cut(a, b, surface)
            push(dry, cut)
            push(wet, cut)
        if b[0][1] >= surface:
            push(dry, b)
        if b[0][1] <= surface:
            push(wet, b)
    out = []
    _emit(dry, 0, out)
    _emit(wet, 1, out)
    return out


def _tinted(tint, full_depth, base, depth):
    if depth <= 0:
        return base
    t = ONE if depth >= full_depth else depth * ONE // full_depth
    channels = []
    for k in range(3):
        value = ((base >> (5 * k)) & 31) + _trunc_div(tint[k] * t, ONE)
        channels.append(min(31, max(0, value)))
    return (channels[2] << 10) | (channels[1] << 5) | channels[0]


def _surface_over(regions, x, y, z):
    best = None
    for region in regions:
        if (region["min_x"] <= x <= region["max_x"] and region["min_z"] <= z <= region["max_z"]
                and y >= region["bottom"]):
            if best is None or region["surface"] > best["surface"]:
                best = region
    return None if best is None else best["surface"]


def prepare_mesh(vertices, indices, face_materials, rgb_materials, base_gouraud,
                 regions, tint, full_depth):
    """Returns a dict: vertices, gouraud, faces (index lists), face_materials,
    above_faces, below_faces, cut_faces, uncut_faces. The above faces come
    first, then the below ones, as sat_water_prepare_mesh lists them."""
    if full_depth <= 0:
        raise ValueError("full_depth must be positive")
    out_vertices = [tuple(v) for v in vertices]
    gouraud = []
    for v, p in enumerate(vertices):
        base = base_gouraud[v] if base_gouraud is not None else NEUTRAL
        surface = _surface_over(regions, p[0], p[1], p[2])
        gouraud.append(base if surface is None else _tinted(tint, full_depth, base, surface - p[1]))

    above, below = [], []
    cut_faces = uncut_faces = 0
    for f in range(len(indices) // 4):
        idx = indices[4 * f:4 * f + 4]
        material = face_materials[f] if face_materials is not None else 0
        quad = [tuple(vertices[i]) for i in idx]
        low, high = min(p[1] for p in quad), max(p[1] for p in quad)
        cx = _trunc_div(sum(p[0] for p in quad), 4)
        cz = _trunc_div(sum(p[2] for p in quad), 4)
        surface = _surface_over(regions, cx, low, cz)
        if surface is None or low >= surface:
            above.append((idx, material))
        elif high <= surface:
            below.append((idx, material))
        elif rgb_materials[material]:
            colors = [base_gouraud[i] if base_gouraud is not None else NEUTRAL for i in idx]
            for submerged, corners in split_quad_shaded(quad, colors, surface):
                new = []
                for point, color in corners:
                    out_vertices.append(point)
                    gouraud.append(_tinted(tint, full_depth, color, surface - point[1]))
                    new.append(len(out_vertices) - 1)
                (below if submerged else above).append((new, material))
            cut_faces += 1
        else:
            uncut_faces += 1
            (below if _trunc_div(low + high, 2) < surface else above).append((idx, material))
    # The C version writes the below faces from the end of its buffer, so they
    # come out in reverse emission order.
    below.reverse()
    faces = above + below
    return {
        "vertices": out_vertices, "gouraud": gouraud,
        "faces": [list(f[0]) for f in faces], "face_materials": [f[1] for f in faces],
        "above_faces": len(above), "below_faces": len(below),
        "cut_faces": cut_faces, "uncut_faces": uncut_faces,
    }
