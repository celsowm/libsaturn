#!/usr/bin/env python3
"""Baked-pose quantization for the Saturn vertex-animation runtime.

The host skinning evaluator produces float scene-space poses; this module
packs them into the compact signed-16-bit stream the runtime decodes with
``pos_fx16 = bias + (scale * q) / 32767``. Quantization error is measured
and reported so the importer can refuse encodings that damage the model.
"""

from __future__ import annotations

import math

FX16_ONE = 65536
Q_MAX = 32767


def float_to_fx16(value: float) -> int:
    return int(math.floor(value * FX16_ONE + 0.5))


def decode_axis_q(bias_fx: int, scale_fx: int, q: int) -> int:
    """Reference decoder (mirrors sat_anim_decode integer math)."""
    return bias_fx + (scale_fx * q) // Q_MAX


class PoseBakeError(Exception):
    pass


def quantize_frames(
    frames: list[list[tuple[float, float, float]]],
    scale: float = 1.0,
    max_error_diag: float = 5e-4,
    bbox_diagonal: float = 1.0,
) -> dict:
    """Pack baked float poses into one int16 stream (frame-major).

    Returns ``{stream, encoding, frame_count, vertex_count, max_error,
    mean_error, pose_bytes}`` where ``encoding`` holds fx16 bias/scale per
    axis and ``max_error`` is in source units. Raises PoseBakeError when
    the encoding itself (not the simplification) exceeds
    ``max_error_diag`` of the bbox diagonal, or when fx16 range overflows.
    """
    if not frames:
        raise PoseBakeError("no animation frames to quantize")
    vertex_count = len(frames[0])
    if vertex_count == 0:
        raise PoseBakeError("animation frames have no vertices")
    for f in frames:
        if len(f) != vertex_count:
            raise PoseBakeError("animation frames disagree in vertex count")
    if scale <= 0.0:
        raise PoseBakeError(f"scale must be positive (got {scale})")

    scaled = [
        [(x * scale, y * scale, z * scale) for (x, y, z) in frame]
        for frame in frames
    ]
    mins = [min(p[a] for frame in scaled for p in frame) for a in range(3)]
    maxs = [max(p[a] for frame in scaled for p in frame) for a in range(3)]
    bias = [(lo + hi) / 2.0 for lo, hi in zip(mins, maxs)]
    half = [(hi - lo) / 2.0 for lo, hi in zip(mins, maxs)]
    # A flat axis carries no information; q=0 must decode exactly bias.
    half = [h if h > 0.0 else 1.0 for h in half]

    for axis, (b, h) in enumerate(zip(bias, half)):
        for value, name in ((b, "bias"), (h, "scale")):
            fx = float_to_fx16(value)
            if not -(2**31) <= fx < 2**31:
                raise PoseBakeError(
                    f"axis {axis} {name} {value} overflows 16.16 fixed point "
                    "(reduce --scale)"
                )
    bias_fx = [float_to_fx16(b) for b in bias]
    scale_fx = [float_to_fx16(h) for h in half]

    stream: list[int] = []
    max_err = 0.0
    sum_err = 0.0
    count = 0
    for frame in scaled:
        for (x, y, z) in frame:
            for a, v in enumerate((x, y, z)):
                q = int(round((v - bias[a]) / half[a] * Q_MAX))
                q = max(-Q_MAX, min(Q_MAX, q))
                stream.append(q)
                # Reference decode in float space for the error bound.
                back = bias[a] + half[a] * q / Q_MAX
                err = abs(back - v)
                max_err = max(max_err, err)
                sum_err += err
                count += 1
    mean_err = sum_err / count if count else 0.0
    if max_err > max_error_diag * bbox_diagonal:
        raise PoseBakeError(
            f"pose quantization error {max_err:.6f} exceeds "
            f"{max_error_diag} of bbox diagonal {bbox_diagonal:.3f}"
        )
    return {
        "stream": stream,
        "encoding": {"bias": bias_fx, "scale": scale_fx},
        "frame_count": len(frames),
        "vertex_count": vertex_count,
        "max_error": max_err,
        "mean_error": mean_err,
        "pose_bytes": len(stream) * 2,
    }
