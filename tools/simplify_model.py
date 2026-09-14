#!/usr/bin/env python3
"""Standalone animation-aware GLB simplification tool (host-only).

Simplifies an animated GLB with attribute/UV/material protection,
animation-aware (worst-pose) costs and silhouette preservation, validates
the result across animation poses, and emits a simplified GLB preview plus
a machine-readable JSON report -- without requiring Saturn asset
generation::

    python tools/simplify_model.py \\
      --input model.glb \\
      --output build/model_simplified.glb \\
      --target-triangles 300 \\
      --animation-aware \\
      --preserve-uv \\
      --preserve-silhouette \\
      --report build/model_simplified.report.json

Saturn-oriented usage gates the result against the VDP1 profile::

    python tools/simplify_model.py \\
      --input model.glb \\
      --output build/model_saturn.glb \\
      --profile saturn-vdp1 \\
      --max-triangles 280 \\
      --animation-aware \\
      --generate-lods \\
      --report build/model_saturn.report.json

The tool never blindly forces the requested count: when quality gates
fail it searches upward for the smallest acceptable triangle count and
reports the delivered number. When Saturn caps cannot meet quality, it
fails loudly instead of damaging the model.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from model_pipeline import emit_glb as emit_glb_mod
from model_pipeline import lod as lod_mod
from model_pipeline import metrics as metrics_mod
from model_pipeline import model as model_mod
from model_pipeline import saturn_profile as profile_mod
from model_pipeline import silhouette as sil_mod
from model_pipeline import simplification as simp_mod
from model_pipeline.animation import bake_clip_poses, clip_sample_times
from model_pipeline.gltf import GltfError, parse_model


def select_clip(model: model_mod.SourceModel, selector: str):
    if not model.clips:
        raise GltfError("model has no animation clips")
    try:
        index = int(selector)
        if index < 0 or index >= len(model.clips):
            raise GltfError(
                f"--animation {selector}: only {len(model.clips)} clip(s) available"
            )
        return index, model.clips[index]
    except ValueError:
        for i, clip in enumerate(model.clips):
            if clip.name == selector:
                return i, clip
        raise GltfError(
            f"--animation {selector!r}: no such clip "
            f"(have: {[c.name for c in model.clips]})"
        )


def sample_times(clip, sample_fps: float | None, sample_all: bool):
    if sample_fps is not None:
        if sample_fps <= 0:
            raise GltfError(f"--sample-fps must be positive (got {sample_fps})")
        n = int(clip.duration * sample_fps)
        times = [min(i / sample_fps, clip.duration) for i in range(n + 1)]
        if times[-1] < clip.duration:
            times.append(clip.duration)
        # Deterministic de-duplication preserving order.
        seen: dict[float, None] = {}
        for t in times:
            seen[float(t)] = None
        return sorted(seen)
    authored = clip_sample_times(clip)
    if sample_all or len(authored) <= 64:
        return authored
    return metrics_mod.sample_times_for_importance(clip)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Animation-aware GLB simplifier (LibSaturn host tool)")
    g = p.add_argument_group("General")
    g.add_argument("--input", required=True, help="Input .glb (or .gltf)")
    g.add_argument("--output", required=True, help="Output simplified .glb preview")
    g.add_argument("--report", default=None, help="JSON report path")
    g.add_argument("--target-triangles", type=int, default=None)
    g.add_argument("--target-ratio", type=float, default=None, help="Fraction of source triangles")
    g.add_argument("--target-error", type=float, default=None, help="Custom p95 surface-error cap (fraction of bbox diagonal)")
    g.add_argument("--quality", default="balanced", choices=("conservative", "balanced", "aggressive"))
    pr = p.add_argument_group("Preservation")
    pr.add_argument("--preserve-uv", dest="preserve_uv", action="store_true", default=True)
    pr.add_argument("--no-preserve-uv", dest="preserve_uv", action="store_false")
    pr.add_argument("--preserve-normals", action="store_true", default=True)
    pr.add_argument("--preserve-boundaries", action="store_true", default=True)
    pr.add_argument("--preserve-silhouette", action="store_true", default=True)
    pr.add_argument("--no-preserve-silhouette", dest="preserve_silhouette", action="store_false")
    pr.add_argument("--animation-aware", dest="animation_aware", action="store_true", default=True)
    pr.add_argument("--no-animation-aware", dest="animation_aware", action="store_false")
    an = p.add_argument_group("Animation")
    an.add_argument("--animation", default="0", help="Clip name or index (default 0)")
    an.add_argument("--sample-fps", type=float, default=None)
    an.add_argument("--sample-all-authored-frames", action="store_true", default=False)
    an.add_argument("--animation-weight", type=float, default=1.0)
    si = p.add_argument_group("Silhouette")
    si.add_argument("--silhouette-views", type=int, default=16)
    si.add_argument("--silhouette-weight", type=float, default=1.0)
    sa = p.add_argument_group("Saturn")
    sa.add_argument("--profile", default=None, help="'saturn-vdp1' to gate against VDP1 budgets")
    sa.add_argument("--max-triangles", type=int, default=None)
    sa.add_argument("--max-vdp1-commands", type=int, default=None)
    sa.add_argument("--generate-lods", action="store_true", default=False)
    dg = p.add_argument_group("Diagnostics")
    dg.add_argument("--verbose", action="store_true", default=False)
    dg.add_argument("--dump-metrics", action="store_true", default=False)
    return p


def run(args) -> int:
    try:
        return _run(args)
    except GltfError as exc:
        print(f"simplify_model: error: {exc}", file=sys.stderr)
        return 1


def _run(args) -> int:
    in_path = Path(args.input)
    out_path = Path(args.output)
    glb = parse_model(in_path)
    model = model_mod.from_gltf(glb, in_path.stem)
    stats = model_mod.source_stats(model)
    clip_index, clip = select_clip(model, args.animation)
    times = sample_times(clip, args.sample_fps, args.sample_all_authored_frames)
    if args.verbose:
        print(f"source: {stats['vertices']} verts / {stats['triangles']} tris / "
              f"{stats['joints']} joints / {len(model.clips)} clip(s)")
        print(f"clip '{clip.name}': duration {clip.duration:.3f}s, {len(times)} sample times")

    profile = None
    face_cap: int | None = None
    if args.profile is not None:
        if args.profile != "saturn-vdp1":
            raise GltfError(f"unknown --profile {args.profile!r} (have: saturn-vdp1)")
        profile = profile_mod.SaturnProfile()
        face_cap = profile_mod.face_command_budget(profile)
    if args.max_vdp1_commands is not None:
        base = profile or profile_mod.SaturnProfile()
        face_cap = args.max_vdp1_commands - base.setup_commands - base.end_commands - base.hud_reserve - base.min_command_headroom
        if face_cap < 1:
            raise GltfError("--max-vdp1-commands leaves no room for model faces")
    if args.max_triangles is not None:
        face_cap = args.max_triangles if face_cap is None else min(face_cap, args.max_triangles)

    if args.target_triangles is not None:
        requested = args.target_triangles
    elif args.target_ratio is not None:
        if not 0.0 < args.target_ratio <= 1.0:
            raise GltfError("--target-ratio must be in (0, 1]")
        requested = max(1, int(round(stats["triangles"] * args.target_ratio)))
    elif face_cap is not None:
        requested = min(face_cap, stats["triangles"])
    else:
        requested = stats["triangles"]
    if requested < 1:
        raise GltfError("requested triangle target must be >= 1")

    preset = args.quality
    if args.target_error is not None:
        if args.target_error <= 0:
            raise GltfError("--target-error must be positive")
        custom = dict(metrics_mod.QUALITY_PRESETS[args.quality])
        custom["__name__"] = f"{args.quality}+err{args.target_error}"
        custom["p95_surface_error"] = args.target_error
        custom["max_surface_error"] = max(
            custom["max_surface_error"], args.target_error * 2.0
        )
        preset = custom

    anim_imp = sil_imp = None
    if args.animation_aware:
        anim_imp = metrics_mod.compute_animation_importance(model, clip, times)
    if args.preserve_silhouette:
        sil_imp = sil_mod.compute_silhouette_importance(
            model, clip=clip, times=times[:: max(1, len(times) // 8)][:8],
            n_views=args.silhouette_views,
        )
    poses = bake_clip_poses(model, clip, times)

    options = simp_mod.SimplificationOptions(
        target_triangles=requested,
        quality=args.quality,
        preserve_uv=args.preserve_uv,
        preserve_normals=args.preserve_normals,
        preserve_boundaries=args.preserve_boundaries,
        animation_weight=args.animation_weight,
        silhouette_weight=args.silhouette_weight,
    )
    simp, quality = metrics_mod.search_upward(
        model, clip, requested, preset, options,
        anim_importance=anim_imp, sil_importance=sil_imp,
        times=times, pose_positions=poses,
    )
    delivered = len(simp.triangles)

    saturn_report = None
    if profile is not None or face_cap is not None:
        prof = profile or profile_mod.SaturnProfile()
        # Pose-stream estimate for the baked-vertex runtime (int16 xyz).
        pose_bytes = len(simp.positions) * len(times) * 3 * 2
        src_tex_bytes = sum(t.width * t.height * 4 for t in model.textures)
        saturn_report = profile_mod.check_resources(
            prof, faces=delivered, texture_payload_bytes=src_tex_bytes,
            pose_stream_bytes=pose_bytes,
        )
        # Enforce the cap against the QUALITY-VALIDATED count: never force
        # a damaging count to fit.
        if face_cap is not None and delivered > face_cap:
            print(profile_mod.format_hard_failure(
                delivered, face_cap,
                f"smallest quality-valid ({args.quality}) mesh has {delivered} triangles"), file=sys.stderr)
            ok, fail = False, ["saturn face cap"]
            return _finish(out_path, args, model, clip, clip_index, times, requested,
                           simp, quality, saturn_report, None, ok, fail, args.quality)
        if not saturn_report["passed"]:
            print(profile_mod.format_hard_failure(
                delivered, face_cap or profile_mod.face_command_budget(prof),
                "; ".join(saturn_report["failing_gates"])), file=sys.stderr)
            return _finish(out_path, args, model, clip, clip_index, times, requested,
                           simp, quality, saturn_report, None, False,
                           saturn_report["failing_gates"], args.quality)

    lod_reports = None
    if args.generate_lods:
        budget = face_cap or profile_mod.face_command_budget(profile or profile_mod.SaturnProfile())
        specs = lod_mod.default_lod_specs(budget, stats["triangles"])
        lod_reports = []
        for entry in lod_mod.generate_lods(
            model, clip, specs, args.quality, anim_imp, sil_imp, times, poses,
            args.silhouette_views,
        ):
            lod_reports.append({k: v for k, v in entry.items() if k not in ("simplified", "quality")})
            lod_reports[-1]["passed"] = entry["passed"]
            lod_reports[-1]["failing_gates"] = entry["failing_gates"]
            stem = out_path.with_suffix("")
            lod_path = stem.parent / f"{stem.name}_{entry['name']}.glb"
            lod_path.parent.mkdir(parents=True, exist_ok=True)
            lod_path.write_bytes(emit_glb_mod.emit_glb(model, entry["simplified"]))
            lod_reports[-1]["output"] = str(lod_path)

    out_bytes = emit_glb_mod.emit_glb(model, simp)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(out_bytes)

    ok = quality["passed"] and (saturn_report is None or saturn_report["passed"])
    preset_label = preset if isinstance(preset, str) else preset.get("__name__", "custom")
    return _finish(out_path, args, model, clip, clip_index, times, requested,
                   simp, quality, saturn_report, lod_reports, ok,
                   quality["failing_gates"], preset_label, out_bytes)


def _finish(out_path, args, model, clip, clip_index, times, requested,
            simp, quality, saturn_report, lod_reports, ok, failing, preset_name,
            out_bytes=None) -> int:
    stats = model_mod.source_stats(model)
    if out_bytes is None and out_path.exists():
        out_bytes = out_path.read_bytes()
    sha = hashlib.sha256(out_bytes).hexdigest() if out_bytes else None
    report = {
        "source": {
            **stats,
            "clip": clip.name,
            "clip_index": clip_index,
            "clip_duration": clip.duration,
            "sample_times": len(times),
        },
        "simplification": {
            **simp.report,
            "quality_preset": preset_name,
            "animation_aware": args.animation_aware,
            "preserve_silhouette": args.preserve_silhouette,
            "silhouette_views": args.silhouette_views,
        },
        "animation_quality": quality,
        "saturn": saturn_report,
        "lods": lod_reports,
        "result": {
            "pass": bool(ok),
            "failing_gates": failing,
            "output": str(out_path),
            "glb_sha256": sha,
        },
    }
    if args.report:
        report_path = Path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
    delivered = len(simp.triangles)
    if delivered != requested:
        print(f"note: requested {requested} triangles, delivered {delivered} "
              f"(quality gates under '{preset_name}')")
    surf = quality["surface"]
    print(f"source {stats['triangles']} tris -> {delivered} tris "
          f"({simp.report.get('reduction_percent', 0.0)}% reduction)")
    print(f"surface error: max {surf['max']:.4f} p95 {surf['p95']:.4f} mean {surf['mean']:.4f} "
          f"(fraction of bbox diagonal {surf['bbox_diagonal']:.3f})")
    sil = quality["silhouette"]
    print(f"silhouette: IoU min {sil['iou_min']:.3f} chamfer max {sil['chamfer_px_max']:.2f}px")
    if args.dump_metrics:
        print(f"worst surface sample: t={surf['worst']['time']:.3f}s "
              f"vertex={surf['worst']['vertex']} err={surf['worst']['error']:.4f}")
        print(f"worst outline: view={sil['worst_chamfer']['view']} "
              f"t={sil['worst_chamfer']['time']:.3f}s chamfer={sil['worst_chamfer']['chamfer_px']:.2f}px")
    print(f"OK: {out_path}" + (f" + {args.report}" if args.report else ""))
    if not ok:
        print("FAIL: quality or Saturn gates did not pass", file=sys.stderr)
        return 2
    return 0


def main() -> int:
    return run(build_parser().parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
