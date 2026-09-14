#!/usr/bin/env python3
"""Validated multi-LOD derivation from the source model (host-only).

Each LOD derives directly from the original source under its own
target/quality gate: recursive LOD1-into-LOD2 simplification would compound
errors unnecessarily. The report states which LODs pass which gates; LODs
that fail are reported, not silently shipped.

Runtime LOD switching is optional for the first ``basic_3d_animation`` ISO:
generating and validating LODs on ``--generate-lods`` is the requirement
this module covers.
"""

from __future__ import annotations

from dataclasses import dataclass

from .gltf import GltfError


@dataclass
class LodSpec:
    name: str
    target_triangles: int
    quality: str = "balanced"


def default_lod_specs(face_budget: int, source_triangles: int) -> list[LodSpec]:
    """LOD targets as fractions of the measured face budget.

    Starting points, not universal counts: LOD0 near, LOD1 normal, LOD2 far,
    LOD3 very far. Every entry is clamped to the source count and validated
    by its own quality gate in :func:`generate_lods`.
    """
    if face_budget < 1:
        raise GltfError("LOD specs need a positive face budget")
    fractions = (("lod0_near", 1.00), ("lod1_normal", 0.70), ("lod2_far", 0.45), ("lod3_veryfar", 0.25))
    specs = []
    for name, frac in fractions:
        target = max(8, min(source_triangles, int(face_budget * frac)))
        specs.append(LodSpec(name=name, target_triangles=target))
    # Deterministic de-duplication: equal targets collapse to one LOD.
    seen: dict[int, LodSpec] = {}
    for spec in specs:
        seen.setdefault(spec.target_triangles, spec)
    return [seen[k] for k in sorted(seen, reverse=True)]


def generate_lods(
    source,
    clip,
    specs: list[LodSpec],
    preset_name: str = "balanced",
    anim_importance=None,
    sil_importance=None,
    times=None,
    pose_positions=None,
    silhouette_views: int = 16,
) -> list[dict]:
    """Simplify + validate one entry per spec; each from the source model."""
    from . import metrics as metrics_mod
    from . import simplification as simp_mod

    if times is None and clip is not None:
        times = metrics_mod.sample_times_for_importance(clip)
    out = []
    for spec in specs:
        options = simp_mod.SimplificationOptions(
            target_triangles=spec.target_triangles, quality=spec.quality
        )
        simp = simp_mod.simplify(
            source,
            anim_importance=anim_importance,
            sil_importance=sil_importance,
            options=options,
            pose_positions=pose_positions,
        )
        quality = metrics_mod.evaluate_candidate(
            source, simp, clip, spec.quality if spec.quality else preset_name,
            times, silhouette_views,
        )
        out.append(
            {
                "name": spec.name,
                "requested_target": spec.target_triangles,
                "delivered_triangles": len(simp.triangles),
                "delivered_vertices": len(simp.positions),
                "quality_preset": spec.quality,
                "passed": quality["passed"],
                "failing_gates": quality["failing_gates"],
                "surface": quality["surface"],
                "silhouette": quality["silhouette"],
                "simplified": simp,
                "quality": quality,
            }
        )
    return out
