"""Host-baked smooth directional light for textured indexed VDP1 assets.

Light is fixed in asset space. Gradients are ordinary texture pixels, so the
runtime needs neither Gouraud tables nor extra per-frame shading work.
"""
import math


def smooth_normals(positions, triangles, moving_point):
    accumulated = {}
    for a, b, c in triangles:
        ab = [positions[b][i] - positions[a][i] for i in range(3)]
        ac = [positions[c][i] - positions[a][i] for i in range(3)]
        normal = (ab[1]*ac[2] - ab[2]*ac[1],
                  ab[2]*ac[0] - ab[0]*ac[2],
                  ab[0]*ac[1] - ab[1]*ac[0])
        for vertex in (a, b, c):
            total = accumulated.setdefault(moving_point[vertex], [0., 0., 0.])
            for axis in range(3):
                total[axis] += normal[axis]
    result = []
    for point in moving_point:
        normal = accumulated.get(point, (0., 1., 0.))
        length = math.sqrt(sum(c*c for c in normal))
        result.append(tuple(c / length for c in normal) if length > 1e-20 else (0., 1., 0.))
    return result


def shade_texture(rgba, width, height, normals, light, ambient, diffuse):
    """Modulate encoded RGB, as the original console's color multiplier does.

    Corner order is the same A/B/C/D canonical rectangle as the texture bake.
    Alpha is preserved. Shared moving points have identical corner lighting.
    """
    import numpy as np
    levels = np.array([ambient + diffuse * max(0., sum(a*b for a, b in zip(n, light)))
                       for n in normals])
    s = (np.arange(width) + .5) / width
    t = ((np.arange(height) + .5) / height)[:, None]
    top = levels[0] + (levels[1] - levels[0]) * s
    bottom = levels[3] + (levels[2] - levels[3]) * s
    gain = top + (bottom - top) * t
    image = np.asarray(rgba, dtype=float).reshape(height, width, 4).copy()
    image[:, :, :3] = np.clip(np.rint(image[:, :, :3] * gain[:, :, None]), 0, 255)
    return [tuple(map(int, pixel)) for pixel in image.reshape(-1, 4)]
