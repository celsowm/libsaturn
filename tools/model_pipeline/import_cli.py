"""Command-line interface and format dispatch for model imports."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import sys
from pathlib import Path
from . import emit_c as emit_anim
from . import gltf as gltf_mod
from . import pose_bake
from .animated_import import import_animated_model, print_animated_stats
from .constants import VDP1_MAX_TEXTURE_HEIGHT, VDP1_MAX_TEXTURE_WIDTH
from .errors import ImportError
from .import_cache import IMPORT_SIGNATURE_SCHEMA, _atomic_write_json, _import_signature, _incremental_cache_hit, _model_output_paths, _write_incremental_manifest
from .static_import import emit_c_h, import_model, print_stats

def main() -> int:
    parser = argparse.ArgumentParser(description="Import textured/animated 3D models for Saturn")
    parser.add_argument("--input", required=True, help="Input OBJ or GLB/GLTF file")
    parser.add_argument("--out-prefix", required=True, help="Output C/H prefix")
    parser.add_argument("--symbol", default=None, help="Generated symbol prefix (default: out-prefix name)")
    parser.add_argument("--scale", type=float, default=1.0, help="Uniform geometry scale")
    parser.add_argument("--flip-x", action="store_true")
    parser.add_argument("--flip-y", action="store_true")
    parser.add_argument("--flip-z", action="store_true")
    parser.add_argument("--reverse-winding", action="store_true",
                        help="Keep source winding instead of converting to LibSaturn clockwise")
    parser.add_argument("--palette-index", type=int, default=0)
    parser.add_argument("--max-texture-width", type=int, default=VDP1_MAX_TEXTURE_WIDTH)
    parser.add_argument("--max-texture-height", type=int, default=VDP1_MAX_TEXTURE_HEIGHT)
    parser.add_argument("--texture-scale", type=float, default=1.0,
                        help="Global baked-texture resolution scale")
    parser.add_argument("--sampling", default="nearest", choices=("nearest", "area"),
                        help="Bake sampling: nearest source texel, or area (box-filtered "
                             "average of the source each baked texel covers)")
    parser.add_argument("--lut-codes", default=None, metavar="LO-HI",
                        help="Animated GLB, lut4: tables hold VDP2 palette codes LO..HI of "
                             "one shared palette instead of RGB (8-bit/pixel hi-res "
                             "framebuffers)")
    parser.add_argument("--texel-extent", type=float, default=None,
                        help="Animated GLB: cap each face's baked texels at its world size, "
                             "scaled so the model's longest extent spans N texels (match "
                             "the on-screen size; pair with --sampling area)")
    parser.add_argument("--target", default=None,
                        help="Compilation target; 'saturn' enables the animated GLB path")
    parser.add_argument("--simplify", default="auto",
                        help="Animated GLB topology: off|auto|TARGET (default auto)")
    parser.add_argument("--quality", default="balanced",
                        help="Quality preset: conservative|balanced|aggressive")
    parser.add_argument("--max-triangles", type=int, default=None)
    parser.add_argument("--max-vdp1-commands", type=int, default=None)
    parser.add_argument("--max-pose-stream-bytes", type=int, default=256 * 1024,
                        help="Maximum baked animation data stored in Saturn RAM (default: 262144)")
    parser.add_argument("--hud-reserve", type=int, default=128,
                        help="VDP1 command budget reserved for this example's HUD (default: 128)")
    parser.add_argument("--animation", default="all",
                        help="Animated clips: all|NAME|INDEX (default all)")
    parser.add_argument("--animation-fps", default="source",
                        help="Runtime clip sampling: source|N fps (default source)")
    parser.add_argument("--merge-rigid-meshes", action="store_true",
                        help="Merge unskinned animated mesh nodes into one synthetic rigid skin")
    parser.add_argument("--generate-lods", action="store_true", default=False)
    parser.add_argument("--silhouette-views", type=int, default=16)
    parser.add_argument("--animation-weight", type=float, default=1.0)
    parser.add_argument("--silhouette-weight", type=float, default=1.0)
    parser.add_argument("--face-colors", default="off",
                        help="Animated GLB faces as solid lit colors instead of textures: "
                             "off|auto|on (default off)")
    parser.add_argument("--light-dir", default="-0.5,0.6,0.8",
                        help="Baked light direction x,y,z in asset space (towards the light)")
    parser.add_argument("--ambient", type=float, default=0.35,
                        help="Baked light floor for --face-colors (linear)")
    parser.add_argument("--diffuse", type=float, default=0.75,
                        help="Baked directional light strength for --face-colors (linear)")
    parser.add_argument("--merge-quads", default="off", choices=("off", "on"),
                        help="Animated GLB: draw adjacent triangle pairs as one VDP1 quad "
                             "when the texture mapping and fold allow it (default off)")
    parser.add_argument("--quad-max-texel-error", type=float, default=1.0,
                        help="Largest baked-texel shift a merged quad may introduce (default 1.0)")
    parser.add_argument("--quad-max-fold-deg", type=float, default=30.0,
                        help="Largest fold between merged triangles in any frame (default 30)")
    parser.add_argument("--locality-order", default="off", choices=("off", "on"),
                        help="Animated GLB, textured: order faces along the longest axis and "
                             "vertices by first use, so a split face list touches split "
                             "vertex ranges (default off)")
    parser.add_argument("--texture-format", default="indexed8", choices=("indexed8", "lut4"),
                        help="Animated GLB textures: indexed8 (one shared 256-color bank) or "
                             "lut4 (4 bits per texel, 15 colors per face in VDP1 lookup "
                             "tables: half the VRAM) (default indexed8)")
    parser.add_argument("--weld-vertices", default="off", choices=("off", "on"),
                        help="Animated GLB, textured: store one runtime vertex per point "
                             "that moves identically in every frame, dropping UV-split "
                             "copies (default off)")
    parser.add_argument("--material-weight", action="append", default=[],
                        help="Animated GLB: NAME=W or INDEX=W simplification cost weight for "
                             "one material; below 1 spends fewer triangles on it (repeatable)")
    parser.add_argument("--split-intersections", action="store_true",
                        help="OBJ static path: split faces that pass through each other so the "
                             "VDP1 painter can order the pieces (no depth buffer)")
    parser.add_argument("--split-face-budget", type=int, default=None,
                        help="Face count the split may grow to (default 4x the input faces); "
                             "exceeding it is an error, never a silent partial split")
    parser.add_argument("--report", default=None, help="JSON report path (animated path)")
    parser.add_argument("--incremental", action="store_true",
                        help="Skip import when inputs, options, tools and outputs match")
    parser.add_argument("--force-import", action="store_true",
                        help="Regenerate even when --incremental finds a cache hit")
    parser.add_argument("--signature-only", action="store_true",
                        help="Update a signature stamp without importing the model")
    parser.add_argument("--signature-file", default=None,
                        help="Signature stamp path used with --signature-only")
    args = parser.parse_args()

    suffix = Path(args.input).suffix.lower()
    if suffix not in (".obj", ".glb", ".gltf"):
        print(f"import_model: error: expected .obj, .glb or .gltf (got {args.input})",
              file=sys.stderr)
        return 1
    if args.signature_only and not args.signature_file:
        print("import_model: error: --signature-only needs --signature-file", file=sys.stderr)
        return 1
    if args.signature_file and not args.signature_only:
        print("import_model: error: --signature-file requires --signature-only", file=sys.stderr)
        return 1
    if args.signature_only and (args.incremental or args.force_import):
        print("import_model: error: --signature-only cannot be combined with import mode flags",
              file=sys.stderr)
        return 1

    signature = None
    if args.signature_only or args.incremental or args.force_import:
        try:
            signature = _import_signature(args)
        except OSError as exc:
            print(f"import_model: error: cannot fingerprint inputs: {exc}", file=sys.stderr)
            return 1
        if args.signature_only:
            stamp = Path(args.signature_file)
            cache_valid = _incremental_cache_hit(args, signature)
            _atomic_write_json(stamp, {
                "schema_version": IMPORT_SIGNATURE_SCHEMA,
                "signature": signature["signature"],
            }, preserve_if_equal=cache_valid)
            if cache_valid:
                output_times = [path.stat().st_mtime_ns
                                for path in _model_output_paths(args).values()]
                if output_times:
                    safe_mtime = max(0, min(output_times) - 2_000_000_000)
                    os.utime(stamp, ns=(safe_mtime, safe_mtime))
            print(f"SIGNATURE: {signature['signature']} "
                  f"({'cache valid' if cache_valid else 'cache miss'})")
            return 0
        if (args.incremental and not args.force_import and
                _incremental_cache_hit(args, signature)):
            paths = _model_output_paths(args)
            print(f"UP-TO-DATE: {paths['source']} + {paths['header']}")
            return 0

    if suffix in (".glb", ".gltf") and args.split_intersections:
        print("import_model: error: --split-intersections applies to the static OBJ path; "
              "animated faces move every frame", file=sys.stderr)
        return 1
    if suffix in (".glb", ".gltf"):
        result_code = _main_animated(args)
        if result_code == 0 and args.incremental and signature is not None:
            _write_incremental_manifest(args, signature)
        return result_code
    if args.simplify != "off" and (args.target == "saturn" or args.quality != "balanced"):
        print("import_model: error: --simplify/--quality apply to the animated GLB path; "
              "OBJ import is not simplified", file=sys.stderr)
        return 1
    try:
        result = import_model(
            obj_path=Path(args.input),
            scale=args.scale,
            flip_x=args.flip_x,
            flip_y=args.flip_y,
            flip_z=args.flip_z,
            reverse_winding=args.reverse_winding,
            palette_index=args.palette_index,
            max_texture_width=args.max_texture_width,
            max_texture_height=args.max_texture_height,
            texture_scale=args.texture_scale,
            sampling=args.sampling,
            split_intersections=args.split_intersections,
            split_face_budget=args.split_face_budget,
        )
        header_path, source_path = emit_c_h(result, Path(args.out_prefix), args.symbol)
    except ImportError as exc:
        print(f"import_model: error: {exc}", file=sys.stderr)
        return 1

    print_stats(result.stats)
    print(f"OK: {header_path} + {source_path}")
    if args.incremental and signature is not None:
        _write_incremental_manifest(args, signature)
    return 0


def _parse_lut_codes(text):
    if text is None:
        return None
    try:
        lo, hi = (int(v) for v in text.split("-"))
    except ValueError:
        raise ImportError(f"--lut-codes must be LO-HI (got {text!r})")
    return lo, hi


def _main_animated(args) -> int:
    if args.target not in (None, "saturn"):
        print(f"import_model: error: unknown --target {args.target!r} (have: saturn)",
              file=sys.stderr)
        return 1
    if args.simplify == "off" and args.generate_lods:
        print("import_model: error: --generate-lods needs simplification enabled",
              file=sys.stderr)
        return 1
    try:
        result = import_animated_model(
            glb_path=Path(args.input),
            scale=args.scale,
            flip_x=args.flip_x,
            flip_y=args.flip_y,
            flip_z=args.flip_z,
            reverse_winding=args.reverse_winding,
            palette_index=args.palette_index,
            max_texture_width=args.max_texture_width,
            max_texture_height=args.max_texture_height,
            texture_scale=args.texture_scale,
            sampling=args.sampling,
            texel_extent=args.texel_extent,
            lut_codes=_parse_lut_codes(args.lut_codes),
            simplify=args.simplify,
            quality=args.quality,
            max_triangles=args.max_triangles,
            max_vdp1_commands=args.max_vdp1_commands,
            max_pose_stream_bytes=args.max_pose_stream_bytes,
            hud_reserve=args.hud_reserve,
            animation=args.animation,
            animation_fps=args.animation_fps,
            merge_rigid_meshes=args.merge_rigid_meshes,
            generate_lods=args.generate_lods,
            silhouette_views=args.silhouette_views,
            animation_weight=args.animation_weight,
            silhouette_weight=args.silhouette_weight,
            face_colors=args.face_colors,
            light_dir=args.light_dir,
            ambient=args.ambient,
            diffuse=args.diffuse,
            merge_quads=args.merge_quads == "on",
            quad_max_texel_error=args.quad_max_texel_error,
            quad_max_fold_deg=args.quad_max_fold_deg,
            material_weights=args.material_weight,
            weld_vertices=args.weld_vertices == "on",
            texture_format=args.texture_format,
            locality_order=args.locality_order == "on",
        )
        header_path, source_path = emit_anim.emit_animated_c_h(
            result.static, result.animations, Path(args.out_prefix), args.symbol)
    except ImportError as exc:
        print(f"import_model: error: {exc}", file=sys.stderr)
        return 1
    except pose_bake.PoseBakeError as exc:
        print(f"import_model: error: {exc}", file=sys.stderr)
        return 1
    except gltf_mod.GltfError as exc:
        print(f"import_model: error: {exc}", file=sys.stderr)
        return 1

    if args.report:
        report_path = Path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        payload = dict(result.report)
        payload["output"] = {"header": str(header_path), "source": str(source_path)}
        c_bytes = source_path.read_bytes()
        h_bytes = header_path.read_bytes()
        payload["output"]["sha256_c"] = hashlib.sha256(c_bytes).hexdigest()
        payload["output"]["sha256_h"] = hashlib.sha256(h_bytes).hexdigest()
        report_path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    print_animated_stats(result.report)
    print(f"OK: {header_path} + {source_path}")
    return 0
