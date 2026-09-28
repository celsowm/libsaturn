"""Animated glTF/GLB import, Saturn validation and asset preparation."""
from __future__ import annotations
import argparse
from dataclasses import dataclass
import math
from pathlib import Path
from . import animation as anim_eval
from . import face_colors as face_color_mod
from . import gltf as gltf_mod
from . import lod as lod_mod
from . import metrics as metrics_mod
from . import model as srcmodel
from . import pose_bake
from . import quad_merge as quad_merge_mod
from . import saturn_profile as saturn_profile_mod
from . import silhouette as sil_mod
from . import simplification as simp_mod
from .constants import VDP1_MAX_TEXTURE_HEIGHT, VDP1_MAX_TEXTURE_WIDTH
from .errors import ImportError
from .static_import import ImportResult, float_to_fx16
from .texture_bake import conform_size, estimate_face_size, bake_face_rgba, build_shared_palette, quantize_face_lut4, _lut_code_palette, map_faces_to_indices
from saturn_asset_common import rgb888_to_rgb555

@dataclass
class AnimatedImportResult:
    static: ImportResult
    animations: list[dict]
    report: dict


def select_animation_clips(model, selector: str) -> list[int]:
    if selector == "all":
        if not model.clips:
            raise ImportError("model has no animation clips")
        return list(range(len(model.clips)))
    if "," in selector:
        # A comma list keeps only the clips a program plays, in that order,
        # so a many-clip rig fits the pose-stream budget.
        picked: list[int] = []
        for part in selector.split(","):
            for index in select_animation_clips(model, part.strip()):
                if index not in picked:
                    picked.append(index)
        return picked
    try:
        index = int(selector)
        if index < 0 or index >= len(model.clips):
            raise ImportError(
                f"--animation {selector}: only {len(model.clips)} clip(s) available"
            )
        return [index]
    except ValueError:
        for i, clip in enumerate(model.clips):
            if clip.name == selector:
                return [i]
        raise ImportError(
            f"--animation {selector!r}: no such clip "
            f"(have: {[c.name for c in model.clips]})"
        )


def animated_frame_times(model, clip, fps: str) -> tuple[list[float], bool]:
    """Runtime sample times for one clip plus loop-duplicate flag."""
    if fps == "source":
        return anim_eval.runtime_frame_times(model, clip)
    try:
        rate = float(fps)
    except ValueError:
        raise ImportError(f"--animation-fps must be 'source' or a number (got {fps!r})")
    if rate <= 0.0:
        raise ImportError(f"--animation-fps must be positive (got {fps!r})")
    n = int(clip.duration * rate)
    times = [min(i / rate, clip.duration) for i in range(n + 1)]
    if times[-1] < clip.duration:
        times.append(clip.duration)
    seen: dict[float, None] = {}
    for t in times:
        seen[float(t)] = None
    ordered = sorted(seen)
    # A terminal sample equal to the first pose is a redundant loop frame.
    poses = anim_eval.bake_clip_poses(model, clip, [ordered[0], ordered[-1]])
    removed = False
    if len(ordered) > 1 and anim_eval.loop_pose_distance(poses[0], poses[1]) <= 1e-5:
        ordered = ordered[:-1]
        removed = True
    return ordered, removed


def rate_fraction(frame_count: int, duration: float) -> tuple[int, int]:
    """Reduced (num, den) frames-per-second fraction for the runtime."""
    if duration <= 0.0 or frame_count <= 0:
        raise ImportError("cannot derive a sample rate from an empty clip")
    num = int(round(frame_count / duration * 1000000))
    den = 1000000
    g = math.gcd(num, den)
    return num // g, den // g


def _face_command_cap(args, profile) -> int | None:
    cap: int | None = None
    if args.target == "saturn" or args.profile is not None:
        cap = saturn_profile_mod.face_command_budget(profile)
    if args.max_vdp1_commands is not None:
        room = (args.max_vdp1_commands - profile.setup_commands - profile.end_commands
                - profile.hud_reserve - profile.min_command_headroom)
        if room < 1:
            raise ImportError("--max-vdp1-commands leaves no room for model faces")
        cap = room if cap is None else min(cap, room)
    if args.max_triangles is not None:
        cap = args.max_triangles if cap is None else min(cap, args.max_triangles)
    return cap


def _glb_winding(tris, uvs, reverse_winding):
    """glTF CCW triangles to LibSaturn A/B/C/D degenerate quads.

    Default reverses to clockwise (outward under cross(D-A, B-A)), the same
    convention as the OBJ path; --reverse-winding keeps source order.
    Returns (quads, quad_uvs) with duplicated corners carrying duplicated
    UVs so the canonical rectangle collapses with the geometric quad.
    """
    quads, quad_uvs = [], []
    for (a, b, c) in tris:
        ua, ub, uc = uvs[a], uvs[b], uvs[c]
        if reverse_winding:
            quads.append((a, b, c, c))
            quad_uvs.append((ua, ub, uc, uc))
        else:
            quads.append((a, c, b, b))
            quad_uvs.append((ua, uc, ub, ub))
    return quads, quad_uvs


def _polygon_winding(polys, uvs, reverse_winding):
    """Source CCW triangles and merged quads to LibSaturn A/B/C/D faces.

    Same convention as _glb_winding: the default reverses to clockwise,
    and a triangle repeats its last corner so its baked rectangle collapses
    with the geometric quad.
    """
    quads, quad_uvs = [], []
    for poly in polys:
        order = list(poly) if reverse_winding else [poly[0]] + list(reversed(poly[1:]))
        if len(order) == 3:
            order.append(order[2])
        quads.append(tuple(order))
        quad_uvs.append(tuple(uvs[i] for i in order))
    return quads, quad_uvs


def _cheapest_rotation(quad, quad_uv, img_w, img_h, texture_scale, max_w, max_h):
    """Cyclic corner rotation whose baked rectangle needs the fewest texels.

    Every rotation draws the same outline and keeps the winding; it only
    decides which edge runs along the texture rows. Ties keep the original
    order so output stays deterministic.
    """
    best = None
    for r in range(4):
        q = quad[r:] + quad[:r]
        uv = quad_uv[r:] + quad_uv[:r]
        w, h = estimate_face_size(uv, img_w, img_h, texture_scale)
        w = max(8, ((w + 7) // 8) * 8)
        if w > max_w or h > max_h:
            continue
        if best is None or w * h < best[0]:
            best = (w * h, q, uv)
    if best is None:
        return quad, quad_uv
    return best[1], best[2]


def _resolve_material_weights(model, specs) -> dict[int, float]:
    """``NAME=W`` / ``INDEX=W`` strings to {material index: weight}."""
    weights: dict[int, float] = {}
    names = [m.get("name") for m in model.materials]
    for spec in specs or []:
        key, sep, value = spec.rpartition("=")
        if not sep or not key:
            raise ImportError(f"--material-weight expects NAME=WEIGHT (got {spec!r})")
        try:
            weight = float(value)
        except ValueError:
            raise ImportError(f"--material-weight {spec!r}: weight is not a number")
        if weight <= 0.0:
            raise ImportError(f"--material-weight {spec!r}: weight must be positive")
        if key in names:
            index = names.index(key)
        elif key.isdigit() and int(key) < len(names):
            index = int(key)
        else:
            raise ImportError(f"--material-weight {spec!r}: no material {key!r} "
                              f"(have: {', '.join(str(n) for n in names)})")
        weights[index] = weight
    return weights


def _locality_order(quads, face_texture_indices, vertices_fx, animations, positions):
    """Reorder faces along the model's longest axis and vertices by first use.

    Any contiguous run of faces then touches a mostly contiguous run of
    vertices, so a caller that splits the face list between CPUs can hand
    each one a vertex window instead of the whole pose to project. Returns
    the reordered (quads, face_texture_indices, vertices_fx) and rewrites the
    animation pose streams in place.
    """
    lo = [min(p[a] for p in positions) for a in range(3)]
    hi = [max(p[a] for p in positions) for a in range(3)]
    axis = max(range(3), key=lambda a: hi[a] - lo[a])
    key = [sum(positions[i][axis] for i in q) / 4.0 for q in quads]
    face_order = sorted(range(len(quads)), key=lambda f: (key[f], f))
    new_of: dict[int, int] = {}
    for f in face_order:
        for i in quads[f]:
            if i not in new_of:
                new_of[i] = len(new_of)
    for i in range(len(vertices_fx)):  # unreferenced vertices keep a slot
        if i not in new_of:
            new_of[i] = len(new_of)
    old_at = [0] * len(new_of)
    for old, new in new_of.items():
        old_at[new] = old
    for anim in animations:
        nv = anim["vertex_count"]
        stream = anim["stream"]
        out: list[int] = []
        for frame in range(anim["frame_count"]):
            base = frame * nv * 3
            for old in old_at:
                out.extend(stream[base + old * 3: base + old * 3 + 3])
        anim["stream"] = out
    return ([tuple(new_of[i] for i in quads[f]) for f in face_order],
            [face_texture_indices[f] for f in face_order],
            [vertices_fx[old] for old in old_at])


def import_animated_model(
    glb_path: Path,
    scale: float = 1.0,
    flip_x: bool = False,
    flip_y: bool = False,
    flip_z: bool = False,
    reverse_winding: bool = False,
    palette_index: int = 0,
    max_texture_width: int = VDP1_MAX_TEXTURE_WIDTH,
    max_texture_height: int = VDP1_MAX_TEXTURE_HEIGHT,
    texture_scale: float = 1.0,
    sampling: str = "nearest",
    simplify: str = "auto",
    quality: str = "balanced",
    max_triangles: int | None = None,
    max_vdp1_commands: int | None = None,
    max_pose_stream_bytes: int = 256 * 1024,
    hud_reserve: int = 128,
    animation: str = "all",
    animation_fps: str = "source",
    merge_rigid_meshes: bool = False,
    generate_lods: bool = False,
    silhouette_views: int = 16,
    animation_weight: float = 1.0,
    silhouette_weight: float = 1.0,
    face_colors: str = "off",
    light_dir=(-0.5, 0.6, 0.8),
    ambient: float = 0.35,
    diffuse: float = 0.75,
    merge_quads: bool = False,
    quad_max_texel_error: float = 1.0,
    quad_max_fold_deg: float = 30.0,
    material_weights: list[str] | None = None,
    weld_vertices: bool = False,
    texture_format: str = "indexed8",
    locality_order: bool = False,
    texel_extent: float | None = None,
    lut_codes: tuple[int, int] | None = None,
    pose_workers: int | None = None,
) -> AnimatedImportResult:
    if palette_index < 0 or palette_index > 7:
        raise ImportError(f"--palette-index must be in 0..7 (got {palette_index})")
    if face_colors not in ("off", "auto", "on"):
        raise ImportError(f"--face-colors must be off|auto|on (got {face_colors!r})")
    if ambient < 0.0 or diffuse < 0.0:
        raise ImportError("--ambient and --diffuse must not be negative")
    try:
        light = face_color_mod.parse_light_dir(light_dir)
    except gltf_mod.GltfError as exc:
        raise ImportError(str(exc))
    if texture_scale <= 0.0:
        raise ImportError(f"--texture-scale must be positive (got {texture_scale})")
    if scale <= 0.0:
        raise ImportError(f"--scale must be positive (got {scale})")
    if lut_codes is not None:
        lo, hi = lut_codes
        if texture_format != "lut4":
            raise ImportError("--lut-codes needs --texture-format lut4")
        if not 1 <= lo <= hi <= 255:
            raise ImportError(f"--lut-codes must be LO-HI within 1..255 (got {lo}-{hi})")
    if texel_extent is not None and texel_extent <= 0.0:
        raise ImportError(f"--texel-extent must be positive (got {texel_extent})")
    if texture_format not in ("indexed8", "lut4"):
        raise ImportError(f"--texture-format must be indexed8|lut4 (got {texture_format!r})")
    if locality_order and face_colors != "off":
        raise ImportError("--locality-order is for textured faces; solid-color "
                          "shade streams are baked in source face order")
    if weld_vertices and face_colors != "off":
        raise ImportError("--weld-vertices is for textured faces; --face-colors "
                          "already welds and bakes per-vertex light")
    if merge_quads and face_colors != "off":
        raise ImportError("--merge-quads needs textured faces (--face-colors off): "
                          "solid-color shades are baked per triangle")
    if quality not in metrics_mod.QUALITY_PRESETS:
        raise ImportError(f"unknown --quality {quality!r}")

    try:
        glb = gltf_mod.parse_model(glb_path)
        model = srcmodel.from_gltf(
            glb, glb_path.stem, merge_rigid_meshes=merge_rigid_meshes
        )
    except gltf_mod.GltfError as exc:
        raise ImportError(str(exc))
    stats = srcmodel.source_stats(model)
    material_weight_map = _resolve_material_weights(model, material_weights)
    # Solid-color mode replaces the model BEFORE importance, poses and
    # simplification: every later stage then works on the welded vertices.
    color_report: dict = {"mode": face_colors, "enabled": False}
    face_levels = 0
    base_colors: list = []
    if face_colors != "off":
        analysis = face_color_mod.analyze(model)
        enable, reason = face_color_mod.decide(face_colors, analysis)
        color_report.update({
            "uniform_faces": analysis.uniform,
            "non_uniform_faces": analysis.non_uniform,
            "distinct_colors": analysis.distinct_colors,
            "reason": reason,
        })
        if face_colors == "on" and not enable:
            raise ImportError(f"--face-colors on: {reason}")
        if enable:
            source_vertices = len(model.vertices)
            model, base_colors, welded_count = face_color_mod.flatten(model, analysis)
            face_levels = face_color_mod.levels_for(len(base_colors))
            color_report.update({
                "enabled": True,
                "levels": face_levels,
                "palette_entries": 1 + len(base_colors) * face_levels,
                "light_dir": list(light),
                "ambient": ambient,
                "diffuse": diffuse,
                "welded_vertices": welded_count,
                "vertices_before_weld": source_vertices,
                "vertices_after_weld": len(model.vertices),
            })

    if not model.textures and not color_report["enabled"]:
        raise ImportError(
            f"{glb_path}: no embedded textures found and solid face colors are disabled"
        )

    clip_ids = select_animation_clips(model, animation)
    per_clip_times: dict[int, list[float]] = {}
    loop_flags: dict[int, bool] = {}
    for ci in clip_ids:
        times, removed = animated_frame_times(model, model.clips[ci], animation_fps)
        if not times:
            raise ImportError(f"clip '{model.clips[ci].name}' produced no sample times")
        per_clip_times[ci] = times
        loop_flags[ci] = removed

    # Bake every clip once on the SOURCE topology for importance, silhouette
    # scoring and worst-pose collapse costs. The shared simplified topology
    # still needs its own frames after simplification.
    poses_by_clip = anim_eval.bake_clips_poses(
        model,
        [(model.clips[ci], per_clip_times[ci]) for ci in clip_ids],
        workers=pose_workers,
    )
    all_poses = [pose for clip_poses in poses_by_clip for pose in clip_poses]
    metric_times = per_clip_times[clip_ids[0]]
    metric_poses = poses_by_clip[0]
    first_clip = model.clips[clip_ids[0]]
    anim_imp = metrics_mod.compute_animation_importance(
        model, first_clip, metric_times, poses=metric_poses
    )
    if len(clip_ids) > 1:
        for ci, poses in zip(clip_ids[1:], poses_by_clip[1:]):
            extra = metrics_mod.compute_animation_importance(
                model, model.clips[ci], per_clip_times[ci], poses=poses
            )
            anim_imp = [max(a, b) for a, b in zip(anim_imp, extra)]
    silhouette_times = metric_times[:: max(1, len(metric_times) // 8)][:8]
    silhouette_poses = [metric_poses[metric_times.index(t)] for t in silhouette_times]
    sil_imp = sil_mod.compute_silhouette_importance(
        model, poses=silhouette_poses,
        n_views=silhouette_views,
    )

    profile = saturn_profile_mod.SaturnProfile()
    if max_pose_stream_bytes < 1:
        raise ImportError("--max-pose-stream-bytes must be positive")
    profile.max_pose_stream_bytes = max_pose_stream_bytes
    if hud_reserve < 0:
        raise ImportError("--hud-reserve must not be negative")
    profile.hud_reserve = hud_reserve
    face_cap = _face_command_cap(
        argparse.Namespace(target="saturn", profile=None, max_triangles=max_triangles,
                           max_vdp1_commands=max_vdp1_commands),
        profile,
    )
    if simplify == "off":
        simp = simp_mod.simplify(
            model, anim_importance=anim_imp, sil_importance=sil_imp,
            options=simp_mod.SimplificationOptions(
                target_triangles=len(model.triangles), quality=quality,
                animation_weight=animation_weight, silhouette_weight=silhouette_weight,
                material_weights=material_weight_map),
            pose_positions=all_poses,
        )
        quality_report = metrics_mod.evaluate_candidate(
            model, simp, first_clip, quality, metric_times, silhouette_views,
            source_poses=metric_poses)
        if not quality_report["passed"]:
            raise ImportError(
                f"full-resolution model fails quality preset {quality!r}: "
                f"{quality_report['failing_gates']}"
            )
    else:
        if simplify == "auto":
            requested = face_cap if face_cap is not None else len(model.triangles)
        else:
            try:
                requested = int(simplify)
            except ValueError:
                raise ImportError(f"--simplify must be off|auto|TARGET (got {simplify!r})")
            if requested < 1:
                raise ImportError(f"--simplify target must be >= 1 (got {simplify!r})")
        try:
            simp, quality_report = metrics_mod.search_upward(
                model, first_clip, requested, quality,
                simp_mod.SimplificationOptions(
                    target_triangles=requested, quality=quality,
                    animation_weight=animation_weight, silhouette_weight=silhouette_weight,
                    material_weights=material_weight_map),
                anim_importance=anim_imp, sil_importance=sil_imp,
                times=metric_times, pose_positions=all_poses,
                source_poses=metric_poses,
                # An explicit numeric --simplify target is a floor: honor the
                # requested density when it passes instead of minimizing away.
                enforce_floor=(simplify != "auto"),
            )
        except gltf_mod.GltfError as exc:
            raise ImportError(str(exc))
    delivered = len(simp.triangles)
    # Merged quads are checked against the command cap once they exist: a
    # pair costs one command, so the triangle count may exceed the cap.
    if face_cap is not None and delivered > face_cap and not merge_quads:
        raise ImportError(saturn_profile_mod.format_hard_failure(
            delivered, face_cap,
            f"smallest {quality}-valid mesh has {delivered} triangles"))

    # Mirror-flip handling matches the OBJ path: odd-axis mirrors toggle
    # the winding reversal so outward normals stay outward.
    if (int(bool(flip_x)) + int(bool(flip_y)) + int(bool(flip_z))) % 2 == 1:
        reverse_winding = not reverse_winding

    def _flip(p):
        x, y, z = p
        return (-x if flip_x else x, -y if flip_y else y, -z if flip_z else z)

    # Bake per-clip pose frames on the shared simplified topology.
    simp_view = metrics_mod.simplified_as_source(simp, model)
    animations: list[dict] = []
    all_frames: list = []
    clip_frames: dict[int, list] = {}
    for ci in clip_ids:
        frames = anim_eval.bake_clip_poses(simp_view, model.clips[ci], per_clip_times[ci])
        clip_frames[ci] = [[_flip(p) for p in frame] for frame in frames]
        all_frames.extend(clip_frames[ci])
    # Runtime vertices: every simplified vertex, or one per point that moves
    # identically in every frame. Faces bake their textures from source UVs
    # at import time, so the Saturn never needs a UV-split copy.
    runtime_of = list(range(len(simp.positions)))
    runtime_src = list(range(len(simp.positions)))
    if weld_vertices:
        runtime_src = []
        seen: dict[tuple, int] = {}
        for vi, p in enumerate(simp.positions):
            key = (tuple(round(c, 7) for c in p),) + tuple(
                tuple(round(c, 7) for c in frame[vi]) for frame in all_frames)
            if key not in seen:
                seen[key] = len(runtime_src)
                runtime_src.append(vi)
            runtime_of[vi] = seen[key]
    weld_report = {"enabled": bool(weld_vertices),
                   "vertices_before": len(simp.positions),
                   "vertices_after": len(runtime_src)}
    for ci in clip_ids:
        clip = model.clips[ci]
        times = per_clip_times[ci]
        frames = clip_frames[ci]
        baked = pose_bake.quantize_frames(
            [[frame[vi] for vi in runtime_src] for frame in frames],
            scale=scale, bbox_diagonal=simp_view.bbox_diagonal() * scale)
        if face_levels:
            baked["shades"] = face_color_mod.bake_shades(
                frames, simp.triangles, simp.tri_materials, face_levels, light,
                clockwise_front=reverse_winding)
            baked["vertex_gouraud"] = face_color_mod.bake_vertex_gouraud(
                frames, simp.triangles, len(simp.positions), light, ambient, diffuse,
                clockwise_front=reverse_winding)
        num, den = rate_fraction(len(times), clip.duration or 1.0) \
            if animation_fps == "source" else (int(float(animation_fps)), 1)
        baked["name"] = clip.name
        baked["sample_rate_num"] = num
        baked["sample_rate_den"] = den
        baked["loop"] = loop_flags[ci]
        baked["frame_times"] = list(times)
        animations.append(baked)

    # Static geometry: bind pose with flips/scale, LibSaturn winding.
    bind = [_flip(p) for p in simp.positions]
    bind = [(x * scale, y * scale, z * scale) for (x, y, z) in bind]
    quad_report = {"enabled": False}
    if merge_quads:
        texel_scales = {}
        for mt, mat in enumerate(model.materials):
            if "texture" in mat:
                tex = model.textures[mat["texture"]]
                texel_scales[mt] = (tex.width * texture_scale, tex.height * texture_scale)
        polys, poly_materials, quad_report = quad_merge_mod.merge_quads(
            simp.triangles, simp.tri_materials, simp.uvs,
            [_flip(p) for p in simp.positions], all_frames, texel_scales,
            quad_merge_mod.QuadMergeOptions(
                max_texel_error=quad_max_texel_error, max_fold_deg=quad_max_fold_deg))
        quads, quad_uvs = _polygon_winding(polys, simp.uvs, reverse_winding)
        face_materials = poly_materials
        delivered = len(quads)
        if face_cap is not None and delivered > face_cap:
            raise ImportError(saturn_profile_mod.format_hard_failure(
                delivered, face_cap,
                f"{len(simp.triangles)} triangles merge to {delivered} faces"))
    else:
        quads, quad_uvs = _glb_winding(simp.triangles, simp.uvs, reverse_winding)
        face_materials = simp.tri_materials
    # Compact bind arrays in simplified order (simp.positions already is).
    vertices_fx = [(float_to_fx16(x), float_to_fx16(y), float_to_fx16(z))
                   for (x, y, z) in (bind[vi] for vi in runtime_src)]
    for i, (x, y, z) in enumerate(vertices_fx):
        if not -(2**31) <= x < 2**31 or not -(2**31) <= y < 2**31 or not -(2**31) <= z < 2**31:
            raise ImportError(f"vertex {i} overflows 16.16 fixed point (reduce --scale)")

    # Canonical face-texture baking from the SIMPLIFIED topology (textured
    # assets only; a solid-color asset's faces are palette shades).
    baked_rgba: list = []
    face_sizes: list[tuple[int, int]] = []
    face_mtls: list[str] = []
    texel_density = None
    if texel_extent is not None:
        # Texels per world unit such that the bind pose's longest extent spans
        # texel_extent texels: a face never bakes more texels than it covers
        # on screen when the model spans that many pixels.
        span = max(max(p[i] for p in bind) - min(p[i] for p in bind) for i in range(3))
        texel_density = texel_extent / max(span, 1e-9)
    for qi, ((a, b, c, d), (ua, ub, uc, ud)) in enumerate(zip(quads, quad_uvs)):
        if face_levels:
            break
        mt = face_materials[qi]
        mat = model.materials[mt] if mt < len(model.materials) else {}
        if "texture" not in mat:
            raise ImportError(
                f"face {qi} uses material {mat.get('name', mt)!r} without a "
                "baseColorTexture (the animated textured path needs a texture "
                "on every material)"
            )
        tex = model.textures[mat["texture"]]
        if merge_quads:
            (a, b, c, d), (ua, ub, uc, ud) = _cheapest_rotation(
                (a, b, c, d), (ua, ub, uc, ud), tex.width, tex.height, texture_scale,
                max_texture_width, max_texture_height)
            quads[qi] = (a, b, c, d)
        est_w, est_h = estimate_face_size((ua, ub, uc, ud), tex.width, tex.height, texture_scale)
        if texel_density is not None:
            pa, pb, pc, pd = (bind[i] for i in (a, b, c, d))
            world_w = max(math.dist(pa, pb), math.dist(pd, pc)) * texel_density
            world_h = max(math.dist(pa, pd), math.dist(pb, pc)) * texel_density
            est_w = max(1, min(est_w, math.ceil(world_w)))
            est_h = max(1, min(est_h, math.ceil(world_h)))
        out_w, out_h = conform_size(est_w, est_h, max_texture_width, max_texture_height,
                                    mat.get("name"), qi)
        baked_rgba.append(bake_face_rgba((ua, ub, uc, ud), tex.width, tex.height,
                                         tex.pixels_rgba, out_w, out_h, sampling))
        face_sizes.append((out_w, out_h))
        face_mtls.append(mat.get("name", str(mt)))

    shade_palette = None
    face_base = None
    has_transparency = False
    luts: list[tuple[int, ...]] = []
    face_luts: list[int] = []
    if face_levels:
        palette_rgb555: list[int] = []
        indexed_faces: list = []
        shade_palette = face_color_mod.shade_palette(base_colors, face_levels, ambient, diffuse)
        face_base = face_color_mod.face_base_shades(simp.tri_materials, face_levels)
    elif texture_format == "lut4":
        if any(a < 128 for face in baked_rgba for (_, _, _, a) in face):
            raise ImportError("--texture-format lut4 needs opaque textures")
        palette_rgb555 = []
        indexed_faces = []
        lut_of: dict[tuple[int, ...], int] = {}
        snap = None
        if lut_codes is not None:
            palette_rgb555, snap = _lut_code_palette(baked_rgba, *lut_codes)
        for rgba, (w, h) in zip(baked_rgba, face_sizes):
            lut, packed = quantize_face_lut4(rgba, w, h)
            if snap is not None:
                lut = [snap(c) for c in lut]
            key = tuple(lut)
            if key not in lut_of:
                lut_of[key] = len(luts)
                luts.append(key)
            face_luts.append(lut_of[key])
            indexed_faces.append(packed)
    else:
        palette_rgb888, has_transparency, _ = build_shared_palette(baked_rgba)
        indexed_faces = map_faces_to_indices(baked_rgba, face_sizes, palette_rgb888)
        palette_rgb555 = []
        for i, (r, g, b) in enumerate(palette_rgb888):
            palette_rgb555.append(0x0000 if (has_transparency and i == 0) else rgb888_to_rgb555(r, g, b))
        while len(palette_rgb555) < 256:
            palette_rgb555.append(0x0000)
        palette_rgb555 = palette_rgb555[:256]
    opaque_flag = 0x0001 if not has_transparency else 0x0000
    if luts:
        opaque_flag |= 0x8000  # SAT_MODEL_TEXTURE_LUT4
    unique: list[dict] = []
    key_to_index: dict[tuple, int] = {}
    face_texture_indices: list[int] = [0xFFFF] * len(quads) if face_levels else []
    for fi, ((w, h), pixels) in enumerate(zip(face_sizes, indexed_faces)):
        slot = face_luts[fi] if luts else 0
        key = (w, h, bytes(pixels), slot, opaque_flag)
        if key in key_to_index:
            face_texture_indices.append(key_to_index[key])
        else:
            idx = len(unique)
            key_to_index[key] = idx
            unique.append({"width": w, "height": h, "pixels": bytes(pixels),
                           "flags": opaque_flag, "pixel_count": w * h,
                           "palette_slot": slot})
            face_texture_indices.append(idx)
    luts_rgb555 = [c for lut in luts for c in lut]

    # Faces baked from simplified-vertex UVs; now point them at runtime ones.
    quads = [tuple(runtime_of[i] for i in q) for q in quads]
    if locality_order:
        quads, face_texture_indices, vertices_fx = _locality_order(
            quads, face_texture_indices, vertices_fx, animations,
            [all_frames[0][vi] for vi in runtime_src])
    static = ImportResult(
        vertices_fx=vertices_fx,
        indices_abcd=quads,
        face_texture_indices=face_texture_indices,
        textures=unique,
        palette_rgb555=palette_rgb555,
        palette_base=palette_index,
        stats={},
        shade_palette_rgb555=shade_palette,
        face_base_shades=face_base,
        luts_rgb555=luts_rgb555,
    )
    # VRAM per texture is its stored bytes; each LUT is another 32.
    texture_sizes = [len(t["pixels"]) for t in unique] + [32] * len(luts)
    indexed_bytes = sum(texture_sizes)
    vram_est = sum(((n + 7) & ~7) for n in texture_sizes)
    largest = max((t["width"] * t["height"], t["width"], t["height"]) for t in unique) if unique else (0, 0, 0)
    shade_bytes = sum(len(a.get("shades") or []) for a in animations)
    gouraud_bytes = sum(len(a.get("vertex_gouraud") or []) for a in animations)
    if face_levels:
        color_report["shade_bytes"] = shade_bytes
        color_report["gouraud_bytes"] = gouraud_bytes + len(face_base or [])
    # Shade and Gouraud streams are per-frame data like poses and share
    # their budget.
    pose_bytes = sum(a["pose_bytes"] for a in animations) + shade_bytes + gouraud_bytes
    resource_report = saturn_profile_mod.check_resources(
        profile, faces=delivered, texture_payload_bytes=indexed_bytes,
        texture_sizes=texture_sizes,
        pose_stream_bytes=pose_bytes)
    if not resource_report["passed"]:
        raise ImportError(saturn_profile_mod.format_hard_failure(
            delivered, face_cap or saturn_profile_mod.face_command_budget(profile),
            "; ".join(resource_report["failing_gates"])))

    lod_reports = None
    if generate_lods:
        budget = face_cap or saturn_profile_mod.face_command_budget(profile)
        specs = lod_mod.default_lod_specs(budget, len(model.triangles))
        lod_reports = []
        for entry in lod_mod.generate_lods(model, first_clip, specs, quality,
                                           anim_imp, sil_imp, metric_times, all_poses,
                                           silhouette_views):
            lod_reports.append({k: v for k, v in entry.items()
                                if k not in ("simplified", "quality")})
            lod_reports[-1]["passed"] = entry["passed"]
            lod_reports[-1]["failing_gates"] = entry["failing_gates"]

    static.stats = {
        "source_vertices": stats["vertices"],
        "source_triangles": stats["triangles"],
        "source_materials": stats["materials"],
        "source_textures": stats["textures"],
        "source_joints": stats["joints"],
        "animation_clips": stats["animation_clips"],
        "selected_clips": [model.clips[ci].name for ci in clip_ids],
        "baked_faces_before_dedup": len(baked_rgba),
        "unique_textures_after_dedup": len(unique),
        "palette_count": 1,
        "indexed_texture_bytes": indexed_bytes,
        "palette_bytes": 512,
        "estimated_vram_usage": vram_est,
        "largest_baked_texture": (largest[1], largest[2]) if unique else (0, 0),
        "has_transparency": has_transparency,
        "scale": scale,
    }
    report = {
        "source": {
            **stats,
            "clip_durations": {model.clips[ci].name: model.clips[ci].duration for ci in clip_ids},
        },
        "simplification": {**simp.report, "quality_preset": quality},
        "quad_merge": quad_report,
        "vertex_weld": weld_report,
        "animation_quality": quality_report,
        "saturn_animation": [
            {
                "clip": a["name"],
                "baked_frames": a["frame_count"],
                "sample_rate_num": a["sample_rate_num"],
                "sample_rate_den": a["sample_rate_den"],
                "loop": a["loop"],
                "duplicate_loop_frame_removed": loop_flags[ci],
                "position_encoding": "int16 scale/bias per axis",
                "pose_stream_bytes": a["pose_bytes"],
                "quantization_max_error": a["max_error"],
                "quantization_mean_error": a["mean_error"],
            }
            for a, ci in zip(animations, clip_ids)
        ],
        "textures": {
            "baked_faces": len(baked_rgba),
            "unique_textures": len(unique),
            "format": texture_format if not face_levels else "none",
            "luts": len(luts),
            "indexed_pixel_bytes": indexed_bytes,
            "palette_bytes": 512,
            "estimated_vram_bytes": vram_est,
        },
        "face_colors": color_report,
        "vdp1": resource_report,
        "lods": lod_reports,
        "result": {"pass": bool(quality_report["passed"] and resource_report["passed"])},
    }
    return AnimatedImportResult(static=static, animations=animations, report=report)


def print_animated_stats(report: dict) -> None:
    src = report["source"]
    simp = report["simplification"]
    print(f"source vertices: {src['vertices']}")
    print(f"source triangles: {src['triangles']}")
    print(f"source joints: {src['joints']}")
    print(f"animation clips: {src['animation_clips']}")
    print(f"requested target: {simp['requested_target']}")
    print(f"delivered triangles: {simp['delivered_triangles']}")
    print(f"delivered vertices: {simp['delivered_vertices']}")
    print(f"reduction: {simp['reduction_percent']}%")
    aq = report["animation_quality"]["surface"]
    print(f"animated surface error: max {aq['max']:.4f} p95 {aq['p95']:.4f} mean {aq['mean']:.4f}")
    for anim in report["saturn_animation"]:
        print(f"clip '{anim['clip']}': {anim['baked_frames']} frames @ "
              f"{anim['sample_rate_num']}/{anim['sample_rate_den']} Hz, loop={anim['loop']}, "
              f"pose bytes {anim['pose_stream_bytes']}, quant err {anim['quantization_max_error']:.6f}")
    colors = report.get("face_colors", {})
    if colors.get("enabled"):
        print(f"face colors: {colors['distinct_colors']} colors x {colors['levels']} light levels "
              f"({colors['non_uniform_faces']} non-uniform faces averaged), "
              f"vertices welded {colors['vertices_before_weld']} -> {colors['vertices_after_weld']}, "
              f"shade bytes {colors['shade_bytes']}, Gouraud bytes {colors['gouraud_bytes']}")
    elif colors.get("mode", "off") != "off":
        print(f"face colors: not used ({colors.get('reason')})")
    quads = report.get("quad_merge", {})
    if quads.get("enabled"):
        print(f"quad merge: {quads['source_triangles']} triangles -> {quads['polygons']} faces "
              f"({quads['merged_quads']} quads, {quads['single_triangles']} triangles)")
    weld = report.get("vertex_weld", {})
    if weld.get("enabled"):
        print(f"vertex weld: {weld['vertices_before']} -> {weld['vertices_after']} runtime vertices")
    print(f"unique textures: {report['textures']['unique_textures']}")
    print(f"texture VRAM estimate: {report['textures']['estimated_vram_bytes']}")
    vdp1 = report["vdp1"]
    print(f"VDP1 commands: model {vdp1['worst_case_model_commands']} + "
          f"reserved {vdp1['reserved_commands']} = {vdp1['total_command_estimate']} "
          f"(headroom {vdp1['command_headroom']})")
    print(f"RESULT: {'PASS' if report['result']['pass'] else 'FAIL'} saturn-vdp1 profile")


